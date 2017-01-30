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
 * Defines the TRChannelDataSubmit class, used for submitting burst data to the framework.
 */

#ifndef TRANSREC_CHANNEL_DATA_SUBMIT_H
#define TRANSREC_CHANNEL_DATA_SUBMIT_H

#include <stddef.h>

#include <epicsTime.h>

#include <NDArray.h>

#include "TRNonCopyable.h"

class TRBaseDriver;
class TRArrayCompletionCallback;

/**
 * Class for submitting burst data.
 * 
 * This object does not have any protection from concurrent use.
 * It is intended to be used within a function on the same thread.
 * Multi-threaded use is possible but external locking is needed in
 * that case.
 */
class TRChannelDataSubmit :
    private TRNonCopyable
{
public:
    /**
     * Constructor for the data-submit object.
     * 
     * The constructor does not have any parameters and @ref allocateArray
     * should be used to allocate the array with the right settings.
     * This allows making an array of these objects.
     * 
     * This class has two major states: without-array (default) and
     * with-array (after a successful @ref allocateArray).
     */
    inline TRChannelDataSubmit ()
    : m_array(NULL)
    {
    }
    
    /**
     * Destructor for the data-submit object.
     * 
     * It ensures that any array is released at destruction.
     */
    inline ~TRChannelDataSubmit ()
    {
        if (m_array != NULL) {
            m_array->release();
        }
    }
    
    /**
     * Set parameters for the array and allocate the NDArray.
     * 
     * This may only be called in the without-array state.
     * Upon success, the state changes to with-array.
     * 
     * This function MUST be called with the base and channels drivers
     * unlocked.
     * 
     * @param driver The TRBaseDriver to which the data would be submitted.
     * @param channel_num Channel number for data. Must be a valid channel
     *                    number for the TRBaseDriver.
     * @param data_type The AreaDetector data type for the array.
     * @param num_samples Number of samples for the burst.
     * @return true on success, false on error
     */
    bool allocateArray (TRBaseDriver &driver, int channel_num, NDDataType_t data_type,
                        int num_samples);
    
    /**
     * Release any array.
     * 
     * After this the state is witout-array.
     */
    inline void releaseArray ()
    {
        if (m_array != NULL) {
            m_array->release();
            m_array = NULL;
        }
    }
    
    /**
     * Returns the data pointer of the array.
     * 
     * This will return NULL if the state is without-array.
     * It is important that the caller checks for NULL before writing data.
     * To write the data, this pointer should be cast to the pointer to
     * type corresponding to the data type of the array as allocated.
     * 
     * This function is not maximally efficient. Avoid calling this in
     * a tight loop and cache the pointer instead.
     * 
     * @return The data pointer of the NDArray, or NULL if there is no array.
     */
    inline void * data ()
    {
        return (m_array == NULL) ? NULL : m_array->pData;
    }
    
    /**
     * Submit the array to AreaDetector.
     * 
     * It is acceptable to call this in the without-array state,
     * notably after a failed @ref allocateArray. After this, the state is
     * without-array (submitting an array implies relasing it).
     * 
     * If called in the with-array state, the driver and channel
     * arguments must be the same as was used in the @ref allocateArray call
     * when the array was allocated.
     * 
     * This function will discard the data if disarming has already
     * been initiated.
     * 
     * This function MUST be called with the base and channels drivers
     * unlocked.
     * 
     * @param driver The TRBaseDriver as was passed to @ref allocateArray.
     * @param channel The channel number as was passed to @ref allocateArray.
     * @param unique_id The unique_id for the NDArray.
     * @param timestamp The timestamp for the NDArray.
     * @param epics_ts The epicsTimeStamp for the NDArray.
     * @param compl_cb Optional callback to be called just before submitting
     *                 the NDArray, after meta-information including timestamps
     *                 and attributes have been set. This allows overriding any
     *                 information in the array and adding additional attributes.
     *                 It is called with the channels port
     *                 (TRChannelsDriver) locked. This callbacks also
     *                 allows inhibiting array submission.
     */
    void submit (TRBaseDriver &driver, int channel, int unique_id,
                 double timestamp, epicsTimeStamp epics_ts,
                 TRArrayCompletionCallback *compl_cb);
    
private:
    // The current NDArray, or NULL if none.
    NDArray *m_array;
};

/**
 * Callback class for final adjustment of the NDArray.
 * 
 * This can be used in TRChannelDataSubmit::submit in order to call
 * a user-defined function before submitting the NDArray.
 */
class TRArrayCompletionCallback {
public:
    /**
     * Callback for final adjustment of the NDArray.
     * 
     * @param array The NDArray about to be submitted.
     * @return True to submit the array, false to inhibit submission.
     */
    virtual bool completeArray (NDArray *array) = 0;
};

#endif
