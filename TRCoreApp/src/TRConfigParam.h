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
 * Defines the TRConfigParam class, representing an acquisition configuration parameter.
 */

#ifndef TRANSREC_CONFIG_PARAM_H
#define TRANSREC_CONFIG_PARAM_H

#include <epicsAssert.h>

#include <asynPortDriver.h>

#include "TRNonCopyable.h"
#include "TRConfigParamTraits.h"

class TRBaseDriver;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/**
 * This is a private base class of TRConfigParam with
 * virtual functions necessary for TRBaseDriver to work
 * with parameters of any type.
 */
class TRConfigParamBase
{
    friend class TRBaseDriver;
    
private:
    virtual void setSnapshotToDesired () = 0;
    virtual void setEffectiveToSnapshot () = 0;
    virtual void setEffectiveToInvalid () = 0;
};

#endif

/**
 * Transient Recorder configuration parameter.
 * 
 * A configuration parameter includes two asyn parameters, one for the
 * desired value and one for the effective value. At the start of arming,
 * a snapshot of the desired value is taken and the driver layer will
 * only be able to access this snapshot, which is guaranteed to not
 * change until the next arming.
 * 
 * The ValueType template parameter is the data type of the desired
 * parameter and the snapshot value. The EffectiveValueType template
 * parameter is the data type of the effective-value parameter.
 * The only supported combinations of pairs are:
 * - int, int
 * - int, double (this allows using NAN as the invalid-value)
 * - double, double
 */
template <typename ValueType, typename EffectiveValueType = ValueType>
class TRConfigParam :
    private TRConfigParamBase,
    private TRNonCopyable
{
    friend class TRBaseDriver;
    
    typedef TRConfigParamTraits<ValueType> Traits;
    typedef TRConfigParamTraits<EffectiveValueType> EffectiveTraits;

private:
    bool m_initialized;
    bool m_internal;
    bool m_irrelevant;
    int m_desired_param;
    int m_effective_param;
    ValueType m_snapshot_value;
    EffectiveValueType m_invalid_value;
    TRBaseDriver *m_driver;
    
public:
    /**
     * Default constructor for configuration parameters.
     * 
     * The parameter should be initialized using TRBaseDriver::initConfigParam
     * or TRBaseDriver::initInternalParam after it is constructed. Public
     * functions of TRConfigParam MUST NOT be called before the parameter
     * has been initialized.
     */
    inline TRConfigParam ()
    : m_initialized(false)
    {
    }
    
    /**
     * Return the current snapshot value of the parameter.
     * 
     * This function MUST NOT be called before TRBaseDriver::checkSettings is
     * called or after TRBaseDriver::stopAcquisition returns (until the next
     * checkSettings call). Refer to TRBaseDriver documentation for details.
     * 
     * @return The current snapshot value.
     */
    inline ValueType getSnapshot ()
    {
        assert(m_initialized);
        
        return m_snapshot_value;
    }
    
    /**
     * Like @ref getSnapshot but does not have any asserts.
     * 
     * This is recommended when performance is important and especially
     * in interrupt context.
     * 
     * @return The current snapshot value.
     */
    inline ValueType getSnapshotFast ()
    {
        return m_snapshot_value;
    }
    
    /**
     * Mark this parameter as irrelevant for the current configuration.
     * 
     * The intended use of this is in TRBaseDriver::checkSettings to inform
     * the framework that this configuration parameter is irrelevant, so that
     * the effective value asyn parameter will be set to the invalid value
     * instead of the snapshot value.
     * 
     * This function MUST NOT be called outside of TRBaseDriver::checkSettings.
     */
    inline void setIrrelevant ()
    {
        assert(m_initialized);
        
        m_irrelevant = true;
    }
    
    /**
     * Adjust the snapshot value for the current configuration.
     * 
     * There are two reasons why this would be used:
     * - To report a different effective value of the parameter than
     *   the desired value, if for whatever reason the desired value
     *   is not what is actually being used.
     * - To communicate the desired settings to derived classes. For example,
     *   TRBaseDriver uses this internally so that
     *   TRBaseDriver::getNumPostSamplesSnapshot returns 0 when pre-samples
     *   are not enabled. Driver classes may use the same approach to communicate
     *   settings to further derived classes.
     * 
     * This function MUST NOT be called outside of TRBaseDriver::checkSettings.
     * 
     * @param value The new snapshot value.
     */
    inline void setSnapshot (ValueType value)
    {
        assert(m_initialized);
        
        m_snapshot_value = value;
    }
    
    /**
     * Return the current desired value of the parameter.
     * 
     * The desired value is controlled by the external interface (EPICS)
     * or alteratively by the driver itself for internal parameters
     * using setDesired.
     * 
     * This function MUST be called with the TRBaseDriver port locked.
     * 
     * Using this function should not be needed for most parameters
     * because TRBaseDriver will make a snapshot of the desired value
     * at the start of arming and the snapshot value is what is meant
     * to be used by the driver. It should be used only when the driver
     * calculates the values of internal configuration parameters based
     * on this parameter.
     * 
     * @return The current desired value.
     */
    ValueType getDesired ();
    
    /**
     * Set the desired value of an internal parameter.
     * 
     * This function MUST be called with the TRBaseDriver port locked.
     * 
     * This function MUST NOT be called for configuration parameters
     * that were not initialized using TRBaseDriver::initInternalParam.
     * 
     * @param value The new desired value.
     */
    void setDesired (ValueType value);
    
    /**
     * Return the asyn parameter index of the desired-value parameter.
     * 
     * This can be used in overridden parameter write functions to determine when the
     * desired value is being changed. There is currently only one foreseen use case,
     * when the driver wishes to apply configuration changes immediately while not
     * armed. See @ref TRBaseDriver::onDisarmed for instructions to do this correctly.
     * 
     * @return The asyn parameter index of the desired-value parameter.
     */
    inline int desiredParamIndex ()
    {
        assert(m_initialized);
        
        return m_desired_param;
    }
    
private:
    void init (TRBaseDriver *driver, char const *base_name, EffectiveValueType invalid_value, bool internal);
    void setEffectiveParam (EffectiveValueType value);
    
private:
    // Implementations of TRConfigParamBase virtual functions
    void setSnapshotToDesired ();
    void setEffectiveToSnapshot ();
    void setEffectiveToInvalid ();
};

#endif
