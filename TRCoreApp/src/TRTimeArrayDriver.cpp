/* This file is part of the Transient Recorder Framework.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the Transient Recorder Framework, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#include <algorithm>
#include <limits>

#include <epicsGuard.h>

#include "TRTimeArrayDriver.h"

TRTimeArrayDriver::TRTimeArrayDriver (std::string const &base_port_name)
:   asynPortDriver(
        (base_port_name + "_time_array").c_str(),
        1, // maxAddr
        NUM_PARAMS,
        asynInt32Mask|asynFloat64Mask|asynFloat64ArrayMask|asynDrvUserMask, // interfaceMask
        asynInt32Mask|asynFloat64Mask|asynFloat64ArrayMask, // interruptMask
        0, // asynFlags (no ASYN_CANBLOCK - we don't block)
        1, // autoConnect
        0, // priority (ignored with no ASYN_CANBLOCK)
        0  // stackSize (ignored witn no ASYN_CANBLOCK)
    ),
    m_unit(0.0),
    m_num_pre(0),
    m_num_post(0)
{
    createParam("ARRAY",  asynParamFloat64Array, &m_params[ARRAY]);
    createParam("UPDATE", asynParamInt32,        &m_params[UPDATE]);
    
    setIntegerParam(m_params[UPDATE], 0);
}

asynStatus TRTimeArrayDriver::readFloat64Array (asynUser *pasynUser, epicsFloat64 *value, size_t nElements, size_t *nIn)
{
    if (pasynUser->reason == m_params[ARRAY]) {
        // Get the current parameters for the time array.
        double unit = m_unit;
        int num_pre = m_num_pre;
        int num_post = m_num_post;
        
        // Sanity check the parameters.
        if (num_pre < 0 || num_post < 0 || num_post > std::numeric_limits<int>::max() - num_pre) {
            *nIn = 0;
            return asynError;
        }
        
        // Calculate the number of elements to write to the array.
        int count = num_pre + num_post;
        if ((unsigned int)count > nElements) {
            count = nElements;
        }
        
        // Write the array.
        for (int i = 0; i < count; i++) {
            value[i] = (i - num_pre) * unit;
        }
        
        // Return the number of elements written and success.
        *nIn = count;
        return asynSuccess;
    }
    
    // Delegate to base class.
    return asynPortDriver::readFloat64Array(pasynUser, value, nElements, nIn);
}

void TRTimeArrayDriver::setTimeArrayParams (double unit, int num_pre, int num_post)
{
    epicsGuard<asynPortDriver> lock(*this);
    
    // Remember the time array parameters.
    m_unit = unit;
    m_num_pre = num_pre;
    m_num_post = num_post;
    
    // Change the UPDATE parameter to trigger EPICS to read the ARRAY.
    int update;
    getIntegerParam(m_params[UPDATE], &update);
    setIntegerParam(m_params[UPDATE], !update);
    callParamCallbacks();
}
