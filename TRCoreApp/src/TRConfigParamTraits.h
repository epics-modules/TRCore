/* This file is part of the Transient Recorder Framework.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the Transient Recorder Framework, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */
#ifndef TRANSREC_CONFIG_PARAM_TRAITS_H
#define TRANSREC_CONFIG_PARAM_TRAITS_H

#include <asynPortDriver.h>

/**
 * This template class implements type-specific aspects needed
 * for working with Transient Recorder configuration parameters. It is used
 * internally in TRConfigParam.
 */
template <typename ValueType>
struct TRConfigParamTraits {};

template <>
struct TRConfigParamTraits<int> {
    typedef double EffectiveValueType;
    
    static asynParamType const asynType = asynParamInt32;

    inline static void setParam (asynPortDriver *port, int index, int newVal)
    {
        port->setIntegerParam(index, newVal);
    }

    inline static int getParam (asynPortDriver *port, int index)
    {
        int val;
        port->getIntegerParam(index, &val);
        return val;
    }
};

template <>
struct TRConfigParamTraits<double> {
    typedef double EffectiveValueType;
    
    static asynParamType const asynType = asynParamFloat64;

    inline static void setParam (asynPortDriver *port, int index, double newVal)
    {
        port->setDoubleParam(index, newVal);
    }

    inline static double getParam (asynPortDriver *port, int index)
    {
        double val;
        port->getDoubleParam(index, &val);
        return val;
    }
};

#endif
