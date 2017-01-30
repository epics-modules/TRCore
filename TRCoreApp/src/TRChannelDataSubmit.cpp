/* This file is part of the Transient Recorder Framework.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the Transient Recorder Framework, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#include <stddef.h>

#include <epicsAssert.h>
#include <epicsGuard.h>
#include <errlog.h>

#include "TRChannelsDriver.h"
#include "TRBaseDriver.h"

#include "TRChannelDataSubmit.h"

bool TRChannelDataSubmit::allocateArray (
    TRBaseDriver &driver, int channel_num, NDDataType_t data_type, int num_samples)
{
    assert(m_array == NULL);
    assert(channel_num >= 0 && channel_num < driver.m_num_channels);
    
    TRChannelsDriver &ch_driver = *driver.m_channels_driver;
    
    // Allocate the NDArray.
    NDArray *array = ch_driver.allocateArray(data_type, num_samples);
    if (array == NULL) {
        errlogSevPrintf(errlogMajor, "TRChannelDataSubmit Error: NDArray allocation failed for channel %d.\n",
            channel_num);
        return false;
    }
    
    // Remember the array and the data pointer.
    m_array = array;
    
    return true;
}

void TRChannelDataSubmit::submit (
    TRBaseDriver &driver, int channel, int unique_id, double timestamp,
    epicsTimeStamp epics_ts, TRArrayCompletionCallback *compl_cb)
{
    assert(channel >= 0 && channel < driver.m_num_channels);
    
    // If there is no array, we don't do anything.
    if (m_array == NULL) {
        return;
    }
    
    // Take the array out of this object.
    NDArray *array = m_array;
    m_array = NULL;
    
    // Set the NDArray metadata fields.
    array->uniqueId = unique_id;
    array->timeStamp = timestamp;
    array->epicsTS = epics_ts;
    
    TRChannelsDriver &ch_driver = *driver.m_channels_driver;
    
    // Sanity check the maxAddr of the channels port.
    assert(ch_driver.maxAddr >= driver.m_num_channels);
    
    bool proceed = true;
    double sample_rate;
    
    {
        epicsGuard<asynPortDriver> lock(driver);
        
        // Check if the main port allows data to be submitted.
        if (!driver.m_allowing_data) {
            proceed = false;
        } else {
            // Get the sample rate from the driver for the attribute.
            sample_rate = driver.m_rate_for_display;
        }
    }
    
    if (proceed) {
        // Pass the array on to the channel driver for the rest of the processing.
        ch_driver.submitArray(array, channel, sample_rate, compl_cb);
    } else {
        array->release();
    }
}
