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
 * Defines the TRBurstMetaInfo class, used for passing information to TRBaseDriver::publishBurstMetaInfo.
 */

#ifndef TRANSREC_BURST_META_INFO_H
#define TRANSREC_BURST_META_INFO_H

#include <math.h>

/**
 * Structure for burst meta-information.
 * 
 * This is intended for passing to TRBaseDriver::publishBurstMetaInfo.
 */
class TRBurstMetaInfo {
public:
    /**
     * Constructor for burst meta-information.
     * 
     * It stores the burst ID and initializes all other variables
     * to predefined invalid values. It is not required to set any
     * other variable.
     * 
     * @param burst_id The burst identifier. This should be one more
     *                 for each subsequent burst and wrap around to zero
     *                 before reaching INT_MAX.
     */
    inline TRBurstMetaInfo (int burst_id)
    : burst_id(burst_id),
      time_burst(NAN),
      time_read(NAN),
      time_process(NAN)
    {
    }

    /**
    * The burst ID (refer to the constructor).
    */
    int burst_id;

    /**
     * The duration of the burst (us).
     */
    double time_burst;

    /**
     * The time it took to read the burst from hardware (us).
     */
    double time_read;

    /**
     * The time it took to process the burst after it was read (us).
     */
    double time_process;
};

#endif
