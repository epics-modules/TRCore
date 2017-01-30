/* This file is part of the Transient Recorder Framework.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the Transient Recorder Framework, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#include <cmath>
#include <string>
#include <algorithm>

#include <epicsThread.h>
#include <epicsGuard.h>
#include <errlog.h>

#include "TRBaseDriver.h"

TRBaseDriver::TRBaseDriver (TRBaseConfig const &cfg)
:
    asynPortDriver(
        cfg.port_name.c_str(),
        1, // maxAddr
        NUM_BASE_ASYN_PARAMS + cfg.num_asyn_params + 2 * (NumBaseConfigParams + cfg.num_config_params),
        asynInt32Mask|asynFloat64Mask|asynDrvUserMask, // interfaceMask
        asynInt32Mask|asynFloat64Mask, // interruptMask
        0, // asynFlags (no ASYN_CANBLOCK - we don't block)
        1, // autoConnect
        0, // priority (ignored with no ASYN_CANBLOCK)
        0  // stackSize (ignored witn no ASYN_CANBLOCK)
    ),
    m_num_channels(cfg.num_channels),
    m_supports_pre_samples(cfg.supports_pre_samples),
    m_init_completed(false),
    m_allowing_data(false),
    m_max_ad_buffers(cfg.max_ad_buffers),
    m_max_ad_memory(cfg.max_ad_memory),
    m_num_config_params(NumBaseConfigParams + cfg.num_config_params),
    m_arm_state(ArmStateDisarm),
    m_rate_for_display(0.0),
    m_time_array_driver(cfg.port_name)
{
    // Reserve space in m_config_params for efficiency.
    m_config_params.reserve(m_num_config_params);
    
    // Create regular asyn parameters.
    createParam("ARM_REQUEST",           asynParamInt32,   &m_asyn_params[ARM_REQUEST]);
    createParam("ARM_STATE",             asynParamInt32,   &m_asyn_params[ARM_STATE]);
    createParam("EFFECTIVE_SAMPLE_RATE", asynParamFloat64, &m_asyn_params[EFFECTIVE_SAMPLE_RATE]);
    createParam("BURST_ID",              asynParamInt32,     &m_asyn_params[BURST_ID]);
    createParam("BURST_TIME_BURST",      asynParamFloat64,   &m_asyn_params[BURST_TIME_BURST]);
    createParam("BURST_TIME_READ",       asynParamFloat64,   &m_asyn_params[BURST_TIME_READ]);
    createParam("BURST_TIME_PROCESS",    asynParamFloat64,   &m_asyn_params[BURST_TIME_PROCESS]);
    createParam("SLEEP_AFTER_BURST",     asynParamFloat64,   &m_asyn_params[SLEEP_AFTER_BURST]);
    
    // Register write-protected parameters.
    addProtectedParam(m_asyn_params[ARM_STATE]);
    addProtectedParam(m_asyn_params[EFFECTIVE_SAMPLE_RATE]);
    addProtectedParam(m_asyn_params[BURST_ID]);
    addProtectedParam(m_asyn_params[BURST_TIME_BURST]);
    addProtectedParam(m_asyn_params[BURST_TIME_READ]);
    addProtectedParam(m_asyn_params[BURST_TIME_PROCESS]);

    // Set initial parameter values.
    setIntegerParam(m_asyn_params[ARM_REQUEST],          ArmStateDisarm);
    setIntegerParam(m_asyn_params[ARM_STATE],            m_arm_state);
    setDoubleParam(m_asyn_params[EFFECTIVE_SAMPLE_RATE], NAN);

    // Initialize configuration parameters
    initConfigParam(m_param_num_bursts,               "NUM_BURSTS",             (double)NAN);
    initConfigParam(m_param_num_post_samples,         "NUM_POST_SAMPLES",       (double)NAN);
    initConfigParam(m_param_num_pre_post_samples,     "NUM_PRE_POST_SAMPLES",   (double)NAN);
    initConfigParam(m_param_requested_sample_rate,    "REQUESTED_SAMPLE_RATE",  (double)NAN);
    initInternalParam(m_param_achievable_sample_rate, "ACHIEVABLE_SAMPLE_RATE", (double)NAN);
    
    // Start the read thread.
    epicsThreadMustCreate(
        (std::string("TRread:") + cfg.port_name).c_str(),
        (unsigned int)cfg.read_thread_prio,
        cfg.read_thread_stack_size>0 ? cfg.read_thread_stack_size : epicsThreadGetStackSize(epicsThreadStackMedium),
        readThreadTrampoline, this);
}

void TRBaseDriver::completeInit ()
{
    assert(!m_init_completed);
    
    TRChannelsDriver *ch_driver = createChannelsDriver();
    assert(ch_driver != NULL);
    
    m_channels_driver.reset(ch_driver);
    m_init_completed = true;
}

double TRBaseDriver::getRequestedSampleRate ()
{
    return m_param_requested_sample_rate.getDesired();
}

void TRBaseDriver::setAchievableSampleRate (double value)
{
    m_param_achievable_sample_rate.setDesired(value);
    callParamCallbacks();
}

void TRBaseDriver::requestDisarmingFromDriver ()
{
    if (m_arm_state != ArmStateDisarm) {
        requestDisarming(ArmStateDisarm);
    }
}

void TRBaseDriver::publishBurstMetaInfo (TRBurstMetaInfo const &info)
{
    epicsGuard<asynPortDriver> lock(*this);
    
    setIntegerParam(m_asyn_params[BURST_ID],          info.burst_id);
    setDoubleParam(m_asyn_params[BURST_TIME_BURST],   info.time_burst);
    setDoubleParam(m_asyn_params[BURST_TIME_READ],    info.time_read);
    setDoubleParam(m_asyn_params[BURST_TIME_PROCESS], info.time_process);
    
    callParamCallbacks();
}

void TRBaseDriver::maybeSleepForTesting ()
{
    double sleep_time;
    {
        epicsGuard<asynPortDriver> lock(*this);
        getDoubleParam(m_asyn_params[SLEEP_AFTER_BURST], &sleep_time);
    }
    
    if (sleep_time > 0) {
        epicsThreadSleep(sleep_time);
    }
}

bool TRBaseDriver::isDisarmed ()
{
    return m_arm_state == ArmStateDisarm;
}

asynStatus TRBaseDriver::writeInt32 (asynUser *pasynUser, int value)
{
    int reason = pasynUser->reason;
    
    // Handle specific parameters.
    if (reason == m_asyn_params[ARM_REQUEST]) {
        // Arm request - update value, handle.
        asynPortDriver::writeInt32(pasynUser, value);
        return handleArmRequest(value);
    }
    
    // Prevent modification of write-protected parameters.
    if (!checkProtectedParamWrite(reason)) {
        return asynError;
    }
    
    // Handle using base class. Either it is a parameter of the base
    // class or it is a simple parameter witn no special write behavior.
	return asynPortDriver::writeInt32(pasynUser, value);
}

asynStatus TRBaseDriver::writeFloat64 (asynUser *pasynUser, epicsFloat64 value)
{
    int reason = pasynUser->reason;
    
    // Handle specific parameters.
    if (reason == m_param_requested_sample_rate.desiredParamIndex()) {
        // Desired sample rate changed - update and inform driver.
        asynPortDriver::writeFloat64(pasynUser, value);
        requestedSampleRateChanged();
        return asynSuccess;
    }
    
    // Prevent modification of write-protected parameters.
    if (!checkProtectedParamWrite(reason)) {
        return asynError;
    }
    
    // Handle using base class. Either it is a parameter of the base
    // class or it is a simple parameter witn no special write behavior.
    return asynPortDriver::writeFloat64(pasynUser, value);
}

void TRBaseDriver::addProtectedParam (int param)
{
    m_protected_params.push_back(param);
}

bool TRBaseDriver::checkProtectedParamWrite (int param)
{
    bool ok = std::find(m_protected_params.begin(), m_protected_params.end(), param)
              == m_protected_params.end();
    if (!ok) {
        errlogSevPrintf(errlogMajor,
            "TRBaseDriver Error: Tried to write write-protected parameter.\n");
    }
    return ok;
}

asynStatus TRBaseDriver::handleArmRequest (int armRequest)
{
    if (armRequest != ArmStateDisarm &&
        armRequest != ArmStatePostTrigger &&
        armRequest != ArmStatePrePostTrigger)
    {
        errlogSevPrintf(errlogMinor, "TRBaseDriver Warning: Invalid arm request.\n");
        return asynError;
    }

    if (m_arm_state == ArmStateDisarm) {
        // We are currently disarmed. If the request is to arm, start the arming.
        // Otherwise the request is to disarm, so no need to do anything.
        if (armRequest != ArmStateDisarm) {
            startArming((ArmState)armRequest);
        }
    } else {
        // We are currently in some state that is not disarmed.
        // Request disarming, and also request rearming if the
        // request is to arm.
        requestDisarming((ArmState)armRequest);
    }

    return asynSuccess;
}

void TRBaseDriver::startArming (ArmState requested_arm_state)
{
    assert(requested_arm_state == ArmStatePostTrigger || requested_arm_state == ArmStatePrePostTrigger);
    assert(m_arm_state == ArmStateDisarm);
    assert(m_init_completed);
    
    // Set the arm state to Busy as we are now arming.
    setArmState(ArmStateBusy);
    
    // Set some state variables.
    m_requested_arm_state = requested_arm_state;
    m_disarm_requested = false;
    m_requested_rearm_state = ArmStateDisarm;
    m_in_read_loop = false;
    
    // Raise the signal to the read thread.
    m_start_arming_event.signal();
}

void TRBaseDriver::requestDisarming (ArmState requested_rearm_state)
{
    assert(m_arm_state != ArmStateDisarm);
    
    // Initiate actions to start disarming, but only the first time
    // that a disarm request is recevied.
    if (!m_disarm_requested) {
        // Set the stop-requested flag so the read thread can see
        // that disarming must be done.
        m_disarm_requested = true;
        
        // Do not allow any more data to be submitted.
        m_allowing_data = false;
        
        // Set the arm state to Busy as we are now disarming.
        setArmState(ArmStateBusy);
        
        // Signal this event. This allows the readThread to wait until
        // disarming is requested.
        m_disarm_requested_event.signal();
        
        // If we are currently in the read loop, call the interruptReading
        // function of the driver. This should make sure that any ongoing
        // readBurst returns quickly and any future readBurst returns
        // immediately, ensuring that the read thread exits the read loop
        // and starts disarming. Note that interruptReading is intentionally
        // called with the lock held and must not block.
        if (m_in_read_loop) {
            interruptReading();
        }
        
        // Note that if reading is not in progress, the read loop will
        // not be entered any more because before entry to the read loop,
        // the readThread checks if disarming was requested.
    }
    
    // Update the requested rearm state, so that we know how
    // to proceed when disarming is complete.
    m_requested_rearm_state = requested_rearm_state;
}

void TRBaseDriver::waitUntilDisarming ()
{
    // Wait until m_disarm_requested_event is signaled.
    m_disarm_requested_event.wait();
    
    // Re-signal the event so multiple waits are possible.
    // This should not matter for the current possible sequences.
    m_disarm_requested_event.signal();
}

void TRBaseDriver::readThreadTrampoline (void *obj)
{
    static_cast<TRBaseDriver *>(obj)->readThread();
}

void TRBaseDriver::readThread ()
{
    // Each iteration of this loop corresponds to one arming.
    while (true) {
        readThreadIteration();
    }
}

void TRBaseDriver::readThreadIteration ()
{
    // Wait for a request for reading/arming to start.
    m_start_arming_event.wait();
    assert(m_arm_state == ArmStateBusy);
    
    // Indicates whether stopAcquisition should be called at the end.
    bool need_stop_acquisition = false;
    
    // We'll only set this to false when we exit the loop normally.
    bool had_error = true;

    {
        lock();
        
        // Wait for preconditfor arming to be satisfied.
        // NOTE: On success this locks the asyn port.
        if (!waitForPreconditions()) {
            unlock();
            goto error;
        }
        
        // Make snapshots of desired configuration parameters.
        processConfigParams(&TRConfigParamBase::setSnapshotToDesired);
        
        // Check basic settings (this already looks at the snapshot values).
        if (!checkBasicSettings()) {
            unlock();
            goto error;
        }
        
        // Check for preconditions, wait for outstanding calculations, etc.
        TRArmInfo arm_info;
        if (!checkSettings(arm_info)) {
            unlock();
            goto error;
        }
        
        // Make sure the driver provided the rate_for_display.
        if (std::isnan(arm_info.rate_for_display)) {
            errlogSevPrintf(errlogMajor, "TRBaseDriver Error: The driver did not provide rate_for_display.");
            unlock();
            goto error;
        }
        
        // Remember the rate for display. This will be also used by
        // TRChannelDataSubmit for the NDArray attributes.
        m_rate_for_display = arm_info.rate_for_display;
        
        // Update effective-value parameters to values used for this arming.
        setEffectiveParams();
        
        // Setup the time array.
        setupTimeArray();
        
        // Reset the arrays in the channels port.
        m_channels_driver->resetArrays();
        
        // This variable is used to limit reading only a specific number of
        // bursts, if desired. A negative value indicates that reading should
        // continue indefinitely until manual disarm. If the value is not
        // negative, it will be decremented with each burst until it reaches 0,
        // then disarming will be done automatically.
        int remainingBursts = m_param_num_bursts.getSnapshot();
        if (remainingBursts == 0) {
            remainingBursts = -1; // we use negative as infinity
        }

        // The overflow flag indicates whether there has been a buffer overflow.
        // It is set to true upon detection of overflow, and back to false after
        // the remaining bursts in the buffer have been read and acquisition has
        // been restarted.
        bool overflow = false;

        // This loop is for overflow recovery.
        while (true) {
            // If disarming has been requested, abort.
            if (m_disarm_requested) {
                unlock();
                goto stopped;
            }
            
            // From this point on we allow data to be submitted.
            // Becsause we don't want to ignore data submitted already
            // before startAcquisition has returned.
            m_allowing_data = true;
            
            unlock();
        
            // We will call stopAcquisition at the end only if we have
            // called startAcquisition (successfully or not).
            need_stop_acquisition = true;
            
            // Call the startAcquisition function of the driver.
            if (!startAcquisition(overflow)) {
                goto error;
            }
            
            lock();
            
            // If disarming has been requested, abort.
            if (m_disarm_requested) {
                unlock();
                goto stopped;
            }
            
            // Set the arm state to armed, unless we are here for overflow recovery.
            if (!overflow) {
                setArmState(m_requested_arm_state);
            }
            
            // Set this flag to indicate we are entering the read loop.
            m_in_read_loop = true;                
            
            unlock();

            // Initialize currentRemBursts to remainingBursts and clear
            // the overflow flag, as we are either here initially or we are
            // recovering from overflow.
            // The counter currentRemBursts is initialized to remainingBursts
            // and will be decremented by one each time a burst is read. It will
            // also be changed in case there is an overflow, so we then read only
            // so many bursts more before recovering.
            // However, remainingBursts is only decremented by one for each burst
            // read and is not affected by overflow handling.
            int currentRemBursts = remainingBursts;
            
            // Clear the overflow flag.
            overflow = false;

            // Main burst reading loop.
            while (currentRemBursts != 0) {
                // Wait for and read a burst of data.
                if (!readBurst()){
                    goto error;
                }
                
                // If disarming has been requested, abort.
                // This check is here intentionally, after reading the burst data
                // but before processing it, so that we do not process the data
                // when we are being disarmed.
                if (checkDisarmRequestedUnlocked()) {
                    goto stopped;
                }
                
                if (!overflow) {
                    // Check for overflow.
                    bool overflow_detected;
                    int num_buffer_bursts;
                    if (!checkOverflow(&overflow_detected, &num_buffer_bursts)){
                        goto error;
                    }

                    if (overflow_detected) {
                        // Starting overflow handling.
                        overflow = true;
                        
                        // The num_buffer_bursts must be positive since it includes the
                        // burst that has just been read.
                        assert(num_buffer_bursts > 0);
                        
                        errlogSevPrintf(errlogMinor,
                            "TRBaseDriver Warning: Buffer overflow, reading up to %d remaining bursts\n",
                            (num_buffer_bursts - 1));
                        
                        // Bump down currentRemainingBursts so that we do not read more than
                        // num_buffer_bursts bursts before restarting.
                        if (currentRemBursts < 0) {
                            currentRemBursts = num_buffer_bursts;
                        } else {
                            currentRemBursts = std::min(currentRemBursts, num_buffer_bursts);
                        }
                    }
                }

                // Process the bust data which was read.
                if (!processBurstData()) {
                    goto error;
                }

                // Decrement burst counters.
                if (currentRemBursts > 0) {
                    currentRemBursts--;
                }
                if (remainingBursts > 0) {
                    remainingBursts--;
                }
                
                // Possibly sleep here if enabled, for testing.
                maybeSleepForTesting();
            }

            // We've come here because currentRemainingBursts==0.
            // If we've read all requested bursts (remainingBursts==0),
            // then we stop normally.
            if (remainingBursts == 0) {
                goto stopped;
            }
            
            // Otherwise we must be here due to a buffer overflow.
            assert(overflow);
            
            errlogSevPrintf(errlogMinor, "TRBaseDriver Warning: Restarting after overflow.");

            lock();
            
            // Clear this flag since we're no longer reading but recovering
            // from overflow.
            m_in_read_loop = false;
        }
        // Unreachable.
    }
    
stopped:
    // We come here if we need to stop normally.
    // Set had_error to false since it was initialized to true.
    had_error = false;
    
error:
    // We come here if there is an unexpected error.
    // The had_error is already true.

    lock();
    
    // Clear this flag to indicate we are no longer in the reading loop,
    // since we may have jumped here from within the reading loop.
    m_in_read_loop = false;
    
    // If there was an error and disarming was not requested, we want to
    // report the error via the state and delay disarming until it is
    // requested.
    if (had_error && !m_disarm_requested) {
        // Set the arm state to error to make the error visible.
        setArmState(ArmStateError);
        
        // Wait until disarming is requested.
        unlock();
        waitUntilDisarming();
        lock();
    }
    
    // Do not allow any more data to be submitted.
    m_allowing_data = false;
    
    // Call the stopAcquisition function of the driver if needed.
    if (need_stop_acquisition) {
        unlock();
        stopAcquisition();
        lock();
    }
    
    // Reset the effective-value parameters to invalid values.
    clearEffectiveParams();
    
    // Clear this event since it may have been signaled but not waited.
    m_disarm_requested_event.tryWait();
    
    // Note, m_start_arming_event need not be cleared since it could
    // not have been signaled again before we set ArmStateDisarm here.
    
    // If rearming is needed, we need to start another arm sequence.
    // Otherwise we need to finish in ArmStateDisarm.
    if (m_requested_rearm_state != ArmStateDisarm) {
        // Set m_arm_state to ArmStateDisarm because of an assert in
        // startArming. We don't use setArmState because then the transition
        // would be visible externally in the asyn parameter.
        m_arm_state = ArmStateDisarm;
        
        // Rearming has been requested, so start another arming.
        startArming(m_requested_rearm_state);
    } else {
        // We're done, set the arm state to disarmed.
        setArmState(ArmStateDisarm);
    }
    
    unlock();
}

void TRBaseDriver::setEffectiveParams ()
{
    // Set the EFFECTIVE_SAMPLE_RATE parameter.
    setDoubleParam(m_asyn_params[EFFECTIVE_SAMPLE_RATE], m_rate_for_display);

    // Set the effective parameter values to the snapshot values.
    processConfigParams(&TRConfigParamBase::setEffectiveToSnapshot);

    callParamCallbacks();
}

void TRBaseDriver::clearEffectiveParams ()
{
    // Reset the EFFECTIVE_SAMPLE_RATE parameter.
    setDoubleParam(m_asyn_params[EFFECTIVE_SAMPLE_RATE], NAN);
    
    // Reset the effective values of configuration parameters.
    processConfigParams(&TRConfigParamBase::setEffectiveToInvalid);
    
    callParamCallbacks();
}

void TRBaseDriver::processConfigParams (void (TRConfigParamBase::*func) ())
{
    typedef std::vector<TRConfigParamBase *>::iterator IterType;

    for (IterType it = m_config_params.begin(); it != m_config_params.end(); ++it) {
        ((*it)->*func)();
    }
}

void TRBaseDriver::setArmState (ArmState armState)
{
    m_arm_state = armState;
    setIntegerParam(m_asyn_params[ARM_STATE], armState);
    callParamCallbacks();
}

bool TRBaseDriver::checkDisarmRequestedUnlocked ()
{
    lock();
    bool stop_requested = m_disarm_requested;
    unlock();
    return stop_requested;
}

bool TRBaseDriver::checkBasicSettings ()
{
    // Sanity check NUM_BURSTS.
    int num_bursts = m_param_num_bursts.getSnapshot();
    if (num_bursts < 0) {
        errlogSevPrintf(errlogMajor, "TRBaseDriver Error: NUM_BURSTS is negative.\n");
        return false;
    }
    
    // Sanity check NUM_POST_SAMPLES.
    int num_post_samples = m_param_num_post_samples.getSnapshot();
    if (num_post_samples <= 0) {
        errlogSevPrintf(errlogMajor, "TRBaseDriver Error: NUM_POST_SAMPLES is not positive.\n");
        return false;
    }
    
    if (m_requested_arm_state == ArmStatePrePostTrigger) {
        // Sanity check post samples.
        if (!m_supports_pre_samples) {
            errlogSevPrintf(errlogMajor,
                "TRBaseDriver Error: prePostTrigger requested but pre-samples not supported.\n");
            return false;
        }
        int num_pre_post_samples = m_param_num_pre_post_samples.getSnapshot();
        if (num_pre_post_samples <= num_post_samples) {
            errlogSevPrintf(errlogMajor,
                "TRBaseDriver Error: NUM_PRE_POST_SAMPLES is not greater than NUM_POST_SAMPLES.\n");
            return false;
        }
    } else {
        // Set NUM_PRE_POST_SAMPLES as irrelevant since pre-samples are not used.
        // Also set its snapshot value to zero so that getNumPostSamplesSnapshot
        // will return zero.
        m_param_num_pre_post_samples.setIrrelevant();
        m_param_num_pre_post_samples.setSnapshot(0);
    }
    
    return true;
}

void TRBaseDriver::setupTimeArray ()
{
    // The unit for the time array is the reciprocal of the rate.
    double unit = 1.0 / m_rate_for_display;
    
    // Get the settings for the number of samples.
    int num_post = m_param_num_post_samples.getSnapshot();
    int num_pre_post = m_param_num_pre_post_samples.getSnapshot();
    
    // Calculate the number of pre-samples.
    // Note the calculation is valid due to checkBasicSettings.
    int num_pre = (num_pre_post == 0) ? 0 : (num_pre_post - num_post);
    
    // Set the the time array parameters.
    m_time_array_driver.setTimeArrayParams(unit, num_pre, num_post);
}

TRChannelsDriver * TRBaseDriver::createChannelsDriver ()
{
    // Default implementation: construct a non-derived TRChannelsDriver.
    return new TRChannelsDriver(TRChannelsDriverConfig(*this));
}

void TRBaseDriver::requestedSampleRateChanged ()
{
    // Default implementation: copy the requested rate to the achievable.
    double sample_rate = getRequestedSampleRate();
    setAchievableSampleRate(sample_rate);
}

bool TRBaseDriver::waitForPreconditions ()
{
    // Default implementation for drivers which do not need any
    // synchronization at this point.
    return true;
}

bool TRBaseDriver::readBurst ()
{
    // Default implementation for drivers which do not use our read loop.
    // We wait until disarming is requested then return. Since the event
    // can raised only from requestDisarming and the readThread will check
    // for disarming just after calling readBurst(), it is implied that
    // after we return here the readThread will not continue with
    // checkOverflow/processBurstData but with stopAcquisition.
    waitUntilDisarming();
    return true;
}

bool TRBaseDriver::checkOverflow (bool *had_overflow, int *num_buffer_bursts)
{
    // Default implementation.
    *had_overflow = false;
    (void)num_buffer_bursts;
    return true;
}

bool TRBaseDriver::processBurstData ()
{
    // Default implementation for drivers which do not use our read loop.
    return false;
}

void TRBaseDriver::interruptReading ()
{
    // Default implementation for drivers which do not use our read loop.
    // We do not need to do anything because m_disarm_requested_event
    // has just been signaled, which will cause readBurst to return.
}
