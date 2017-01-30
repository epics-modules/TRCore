/* This file is part of the Transient Recorder Framework.
 * It is subject to the license terms in the LICENSE.txt file found in the
 * top-level directory of this distribution and at
 * https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html. No part
 * of the Transient Recorder Framework, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms
 * contained in the LICENSE.txt file.
 */

#include <string>

#include <epicsAssert.h>
#include <cantProceed.h>

#include "TRConfigParam.h"
#include "TRBaseDriver.h"

template <typename ValueType, typename EffectiveValueType>
void TRConfigParam<ValueType, EffectiveValueType>::init (
    TRBaseDriver *driver, char const *base_name, EffectiveValueType invalid_value, bool internal)
{
    assert(!m_initialized);
    assert(driver->m_config_params.size() < driver->m_num_config_params);

    // Remember initialization info.
    m_initialized = true;
    m_internal = internal;
    m_driver = driver;
    m_invalid_value = invalid_value;
    
    // Set the initial snapshot value to 0.
    // There is not need to support initializing to any specific default
    // value because the snapshot value it is not supposed to be read
    // before setSnapshotToDesired is called.
    m_snapshot_value = 0;
    m_irrelevant = true;

    asynStatus status;
    std::string param_name;
    
    // Create the asyn parameter for the desired value.
    param_name = std::string("DESIRED_") + base_name;
    status = m_driver->createParam(param_name.c_str(), Traits::asynType, &m_desired_param);
    if (status != asynSuccess){
        cantProceed("createParam failed");
    }
    
    // Create the asyn parameter for the effective value.
    param_name = std::string("EFFECTIVE_") + base_name;
    status = m_driver->createParam(param_name.c_str(), EffectiveTraits::asynType, &m_effective_param);
    if (status != asynSuccess){
        cantProceed("createParam failed");
    }
    
    // Leave the desired parameter undefined. If we initialized,
    // then EPICS DB records would not be able to initialize it
    // using PINI+VAL, because the VAL would be overwritten with
    // out value at record initialization.
    
    // Set the effective value parameter to the invalid value.
    setEffectiveParam(m_invalid_value);
    
    // Add this configuration parameter to the list in the driver.
    m_driver->m_config_params.push_back(this);
    
    // Add the effective-value parameter to the list of
    // write-protected parameters.
    m_driver->addProtectedParam(m_effective_param);
    
    // If this is an internal parameter, add the desired-value
    // parameter to the list of write-protected parameters.
    if (m_internal) {
        m_driver->addProtectedParam(m_desired_param);
    }
}

template <typename ValueType, typename EffectiveValueType>
ValueType TRConfigParam<ValueType, EffectiveValueType>::getDesired ()
{
    assert(m_initialized);
    
    return Traits::getParam(m_driver, m_desired_param);
}

template <typename ValueType, typename EffectiveValueType>
void TRConfigParam<ValueType, EffectiveValueType>::setDesired (ValueType value)
{
    assert(m_initialized);
    assert(m_internal);
    
    Traits::setParam(m_driver, m_desired_param, value);
}

template <typename ValueType, typename EffectiveValueType>
void TRConfigParam<ValueType, EffectiveValueType>::setEffectiveParam (EffectiveValueType value)
{
    EffectiveTraits::setParam(m_driver, m_effective_param, value);
}

template <typename ValueType, typename EffectiveValueType>
void TRConfigParam<ValueType, EffectiveValueType>::setSnapshotToDesired ()
{
    m_snapshot_value = Traits::getParam(m_driver, m_desired_param);
    m_irrelevant = false;
}

template <typename ValueType, typename EffectiveValueType>
void TRConfigParam<ValueType, EffectiveValueType>::setEffectiveToSnapshot ()
{
    setEffectiveParam(m_irrelevant ? m_invalid_value : m_snapshot_value);
}

template <typename ValueType, typename EffectiveValueType>
void TRConfigParam<ValueType, EffectiveValueType>::setEffectiveToInvalid ()
{
    setEffectiveParam(m_invalid_value);
}

// Explicit template instantiations
template class TRConfigParam<int, int>;
template class TRConfigParam<int, double>;
template class TRConfigParam<double, double>;
