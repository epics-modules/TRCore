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
 * Defines the TRChannelsDriver class, used for submitting data into AreaDetector.
 */

#ifndef TRANSREC_BASE_CHANNELS_DRIVER_H
#define TRANSREC_BASE_CHANNELS_DRIVER_H

#include <stddef.h>

#include <string>

#include <asynNDArrayDriver.h>

#include "TRNonCopyable.h"

class TRBaseDriver;
class TRChannelsDriver;
class TRChannelDataSubmit;
class TRArrayCompletionCallback;

/**
 * Construction parameters for TRChannelsDriver.
 */
class TRChannelsDriverConfig {
    friend class TRChannelsDriver;
    
public:
    /**
     * Constructor for channel driver parameters.
     * 
     * This is meant to be used in TRBaseDriver::createChannelsDriver to
     * construct the channels port (TRChannelsDriver derived class), if
     * the driver overrides createChannelsDriver.
     * 
     * @param base_driver The TRBaseDriver for which the channels driver
     *                    will be constructed.
     */
    inline TRChannelsDriverConfig (TRBaseDriver &base_driver)
    : num_extra_addrs(0),
      num_asyn_params(0),
      base_driver(base_driver)
    {
    }
    
    /**
     * Number of additional asyn addresses to support.
     * 
     * The addresses in the channels driver will be first one address
     * for each channel, then this many additional addresses. This
     * allows the driver to implement additional data sources.
     */
    int num_extra_addrs;
    
    /**
     * Number of asyn parameters defined by the derived class.
     */
    int num_asyn_params;
    
    /**
     * Helper for setting parameters allowing chaining.
     * 
     * Example:
     * \code
     * TRChannelsDriverConfig(base_driver)
     *  .set(&TRChannelsDriverConfig::num_extra_addrs, 1)
     *  .set(&TRChannelsDriverConfig::num_asyn_params, MY_NUM_PARAMS)
     * \endcode
     * 
     * @param param Pointer to member variable to set.
     * @param value Value to set the variable to.
     * @return *this
     */
    template <typename ParamType>
    inline TRChannelsDriverConfig & set (ParamType TRChannelsDriverConfig::*param, ParamType const &value)
    {
        this->*param = value;
        return *this;
    }
    
private:
    TRBaseDriver &base_driver;
};

/**
 * An asynNDArrayDriver-based class though which burst
 * data is submitted into the AreaDetector framework.
 * 
 * The same class is used for submitting data for all channels,
 * using multiple asyn addresses.
 * 
 * This class is created automatically be the framework within
 * TRBaseDriver::completeInit. Drivers do not necessarily have
 * to concern themselves with this class because interaction with
 * AreaDetector is meant to be simplified through the use of the
 * class TRChannelDataSubmit.
 * 
 * However, drivers are allowed to define their own class
 * derived from TRChannelsDriver. In this case, the driver
 * will need to override the virtual function
 * TRBaseDriver::createChannelsDriver. Doing so allows the
 * driver to define channel-specific asyn parameters.
 */
class TRChannelsDriver : public asynNDArrayDriver,
    private TRNonCopyable
{
    friend class TRBaseDriver;
    friend class TRChannelDataSubmit;
    
private:
    // Enumeration of asyn parameters.
    enum Params {
        UPDATE_ARRAYS,
        NUM_CHANNEL_ASYN_PARAMS
    };
    
public:
    /**
     * Constructor for the channel driver.
     * 
     * @param cfg Construction parameters.
     */
    TRChannelsDriver (TRChannelsDriverConfig const &cfg);
    
    virtual ~TRChannelsDriver ();
    
private:
    // The follwing functions are for internal use by Transient Recorder framework.
    
    // Clear all arrays from pArrays (called during arming).
    void resetArrays ();
    
    // Allocate an NDArray for later submission.
    NDArray * allocateArray (NDDataType_t data_type, int num_samples);
    
    // Submit an NDArray to the port.
    void submitArray (NDArray *array, int channel, double sample_rate,
                      TRArrayCompletionCallback *compl_cb);
    
private:
    // Array of asyn parameter indices.
    int m_asyn_params[NUM_CHANNEL_ASYN_PARAMS];
};

#endif
