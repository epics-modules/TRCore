/* This file is part of the Transient Recorder Framework.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the Transient Recorder Framework, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#ifndef TRANSREC_TIME_ARRAY_DRIVER_H
#define TRANSREC_TIME_ARRAY_DRIVER_H

#include <stddef.h>

#include <string>

#include <epicsTypes.h>

#include <asynPortDriver.h>

#include "TRNonCopyable.h"

class TRBaseDriver;

class TRTimeArrayDriver : public asynPortDriver,
    private TRNonCopyable
{
    friend class TRBaseDriver;
    
    // Enumeration of asyn parameters.
    enum Params {
        ARRAY,
        UPDATE,
        NUM_PARAMS
    };
    
private:
    int m_params[NUM_PARAMS];
    double m_unit;
    int m_num_pre;
    int m_num_post;

public:
    TRTimeArrayDriver (std::string const &base_port_name);
    
    virtual asynStatus readFloat64Array (asynUser *pasynUser, epicsFloat64 *value, size_t nElements, size_t *nIn);
    
private:
    // The follwing functions are for internal use by Transient Recorder framework.
    
    // Set the parameters for the time array and poke the UPDATE parameter.
    void setTimeArrayParams (double unit, int num_pre, int num_post);
};

#endif
