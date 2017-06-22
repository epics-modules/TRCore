/* This file is part of the Transient Recorder Framework.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the Transient Recorder Framework, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

/**
 * @file
 * 
 * Defines the TRBaseDriver class, the central class of the framework.
 */

#ifndef TRANSREC_BASE_DRIVER_H
#define TRANSREC_BASE_DRIVER_H

#include <stddef.h>

#include <vector>

#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsTime.h>
#include <epicsMemory.h>

#include <asynPortDriver.h>

#include "TRArmInfo.h"
#include "TRBaseConfig.h"
#include "TRBurstMetaInfo.h"
#include "TRChannelDataSubmit.h"
#include "TRChannelsDriver.h"
#include "TRConfigParam.h"
#include "TRNonCopyable.h"
#include "TRTimeArrayDriver.h"

/**
 * Central class of the Transient Recorder framework for transient recorders (digitizers).
 * 
 * Digitizer drivers use this class by inheriting it, constructing it with
 * the appropriate parameters and implementing various virtual functions.
 * 
 * The major features provided by this class are:
 * - Management of arming and disarming.
 * - Generic implementation of a burst reading/processing loop
 *   (optional).
 * - Management of configuration parameters with automatic
 *   capture of a configuration snapshot at the start of arming.
 * - Abstraction of submitting data into the AreaDetector system.
 * - Infrastructure for configuration of the sample rate and
 *   implementation of the time array (for graphs).
 */
class TRBaseDriver : public asynPortDriver,
    private TRNonCopyable
{
    template <typename, typename>
    friend class TRConfigParam;
    
    friend class TRChannelsDriver;
    friend class TRChannelDataSubmit;

public:
    /**
     * Constructor for TRBaseDriver, to be used from constructors
     * of derived classes.
     * 
     * The function @ref completeInit MUST be called just after the entire object
     * is constructed.
     * 
     * @param cfg Parameters for construction as a struct. See
     *            TRChannelDataSubmit for an example of how to initialize
     *            the parameters in an expression.
     */
    TRBaseDriver (TRBaseConfig const &cfg);
    
    /**
     * Initialize a configuration parameter.
     * 
     * This should be called in the constructor of derived classes for all
     * configuration parameters (ConfigParam) defined by the driver, excluding
     * internal configuration parameters (those should be initialized using
     * @ref initInternalParam).
     * 
     * @param param The configuration parameter to be initialized.
     * @param base_name The base name of the configuration parameter. The prefixes
     *                  DESIRED_ and EFFECTIVE_ will be added  to form the asyn
     *                  parameter names.
     * @param invalid_value The invalid value for the effective-value parameter,
     *                      which is its value when the device is not armed or
     *                      the parameter is irrelevant due to other configuration.
     */
    template <typename ValueType, typename EffectiveValueType>
    inline void initConfigParam (TRConfigParam<ValueType, EffectiveValueType> &param,
        char const *base_name, EffectiveValueType invalid_value = EffectiveValueType())
    {
        param.init(this, base_name, invalid_value, false);
    }
    
    /**
     * Initialize an internal configration parameter.
     * 
     * An internal parameter differs from a regular configuration parameter
     * in that its desired value is not set though the external (EPICS)
     * interface but by the driver, using TRConfigParam::setDesired.
     * 
     * @param param The configuration parameter to be initialized.
     * @param base_name The base name of the configuration parameter. The prefixes
     *                  DESIRED_ and EFFECTIVE_ will be added  to form the asyn
     *                  parameter names.
     * @param invalid_value The invalid value for the effective-value parameter,
     *                      which is its value when the device is not armed or
     *                      the parameter is irrelevant due to other configuration.
     */
    template <typename ValueType, typename EffectiveValueType>
    inline void initInternalParam (TRConfigParam<ValueType, EffectiveValueType> &param,
        char const *base_name, EffectiveValueType invalid_value = EffectiveValueType())
    {
        param.init(this, base_name, invalid_value, true);
    }
    
    /**
     * Complete initialization of the class.
     * 
     * This MUST be called immediately after the entire object is constructed
     * including derived objects. It performs initialization which could not
     * be performed in the constructor.
     * 
     * Notably, it creates the channels driver, which involves calling the
     * virtual function createChannelsDriver.
     */
    void completeInit ();
    
    /**
     * Return a reference to the channels driver.
     * 
     * This MUST NOT be called before @ref completeInit is completed,
     * as the channels driver is constructed in completeInit.
     * 
     * @return Reference to the channels driver.
     */
    inline TRChannelsDriver & getChannelsDriver ()
    {
        return *m_channels_driver;
    }
    
    /**
     * Set the name of the digitizer, which will appear as the value
     * of the "name" PV.
     * 
     * The default name is the asyn port name.
     * 
     * This function MUST be called with the port locked, except if
     * called before EPICS could interact with the asyn port such as
     * from the driver's constructor.
     * 
     * @param name Digitizer name (pointer is not used after return).
     *             Must not be NULL.
     */
    void setDigitizerName (char const *name);
    
    /**
     * Returns the requested sample rate.
     * 
     * Note that it is allowed for the driver to use special sample
     * rate values such as negative ones, e.g. to use an external clock
     * source. But otherwise the value should be in Hz.
     * 
     * This function MUST be called with the port locked.
     * 
     * @return The requested sample rate.
     */
    double getRequestedSampleRate ();
    
    /**
     * Sets the achievable sample rate corresponding to the requested
     * sample rate.
     * 
     * As in @ref getRequestedSampleRate, it is allowed for the driver to use
     * special sample rate values. But otherwise the value should be in Hz.
     * 
     * This function MUST be called with the port locked.
     * 
     * @param value The achievable sample rate.
     */
    void setAchievableSampleRate (double value);
    
    /**
     * Returns the snapshot value of the number of bursts to capture.
     * 
     * This will be either positive (for a specific number of bursts)
     * or 0 (for an unlimited number of bursts).
     * 
     * This function is meant to be used by drivers which do not use the
     * framework's read loop so that they can initiate disarming when the
     * requested number of bursts have been captured.
     * 
     * See @ref checkSettings for limitations regarding reading snapshot values.
     * 
     * @return The snapshot number of bursts.
     */
    inline int getNumBurstsSnapshot ()
    {
        return m_param_num_bursts.getSnapshot();
    }
    
    /**
     * Returns the snapshot value of the number of post-trigger samples per event.
     * 
     * This will be non-negative. If the driver does not declare support
     * for pre-trigger samples (@ref TRBaseConfig::supports_pre_samples is false),
     * then this is also guaranteed to be positive. Otherwise, see
     * @ref getNumPrePostSamplesSnapshot for additional guarantees.
     * 
     * See @ref checkSettings for limitations regarding reading snapshot values.
     * 
     * @return The snapshot number of post-samples.
     */
    inline int getNumPostSamplesSnapshot ()
    {
        return m_param_num_post_samples.getSnapshot();
    }
    
    /**
     * Returns the snapshot value for the total number of samples per event
     * (counting pre-trigger and post-trigger samples).
     * 
     * This will always be positive and will be greater than or equal to
     * @ref getNumPostSamplesSnapshot. Equality means that no pre-trigger samples
     * are desired.
     * 
     * See @ref checkSettings for limitations regarding reading snapshot values.
     * 
     * @return The snapshot number of pre-post-samples.
     */
    inline int getNumPrePostSamplesSnapshot ()
    {
        return m_param_num_pre_post_samples.getSnapshot();
    }
    
    /**
     * Returns the snapshot value of the requested sample rate.
     * 
     * This will be the value of @ref getRequestedSampleRate at the time the
     * snapshot was made.
     * 
     * See @ref checkSettings for limitations regarding reading snapshot values.
     * 
     * @return The snapshot requested sample rate.
     */
    inline double getRequestedSampleRateSnapshot ()
    {
        return m_param_requested_sample_rate.getSnapshot();
    }
    
    /**
     * Returns the snapshot value of the achievable sample rate.
     * 
     * This will be the last value set using @ref setAchievableSampleRate at the
     * time the snapshot was made.
     * 
     * See @ref checkSettings for limitations regarding reading snapshot values.
     * 
     * @return The snapshot achievable sample rate.
     */
    inline double getAchievableSampleRateSnapshot ()
    {
        return m_param_achievable_sample_rate.getSnapshot();
    }
    
    /**
     * Request disarming of acquisition.
     * 
     * This allows the driver itself to initiate disarming as if
     * a disarm request was received. If acquisition is currently
     * disarmed, nothing will be done.
     * 
     * This function MUST be called with the port lock held.
     * 
     * This function may sychronously call the @ref interruptReading driver
     * function. That must be considered when the driver has overridden
     * interruptReading (that should be the case in any driver that
     * uses the framework's read loop).
     * 
     * There are two use cases for this:
     * - When the driver does not use the framework's read loop
     *   (leaves default implementations of @ref readBurst, @ref processBurstData,
     *   @ref checkOverflow and @ref interruptReading), in order to indicate
     *   to the framework that the requested number of bursts have
     *   been read.
     * - To indicate to the framework that a driver-specific condition
     *   to automatically disarm has been detected (whether or not the
     *   driver uses the framework's read loop).
     * 
     * If the driver uses the read loop and does not support any
     * driver-specific conditions to automatically disarm, there is no
     * reason to call this; the framework will itself automatically
     * disarm after the desired number of bursts have been read.
     */
    void requestDisarmingFromDriver ();
    
    /**
     * Publish meta-information about a burst.
     * 
     * This function should be called after data for a burst has been
     * submitted (using TRChannelDataSubmit). If the driver uses the
     * framework's read loop, this should be from the function
     * @ref processBurstData.
     * 
     * The driver should call `asynPortDriver::updateTimeStamp` on this
     * driver (not the channels driver) for each burst before calling
     * this function. This is because the framework will report the time
     * of the last burst based on the current timestamp in the driver.
     * It is expected that most drivers will use the timestamp obtained
     * by updateTimeStamp as the timestamp of channel data arrays of
     * all channels involved in the burst (the epics_ts argument to
     * TRChannelDataSubmit::submit).
     * 
     * This function MUST be called with the port unlocked.
     * 
     * @param info Meta-information about the burst.
     */
    void publishBurstMetaInfo (TRBurstMetaInfo const &info);
    
    /**
     * Possibly sleep for testing if enabled.
     * 
     * The framework implements a feature to optionally sleep after
     * reading a burst, for testing purposes, especially testing of
     * buffer overflow handling .
     * 
     * If the driver uses the framework's read loop, this function is
     * automatically called after (successful) @ref processBurstData.
     * Otherwise, the driver may call this at the appropriate time to
     * gain the capability to sleep after reading a burst (or generally
     * wherever it wishes).
     * 
     * This function MUST be called with the port unlocked.
     */
    void maybeSleepForTesting ();
    
    /**
     * Check if acquisition is currently armed.
     * 
     * For the purposes of this function, acquisition becomes armed when
     * waitForPreconditions is started and becomes not armed after
     * stopAcquisition returns, or if there was an error before
     * startAcquisition then immediately after the error.
     * 
     * This function MUST be called with the port locked.
     * 
     * @return True if disarmed, false if not.
     */
    bool isArmed ();
    
protected:
    /**
     * Create the channels driver.
     * 
     * This function allows the driver to use its own derived class of
     * TRChannelsDriver for the channels port.
     * 
     * If this function is overridden, it MUST create an instance (using new)
     * of a TRChannelsDriver derived class. If the driver does not override
     * this function, the default implementation will create an instance of
     * TRChannelsDriver.
     * 
     * One reason the driver may want to use a custom TRChannelsDriver derived
     * class is to add asyn parameters to it. However, note that this is only
     * needed when the parameter needs to be per-channel, otherwise the parameter
     * would be better suited to the main class (derived class of TRBaseDriver).
     * 
     * @return The new channels driver, allocated using new.
     */
    virtual TRChannelsDriver * createChannelsDriver ();
    
    /**
     * Reports that the requested sample rate has changed.
     * 
     * The driver should recalculate the achievable sample rate
     * (@ref setAchievableSampleRate) based on the requested sample rate
     * (@ref getRequestedSampleRate). That can be done either synchronously in this
     * function (if the calculation is not demanding) or asynchronously. In the
     * latter case, it is important that @ref waitForPreconditions waits until any ongoing
     * clock calculation is completed.
     * 
     * This is called with the port locked and it MUST NOT unlock it at any point.
     * 
     * The default implementation calls getRequestedSampleRate then calls
     * setAchievableSampleRate with that value. Be careful because this is
     * probably not currect for most drivers, as the hardware usually allows
     * only a discrete set of sample rates.
     */
    virtual void requestedSampleRateChanged ();
    
    /**
     * Wait for preconditions for arming to be satisifed.
     * 
     * This is called at the very beginning of arming.
     * If this function returns false, arming will not proceed and an error will
     * be reported. In that case the framework will wait until disarming is
     * requested and only then allow another arming attempt.
     * 
     * This function is called with the port locked and MUST return with the
     * port locked. It MAY internally unlock and re-lock the port (actually
     * it must do that while waiting for anything).
     * 
     * In case of successful return, the framework will make snapshots of desired
     * configuration parameters while the port is still locked. Normally arming will
     * then proceed with @ref checkSettings. However, there is no guarantee that
     * a successful waitForPreconditions call will be followed by checkSettings;
     * arming may be aborted without notice at this stage.
     * 
     * The default implementation only returns true.
     * 
     * @return True on success (port locked, proceed with arming),
     *         false on error (port not locked, abort arming).
     */
    virtual bool waitForPreconditions ();
    
    /**
     * Check preconditions for arming and report the sample freq rate for display.
     * 
     * This is called just after a successful @ref waitForPreconditions.
     * 
     * If this function returns false, arming will not proceed and an error will
     * be reported. In that case the framework will wait until disarming is
     * requested and only then allow another arming attempt.
     * 
     * If this function returns true, it must fill in arm_info as appropriate.
     * Note that setting TRArmInfo::rate_for_display is mandatory in this case.
     * 
     * This is called with the port locked and it MUST NOT unlock it at any point.
     * 
     * There is no guarantee that a successful checkSettings will be followed
     * by @ref startAcquisition, arming may be aborted without notice at this stage.
     * 
     * Starting with this function, the driver is allowed to read the parameter
     * snapshot values using TRConfigParam::getSnapshot as well as snapshot values
     * provided by the framework (get*Snapshot functions). The snapshot values were
     * captured just after waitForPreconditions returned successfully. Snapshot values
     * may be read and are guaranteed to not change until the driver returns from
     * stopAcquisition.
     * 
     * NOTE: Snapshot values MUST NOT be read before checkSettings is called or
     * after @ref stopAcquisition returns (until the next checkSettings call).
     * 
     * @param arm_info The driver must set the relevant variables of this class
     *                 if it returns true. Note that some of the variables are
     *                 mandatory.
     * @return True on success (proceed with arming), false on error (abort arming).
     */
    virtual bool checkSettings (TRArmInfo &arm_info) = 0;
    
    /**
     * Configure the hardware to start acquisition.
     * 
     * This can be called in two scenarios. The normal scenario is just after
     * @ref checkSettings returned true (overflow==false). The other scenario is
     * as part of recovery from buffer overflow (overflow==true).
     * 
     * This function should return false in case of unexpected errors. In that
     * case an error will be reported, the framework will wait until disarming
     * is requested, and only then call @ref stopAcquisition and allow another
     * arming attempt.
     * 
     * This is called with the port unlocked and MUST return unlocked. It MAY
     * internally lock and unlock the port.
     * 
     * @param overflow Whether this the initial call during for arming
     *                 (overflow==false) or a subsequent call after a buffer
     *                 overflow (overflow==true).
     * @return True on success (proceed with the read loop), false on error
     *         (abort arming).
     */
    virtual bool startAcquisition (bool overflow) = 0;

    /**
     * This is called in a loop to wait for and read a burst of data.
     * 
     * If the driver does not wish to use the framework's implementation
     * of the read loop:
     * 
     * - It MUST NOT override @ref readBurst and @ref interruptReading
     *   and should not override @ref checkOverflow and @ref processBurstData
     *   (the latter two would never be called then).
     * - It should submit data (using TRChannelDataSubmit) from
     *   its own threads.
     * 
     * This function should read one burst of data from the hardware buffers.
     * Processing and submitting of data should be done in @ref processBurstData
     * which will normally follow @ref readBurst (but may not in case of disarming).
     * 
     * This function should return false in case of unexpected errors. In that
     * case an error will be reported, the framework will wait until disarming
     * is requested, and only then call @ref stopAcquisition and allow another
     * arming attempt.
     * 
     * This is called with the port unlocked and MUST return unlocked. It MAY
     * internally lock and unlock the port.
     * 
     * @return True on success of if aborted due to @ref interruptReading, false
     *         on error (stop reading).
     */
    virtual bool readBurst ();

    /**
     * Check if there has been a buffer overlow.
     * 
     * If the driver does not wish to use the framework's implementation
     * of the read loop, it should not override this function (it would not
     * be called).
     * 
     * This is called after every successful @ref readBurst when a buffer overflow
     * has not yet occurred. It should set *had_overflow to whether an overflow
     * has occurred. If it sets *had_overflow to true, it should also set
     * *num_buffer_bursts to the remaining number of bursts that can be read
     * before restarting PLUS ONE (i.e. including the burst just read). Typically,
     * *num_buffer_bursts would be set to the number of bursts which fit into
     * the hardware buffer. But be careful because hardware may claim to have some
     * power-of-two buffer size but really have one fewer usable capacity.
     * 
     * This function should return false in case of unexpected errors. In that
     * case it is not necessary to set *had_overflow or *num_buffer_bursts, an
     * error will be reported, the framework will wait until disarming is requested,
     * and only then call @ref stopAcquisition and allow another arming attempt.
     * 
     * If an overflow is detected, the driver may also perform any actions
     * necessary to allow reading of the remaining data to proceed without
     * problems.
     * 
     * This is called with the port unlocked and MUST return unlocked. It MAY
     * internally lock and unlock the port.
     * 
     * The default implementation sets *had_overflow to false and returns true.
     * 
     * @param had_overflow On success, this should be set to whether a buffer
     *                     overflow has occurred.
     * @param num_buffer_bursts On success and if a buffer overflow has occurred,
     *                          this MUST be set to the remaining number of bursts
     *                          that can be read PLUS ONE.
     * @return True on success, false on error (stop reading).
     */
    virtual bool checkOverflow (bool *had_overflow, int *num_buffer_bursts);
    
    /**
     * Process the read that has just been read by readBurst.
     * 
     * If the driver does not wish to use the framework's implementation
     * of the read loop, it should not override this function (it would not
     * be called).
     * 
     * This is normally called after each successful @ref readBurst call
     * (but may not be in exceptional cases like disarming or errors).
     * From within this function, the driver should submit burst data using
     * TRChannelDataSubmit objects and (preferably after that) call
     * @ref publishBurstMetaInfo.
     * 
     * This function should return false in case of unexpected errors. In that
     * case an error will be reported, the framework will wait until disarming
     * is requested, and only then call @ref stopAcquisition and allow another
     * arming attempt.
     * 
     * This is called with the port unlocked and MUST return unlocked. It MAY
     * internally lock and unlock the port.
     * 
     * @return True on success, false on error (stop reading).
     */
    virtual bool processBurstData ();
    
    /**
     * Interrupt reading of data.
     * 
     * If the driver does not wish to use the framework's implementation
     * of the read loop, MUST NOT override this function.
     * 
     * This will only be called while the read thread is in the core of the
     * read loop, that is after a successful @ref startAcquisition but before
     * @ref stopAcquisition, and also only in between two startAcquisition in
     * case of buffer overflows. It will also be called no more than once in
     * the entire arming sequence.
     * 
     * Calling this must ensure that any ongoing or future @ref readBurst call
     * returns as soon as possible and that any future readBurst call
     * returns immediately. Note that readBurst must not return an error
     * (false) due to this interruption; readBurst does not need to report
     * to the caller whether it returned due to interruption or because a
     * burst was read.
     * 
     * This is called with the port locked and it MUST NOT unlock it at any point.
     * It MUST NOT block. If synchronous actions are needed to ensure that
     * reading is interrupted reliably, they MUST be done on another thread.
     */
    virtual void interruptReading ();
    
    /**
     * Configure the hardware to stop acquisition.
     * 
     * This is called after a call to @ref startAcquisition, and is to be understood
     * as the reverse of that. It is called after any @ref readBurst, @ref checkOverflow
     * or @ref processBurstData calls have returned.
     * 
     * After this returns, the hardware will be considered disarmed and a new
     * arming may later start, beginning with @ref waitForPreconditions. There is no
     * way to return an error here as the framework could not reasonably react
     * to an error.
     * 
     * This is called with the port unlocked and MUST return unlocked. It MAY
     * internally lock and unlock the port.
     */
    virtual void stopAcquisition () = 0;
    
    /**
     * Called when @ref isArmed() changes from true to false.
     * 
     * This is called with the port locked and it MUST NOT unlock it.
     * 
     * This was added to support a design where changing a desired configuration
     * parameter value should actually apply the change immediately unless the digitizer
     * is armed, but if the value is changed while armed it should still be applied
     * automatically when the digitizer is disarmed. To implement this correctly
     * you should:
     * - Override the write function corresponding to the type of the type of the
     *   parameter (@ref writeInt32 or @ref writeFloat64). There, before checking
     *   whether the parameter is owned by one of the base classes, check if the
     *   parameter index is equal to @ref TRConfigParam::desiredParamIndex().
     *   If it is equal then...
     * - Call @ref isArmed() and if it returned false, apply the configuration to hardware.
     *   You should use the value parameter of the function since
     *   @ref TRConfigParam::getDesired was not updated yet at this point.
     *   You should still call and return the result of the base class write function
     *   regardless of isArmed.
     * - Implement onDisarmed and in that function ensure that the value returned by
     *   @ref TRConfigParam::getDesired is applied to the hardware. You can use a dirty
     *   flag to avoid having to access the hardware redundantly.
     * 
     * Do not instead use @ref stopAcquisition for this or similar purposes because of
     * possible race conditions and since stopAcquisition may not be called in case of
     * an early error.
     * 
     * The default implementation does nothing.
     */
    virtual void onDisarmed ();
    
public:
    /**
     * Overridden asyn parameter write handler.
     * 
     * It is important that derived classes which override this function
     * delegate to the base class method when the parameter is not from the
     * derived class, including when the parameter belongs to a TRConfigParam
     * defined by the derived class.
     * 
     * Care is needed because a comparison of the parameter index to the index
     * of the first own parameter of the derived class may not yield the expected
     * result. If such a check is used, then the dervived class must initialize
     * all its TRConfigParam before its normal asyn parameters. This way, the
     * asyn parameters belonging to TRConfigParam will receive lower indices than
     * normal asyn parameters of the derived class.
     * 
     * @param pasynUser Asyn user object.
     * @param value Value to be written.
     * @return Operation result.
     */
    virtual asynStatus writeInt32 (asynUser *pasynUser, epicsInt32 value);
    
    /**
     * Overridden asyn parameter write handler.
     * 
     * Refer to the documentation of @ref writeInt32.
     * 
     * @param pasynUser Asyn user object.
     * @param value Value to be written.
     * @return Operation result.
     */
    virtual asynStatus writeFloat64 (asynUser *pasynUser, epicsFloat64 value);
    
private:
    // Enumeration of asyn parameters of this class, excluding
    // those managed by TRConfigParam.
    enum Params {
        ARM_REQUEST,
        ARM_STATE,
        EFFECTIVE_SAMPLE_RATE,
        BURST_ID,
        BURST_TIME_BURST,
        BURST_TIME_READ,
        BURST_TIME_PROCESS,
        SLEEP_AFTER_BURST,
        DIGITIZER_NAME,
        TIME_ARRAY_UNIT_INV,
        NUM_BASE_ASYN_PARAMS
    };

    // Possible arm states
    enum ArmState {
        ArmStateDisarm,
        ArmStatePostTrigger,
        ArmStatePrePostTrigger,
        ArmStateBusy,
        ArmStateError
    };
    
private:
    // Number of channels supported (as passed to constructor).
    int m_num_channels;
    
    // Whether the driver supports pre-samples.
    bool m_supports_pre_samples;
    
    // Whether copies of submitted NDArrays are kept in the TRChannelsDriver
    // (initial value only used by TRChannelsDriver constructor).
    bool m_update_arrays;

    // Flag whether completeInit has been called.
    bool m_init_completed;
    
    // Whether we allow data to be submitted.
    bool m_allowing_data;
    
    // Parameters remembered for the TRChannelsDriver constructor.
    int m_max_ad_buffers;
    size_t m_max_ad_memory;
    
    // Total number of configuration parameters.
    int m_num_config_params;

    // Array of framework-managed asyn params' indices
    int m_asyn_params[NUM_BASE_ASYN_PARAMS];

    // Every config parameter is added to this list.
    std::vector<TRConfigParamBase *> m_config_params;
    
    // List of asyn parameter IDs which are not allowed to be modified.
    std::vector<int> m_protected_params;

    // Base configuration perameters.
    // NOTE: Update NumBaseConfigParams below on every change!
    // There doesn't seem to be a nice way to avoid having to manually
    // maintain the parameter count, so long as the parameters are
    // defined like this as members of the class.
    TRConfigParam<int, double> m_param_num_bursts;
    TRConfigParam<int, double> m_param_num_post_samples;
    TRConfigParam<int, double> m_param_num_pre_post_samples;
    TRConfigParam<double>      m_param_requested_sample_rate;
    TRConfigParam<double>      m_param_achievable_sample_rate;
    static int const NumBaseConfigParams = 5;
    
    // Arm state maintained internally.
    ArmState m_arm_state;
    
    // A simplified armed state useful for drivers.
    bool m_armed;
    
    // When arming, the requested arm state (ArmStatePostTrigger or ArmStatePrePostTrigger).
    ArmState m_requested_arm_state;

    // This flag is set to true when disarming is requested.
    bool m_disarm_requested;
    
    // This flag indicates whether a new arming has been requested
    // during an existing arming, that is we need to re-start arming
    // after disarming.
    ArmState m_requested_rearm_state;
    
    // This flag indicates whether we are inside the read loop.
    bool m_in_read_loop;
    
    // Sample rate for display (time array and NDArrray attributes).
    // This is set during arming to the value provided by checkSettings.
    double m_rate_for_display;
    
    // This event is raised from handleArmRequest to the
    // read_thread in order to start the arming.
    epicsEvent m_start_arming_event;
    
    // This event is raised from handleArmRequest to the
    // read_thread to allow disarming to proceed after an error,
    // and also to allow the default readBurst to wait until
    // disarming is requested.
    epicsEvent m_disarm_requested_event;
    
    // AreaDetector port for the channel data.
    epics_auto_ptr<TRChannelsDriver> m_channels_driver;
    
    // Asyn port for the time array.
    TRTimeArrayDriver m_time_array_driver;

private:
    // Add this parameter index to m_protected_params.
    void addProtectedParam (int param);
    
    // Cheks is a parameter is allowed to be written and prints an error if not.
    bool checkProtectedParamWrite (int param);
    
    // Called from writeInt32 when ARM_REQUEST is being written.
    asynStatus handleArmRequest (int armRequest);
    
    // Starts arming, must be currently locked and in ArmStateDisarm.
    void startArming (ArmState requested_arm_state);
    
    // Requests disarming, must be currently locked and not in ArmStateDisarm.
    void requestDisarming (ArmState requested_rearm_state);
    
    // Waits until disarming is reqested.
    void waitUntilDisarming ();
    
    // Read thread.
    static void readThreadTrampoline (void *obj);
    void readThread ();
    
    // One iteration of the read thread (one arming and disarming).
    void readThreadIteration ();
    
    // Set effective-value parameters, during arming.
    void setEffectiveParams ();
    
    // Clear effective-value parameters, during disarming.
    void clearEffectiveParams ();
    
    // Calls the given function on all configuration parameters.
    void processConfigParams (void (TRConfigParamBase::*func) ());
    
    // Sets m_arm_state and updates the asyn parameter.
    void setArmState (ArmState armState);
    
    // Reads the m_stop_requested, locking and unlocking the port.
    bool checkDisarmRequestedUnlocked ();
    
    // Check basic settings before proceeding with arming.
    bool checkBasicSettings ();
    
    // Check values provided by checkSettings in TRArmInfo.
    bool checkArmInfo (TRArmInfo const &arm_info);
    
    // Sets up the time array based on snapshot settings and m_rate_for_display.
    void setupTimeArray (TRArmInfo const &arm_info);
};

#endif
