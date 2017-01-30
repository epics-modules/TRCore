/* This file is part of the Transient Recorder Framework.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the Transient Recorder Framework, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#include <stddef.h>

#include <string>

#include <epicsAssert.h>
#include <epicsGuard.h>

#include "TRBaseDriver.h"
#include "TRChannelsDriver.h"
#include "TRChannelDataSubmit.h"

TRChannelsDriver::TRChannelsDriver (TRChannelsDriverConfig const &cfg)
:   asynNDArrayDriver(
        (std::string(cfg.base_driver.portName) + "_channels").c_str(),
        cfg.base_driver.m_num_channels + cfg.num_extra_addrs,
        NUM_CHANNEL_ASYN_PARAMS + cfg.num_asyn_params,
        cfg.base_driver.m_max_ad_buffers,
        cfg.base_driver.m_max_ad_memory,
        asynGenericPointerMask|asynDrvUserMask, // interfaceMask
        asynGenericPointerMask, // interruptMask
        ASYN_MULTIDEVICE, // asynFlags (no ASYN_CANBLOCK - we don't block)
        1, // autoConnect
        0, // priority (ignored with no ASYN_CANBLOCK)
        0  // stackSize (ignored witn no ASYN_CANBLOCK)
    )
{
    // Create asyn parameters.
    createParam("UPDATE_ARRAYS", asynParamInt32, &m_asyn_params[UPDATE_ARRAYS]);
    
    int num_channels = cfg.base_driver.m_num_channels;
    for (int channel = 0; channel < num_channels; channel++) {
        // Enable callbacks by default.
        setIntegerParam(channel, NDArrayCallbacks, 1);
        
        // Enable pArrays updates by default.
        setIntegerParam(channel, m_asyn_params[UPDATE_ARRAYS], 1);
    }
}

TRChannelsDriver::~TRChannelsDriver ()
{
}

void TRChannelsDriver::resetArrays ()
{
    epicsGuard<asynPortDriver> lock(*this);
    
    int num_channels = maxAddr;
    for (int channel = 0; channel < num_channels; channel++) {
        if (pArrays[channel] != NULL) {
            pArrays[channel]->release();
            pArrays[channel] = NULL;
        }
    }
}

NDArray * TRChannelsDriver::allocateArray (NDDataType_t data_type, int num_samples)
{
    epicsGuard<asynPortDriver> lock(*this);
    
    size_t dims[1] = {(size_t)num_samples};
    return pNDArrayPool->alloc(1, dims, data_type, 0, NULL);
}

void TRChannelsDriver::submitArray (
    NDArray *array, int channel, double sample_rate, TRArrayCompletionCallback *compl_cb)
{
    assert(array != NULL);
    assert(channel < maxAddr);
    
    // NOTE: We are given a reference to the array, so do not use
    // premature "return" which would leak the array!
    
    int arrayCallbacks;
    bool submit = true;
    
    {
        epicsGuard<asynPortDriver> lock(*this);
        
        // Check if array callbacks are enabled.
        getIntegerParam(channel, NDArrayCallbacks, &arrayCallbacks);
        
        // Check if pArrays updates are enabled.
        int update_parrays;
        getIntegerParam(channel, m_asyn_params[UPDATE_ARRAYS], &update_parrays);
        
        // Call getAttributes of the channels port to fill the attributes.
        getAttributes(array->pAttributeList);
        
        // Add the sample rate attribute.
        array->pAttributeList->add("READ_SAMPLE_RATE", "sample rate", NDAttrFloat64, (void *)&sample_rate);
        
        // Call the array completion callback if given.
        if (compl_cb != NULL) {
            submit = compl_cb->completeArray(array);
        }
        
        if (submit && update_parrays) {
            // Increment the reference count of the array since we will
            // be putting it into pArrays.
            array->reserve();
            
            // Update the NDArray for this channel in the channels port.
            if (pArrays[channel] != NULL) {
                pArrays[channel]->release();
            }
            pArrays[channel] = array;
        }
    }
    
    // Call the array callback if enabled.
    if (submit && arrayCallbacks) {
        doCallbacksGenericPointer(array, NDArrayData, channel);
    }
    
    // Release our reference to the array.
    array->release();
}
