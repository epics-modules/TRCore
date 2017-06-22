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
 * Defines the TRArmInfo class, used for passing information to the framework during arming.
 */

#ifndef TRANSREC_ARM_INFO_H
#define TRANSREC_ARM_INFO_H

#include <cmath>

#include "TRNonCopyable.h"

class TRBaseDriver;

/**
 * Information provided by the driver to the framework at the start of arming.
 * 
 * An instance of this is passed to the driver in TRBaseDriver::checkSettings
 * where the driver should fill in the relevant data.
 */
class TRArmInfo :
    private TRNonCopyable
{
    friend class TRBaseDriver;
    
private:
    inline TRArmInfo ()
    : rate_for_display(NAN),
      custom_time_array_calc_inputs(false),
      custom_time_array_num_pre_samples(0),
      custom_time_array_num_post_samples(0)
    {
    }
    
public:
    /**
     * The driver MUST set this to the display sample rate.
     * 
     * Notably, this will used for computing the time array.
     */
    double rate_for_display;
    
    /**
     * Enable custom sample number inputs for calculation of the time array.
     * 
     * If this is true, than @ref custom_time_array_num_pre_samples
     * and @ref custom_time_array_num_post_samples will be used to calculate
     * the time array instead of the desired sample number settings.
     * The default is false.
     */
    bool custom_time_array_calc_inputs;
    
    /**
     * Custom number of pre-trigger samples for the time array.
     * 
     * The default is zero, and this is only relevant if
     * @ref custom_time_array_calc_inputs is true.
     */
    int custom_time_array_num_pre_samples;
    
    /**
     * Custom number of post-trigger samples for the time array.
     * 
     * The default is zero, and this is only relevant if
     * @ref custom_time_array_calc_inputs is true.
     */
    int custom_time_array_num_post_samples;
};

#endif
