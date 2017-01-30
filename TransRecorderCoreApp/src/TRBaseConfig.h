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
 * Defines the TRBaseConfig class, used for configuring TRBaseDriver.
 */

#ifndef TRANSREC_BASE_CONFIG_H
#define TRANSREC_BASE_CONFIG_H

#include <stddef.h>

#include <string>

/**
 * Construction parameters for TRBaseDriver.
 * 
 * This class is used as a parameter to the @ref TRBaseDriver::TRBaseDriver
 * constructor.
 */
class TRBaseConfig {
public:
    /**
     * Constructor which sets default values.
     * 
     * This is designed with backward compatibility in mind, so that new
     * parameters can be added to the framework which are initialized to
     * default values.
     */
    inline TRBaseConfig ()
    : num_channels(0),
      num_config_params(0),
      num_asyn_params(0),
      read_thread_prio(0),
      read_thread_stack_size(0),
      max_ad_buffers(0),
      max_ad_memory(0),
      supports_pre_samples(false)
    {
    }
    
    /**
     * Name for the base asyn port.
     * 
     * The names of associated ports (channels, time array) will
     * use this as a prefix. This parameter is mandatory.
     */
    std::string port_name;
    
    /**
     * Number of channels supported.
     * 
     * This parameter is mandatory and must be positive.
     */
    int num_channels;
    
    /**
     * Number of configuration parameters (TRConfigParam)
     * defined by the derived class.
     * 
     * This MUST be greater than or equal to the number of TRConfigParam
     * that will be initiaized by the derived class.
     */
    int num_config_params;
    
    /**
     * Number of asyn parameters defined by the derived class.
     * 
     * This MUST be greater than or equal to the number of asyn parameters
     * that will be initiaized by the derived class (not including asyn
     * parameters which the framework will initialize for TRConfigParam).
     */
    int num_asyn_params;
    
    /**
     * Priority for the read thread, in EPICS units.
     * 
     * The default is 0.
     */
    int read_thread_prio;
    
    /**
     * Stack size for the read thread.
     * 
     * If left at the default value 0, the stack size will be
     * epicsThreadGetStackSize(epicsThreadStackMedium).
     */
    int read_thread_stack_size;
    
    /**
     * Maximum number of allocated NDArrays of the channels port.
     * 
     * This is just forwarded to the epicsNDArrayDriver constructor.
     */
    int max_ad_buffers;
    
    /**
     * Maximum memory used by NDArrays of the channels port.
     * 
     * This is just forwarded to the epicsNDArrayDriver constructor.
     */
    size_t max_ad_memory;
    
    /**
     * Whether the driver supports samples before the trigger event.
     * 
     * The default is false.
     */
    bool supports_pre_samples;
    
    /**
     * Helper function for setting parameters using chaining.
     * 
     * Example:
     * \code
     * TRBaseConfig()
     *  .set(&TRBaseConfig::port_name, std::string("foo"))
     *  .set(&TRBaseConfig::num_channels, 5)
     *  .set(&TRBaseConfig::num_config_params, NUM_DRIVER_CONFIG_PARAMS)
     * \endcode
     * 
     * @param param Pointer to member variable to set.
     * @param value Value to set the variable to.
     * @return *this
     */
    template <typename ParamType>
    inline TRBaseConfig & set (ParamType TRBaseConfig::*param, ParamType const &value)
    {
        this->*param = value;
        return *this;
    }
};

#endif
