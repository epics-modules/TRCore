List of Framework PVs       {#framework-pvs}
============

This page describes the public PVs implemented by the Transient Recorder framework as well
as internal PVs meant to be used by driver-specific database code.

For instructions for loading the correct DB files, consult the [main page](@ref index).

The names of all the PVs documented here can be customized (see @ref customizing-pvs).

Most of the records in this section are provided by the database file `TRBase.db`.
However, channel-specific records (containing `CH<N>` in their name) originate
from `TRChannel.db` or `TRChannelWaveform.db`.

The common record prefixes are omitted in these listings. They are specified as database
template parameters to the associated database files. Note that channel-specific database files
may be loaded with a prefix different than `CH<N>`, perhaps with a name more descriptive
of the purpose of the channel.

## Device Information

<table>
    <tr>
        <th>PV name, record type</th>
        <th>Description</th>
    </tr>
    <tr>
        <td valign="top">`name` (stringin)</td>
        <td>
            The device name or model.
            
            It is the value that the driver set using @ref TRBaseDriver::setDigitizerName
            or the asyn port name if the driver did not call this function.
        </td>
    </tr>
</table>

## Desired Settings for Arming

These PVs should be set to the desired settings before an arm request is issued.
Changing them has no immediate effect, but their values are caputed at the start of the arm procedure.
The one exception is the `ACHIEVABLE_SAMPLE_RATE` which must not be written but is
automatically calculated from the desired clock settings.

<table>
    <tr>
        <th>PV name, record type</th>
        <th>Description</th>
    </tr>
    <tr>
        <td valign="top">`autoRestart` (bo)</td>
        <td>
            Selects single-burst mode or determined by `NUM_BURSTS`.
            
            `Off` means to disarm after the first burst,
            `On` means the number of bursts is selected by `NUM_BURSTS`.
            The default os `On`.
            
            It is possible to customize the ZNAM/ONAM fields by passing the macros
            `AUTORESTART_ZNAM`/`AUTORESTART_ONAM` to `TRBase.db`.
        </td>
    </tr>
    <tr>
        <td valign="top">`NUM_BURSTS` (longout)</td>
        <td>
            When `autoRestart` is `On`, selects the number of bursts.
            
            Zero or a negative value means to process an unlimited number bursts,
            a positive value means to process the fixed number of bursts then disarm.
        </td>
    </tr>
    <tr>
        <td valign="top">`numberPTS` (longout)</td>
        <td>
            Number of post-trigger samples (of each channel).
        </td>
    </tr>
    <tr>
        <td valign="top">`numberPPS` (longout)</td>
        <td>
            Total number of samples for prePostTrigger mode.
            
            This is only relevant when arming in prePostTrigger mode.
            In that case, it should be the total number of samples per burst
            (pre + post samples), and must be greater than `numberPTS`.
        </td>
    </tr>
    <tr>
        <td valign="top">`_requestedSampleRate` (ao)</td>
        <td>
            This is an internal record designed to be written by driver-specific
            database code to forward the sample rate requested by the user to the framework.
            
            Writing this PV will result in the function
            @ref TRBaseDriver::requestedSampleRateChanged being called, and the value written
            will be available through the function @ref TRBaseDriver::getRequestedSampleRate.
            
            For an example of database code writing this record, see the implementation in the
            General Standards driver (`TRGeneralStandards.db`). There, the value of `_requestedSampleRate`
            is computed based on the records `clock` and `CUSTOM_SAMPLE_RATE`.
            
            This record can be excluded by passing `NOCLK=#` to `TRBase.db`.
        </td>
    </tr>
    <tr>
        <td valign="top">`ACHIEVABLE_SAMPLE_RATE` (ai)</td>
        <td>
            Provides the sample rate (Hz) that would actually be achieved.
            
            This is computed based on the desired sample clock configuration
            (`_requestedSampleRate` but possibly also driver-specific settings).
            The value of this record is provided by the function `TRBaseDriver::setAchievableSampleRate`.
            
            Note that because calculation of the clock configuration may be performed
            asynchronously, this PV is not guaranteed to reflect the new settings immediately
            after the settings are changed.
            
            This record can be excluded by passing `NOCLK=#` to `TRBase.db`.
        </td>
    </tr>
</table>

## Arming and Disarming

These records are for requesting arming or disarming and for determining the current state
of the arming sequence.

<table>
    <tr>
        <th>PV name, record type</th>
        <th>Description</th>
    </tr>
    <tr>
        <td valign="top">`arm` (mbbo)</td>
        <td>
            This PV both indicates the current armed state and can be used to request arming or disarming.
            
            Possible values of this PV are:
            - `disarm` (device is disarmed; write to request disarming),
            - `postTrigger` (device is armed without pre-samples; write to request arming),
            - `prePostTrigger` (device is armed with pre-samples; write to request arming),
            - `busy` (device is being armed or disarmed),
            - `error` (there has been an error).
            
            It is only allowed to write `disarm`, `postTrigger` or `prePostTrigger` to this PV
            Writes of other values will be ignored and the PV will remain at its original value.
            
            Writing `disarm`, `postTrigger` or `prePostTrigger` will normally result in a transition
            to the `busy` state. Only at the end of arming/disarming will the value change to the value
            that was written (or to `error`).
            
            If `postTrigger` or `prePostTrigger` is written while the state is not `disarm`, disarming
            and re-arming will be performed. Therefore, writing `postTrigger` or `prePostTrigger` is
            acceptable at any time to get the device armed with the current settings irrespective of
            the current state.
            
            The `prePostTrigger` option is only available when the device supports pre-samples.
        </td>
    </tr>
    <tr>
        <td valign="top">`set_arm` (mbbo)</td>
        <td>
            This PV can be used for arm or disarm requests.
            
            Writing `disarm`, `postTrigger` or `prePostTrigger` to this PV is equivalent to writing
            it into `arm`. Conversly, writing to `arm` will update `set_arm`, reflecting the last request.
            
            The reason for this PV is that it does not have the special values `busy` and `error`
            defined, which is useful with certain GUIs, so that these are not shown as values that
            can be written.
            
            The `prePostTrigger` option is only available when the device supports pre-samples.
        </td>
    </tr>
</table>

## Current Armed Settings 

These PVs hold the settings that the device has been armed with.
When the device is not armed, they contain placeholder values
such as `NAN` for numeric values and `N/A` for choice values.
These PVs must not be written.

Note that for `mbbi` records, the values are not listed,
and are the same as for the corresponding desired-setting PVs with the addition of `N/A`.

<table>
    <tr>
        <th>PV name, record type</th>
        <th>Description</th>
    </tr>
    <tr>
        <td valign="top">`GET_ARMED_NUM_BURSTS` (ai)</td>
        <td>
            Current effective number of bursts to be processed.
            
            This is based on the `autoRestart` and `NUM_BURSTS` settings used for arming.
            Zero means unlimited bursts, positive means that number of bursts.
        </td>
    </tr>
    <tr>
        <td valign="top">`get_numberPTS` (ai)</td>
        <td>
            Current armed number of post-trigger samples; see `numberPTS`.
        </td>
    </tr>
    <tr>
        <td valign="top">`get_numberPPS` (ai)</td>
        <td>
            Current armed total number of samples for prePostTrigger mode; see `numberPPS`.
        </td>
    </tr>
    <tr>
        <td>`GET_ARMED_REQUESTED_SAMPLE_RATE` (ai)</td>
        <td valign="top">
            Current armed requested sample rate; see `_requestedSampleRate`.
            
            This record can be excluded by passing `NOCLK=#` to `TRBase.db`.
        </td>
    </tr>
    <tr>
        <td valign="top">`GET_SAMPLE_RATE` (ai)</td>
        <td>
            Current armed achievable sample rate; see `ACHIEVABLE_SAMPLE_RATE`.
            
            This record can be excluded by passing `NOCLK=#` to `TRBase.db`.
        </td>
    </tr>
    <tr>
        <td valign="top">`GET_DISPLAY_SAMPLE_RATE` (ai)</td>
        <td>
            Current armed display sample rate.
            
            This is the value that was returned by the driver in @ref TRBaseDriver::checkSettings
            via @ref TRArmInfo::rate_for_display.
            It also the same sample rate that the time array (`TIME_DATA`) is based on.
        </td>
    </tr>
</table>

## Acquisition Information

These PVs provide the channel data and additional information about burst processing.

Note that the size (`NELM`) of all waveform records is determined by the value
of the `SIZE` parameter of associated database templates (`TRBase.db` and `TRChannelData.db`).

The data type of the waveforms is determined by the `FTVL` parameter to `TRChannelData.db`
and is `DOUBLE` by default (if this is changed then `WF_DTYP` also needs to be adjusted).
However be aware that the digitizer driver determines the data type of NDArrays it submits.
If the NDArray data type does not match `FTVL`, the StdArrays plugin will convert the data.

<table>
    <tr>
        <th>PV name, record type</th>
        <th>Description</th>
    </tr>
    <tr>
        <td valign="top">`perSecond` (longin)</td>
        <td>
            The number of bursts per second.
            
            This PV is updated each second to the number of bursts which have been
            processed in the previous second.
            
            It is possible to add custom fields to this record by passing the macro
            `PERSECOND_FIELDS` to `TRBase.db`.
        </td>
    </tr>
    <tr>
        <td valign="top">`TIME_DATA` (waveform)</td>
        <td>
            The relative sample time waveform (unit: see `TIME_UNIT_INV`).
            
            This is recalculated whenever the device is armed.
            It will consist of time values for pre-samples (if pre-samples are enabled)
            and time values for post-samples. The pre-sample times will be negative,
            the first post-sample time will be 0 and the remaining post-sample times
            will be positive. The difference between subsequent sample times will be
            1/`GET_DISPLAY_SAMPLE_RATE`.
            However drivers are able to override the calculation of the time array,
            so the semantics may be different.
            
            The `EGU` field of this record corresponds to the macro `TIME_EGU` passed
            to `TRBase.db`, which is "s" by default if not passed.
        </td>
    </tr>
    <tr>
        <td valign="top">`TIME_UNIT_INV` (ao)</td>
        <td>
            The unit for `TIME_DATA`, as fractions of a second, e.g. 1000 for ms.
            
            The initial value is according to the `TIME_UNIT_INV` macro passed
            to `TRBase.db`, which itself has the default value 1 (second).
            
            Changes to this PV during operation will be reflected in new data
            (newly generated NDArrays). Note that changes do not affect the EGU
            of the data waveforms, it is your responsibility to keep these in sync
            if needed.
        </td>
    </tr>
    <tr>
        <td valign="top">`CH<N>:DATA` (waveform)</td>
        <td>
            The data waveform for channel N.
            
            The timestamp of the data waveform is based on the timestamp of the
            associated NDArray, which is itself obtained from the Asyn updateTimeStamp
            function on the main port (not the channels port).
            
            While this is not reflected in this waveform record, note that the NDArrays
            generated by the channel ports will be tagged with an attribute `READ_SAMPLE_RATE`
            which will be equal to `GET_DISPLAY_SAMPLE_RATE`.
            
            It is possible to define a link to be processed after data is updated,
            by passing the macro `DATA_UPD_LNK` to `TRChannelData.db`.
        </td>
    </tr>
    <tr>
        <td valign="top">`CH<N>:DATA_SNAPSHOT` (waveform)</td>
        <td>
            The slow-updating data waveform for channel N.
            
            The data and timestamp will match the `CH<N>:DATA` waveform at the time the
            snapshot was made.
            
            The update rate of slow waveforms is defined by the parameter `SNAP_SCAN`
            to the database template `TRChannelData.db`. Each update period, the update is made
            only if the `DATA` waveform has been updated since the previous time.
            That is, there will be no redundant periodic CA monitor events if there was no new data.
            
            It is possible to define a link to be processed after snapshot data is updated,
            by passing the macro `SNAP_UPD_LNK` to `TRChannelData.db`.
            
            These snapshot records can be disabled by passing the macro `SNAPSHOT=#` to
            `TRChannelData.db`.
        </td>
    </tr>
    <tr>
        <td valign="top">`GET_BURST_ID` (longin)</td>
        <td>
            The identification number of the last burst.
            
            This PV will be updated for every burst with the burst ID of that burst.
            Burst IDs are generated by the driver, but the expectation is that the first
            burst ID will be zero, each subsequent ID will be incremented by one
            up to 2<sup>31</sup>-1, after which point it will wrap back to zero.
            The expectation is also that the ID is not reset to zero when arming
            again.
            
            It is possible to specify a database link to be processed on each
            new burst, by passing the macro `LNK_NEW_BURST` to `TRBase.db`. This link
            would be processed after framework PVs with burst information are updated
            (e.g. `GET_LAST_BURST_TIME`).
        </td>
    </tr>
    <tr>
        <td valign="top">`GET_LAST_BURST_TIME` (stringin)</td>
        <td>
            The time-and-date string of the last processed burst, based on the burst timestamp.
            
            The format of the timestamp is according to the `TIMESTAMP_FMT` macro passed
            to `TRBase.db` (as understood by the EPICS Soft Timestamp device support).
            If the macro is not passed the default is `%Y-%m-%d %H:%M:%S.%06f`.
        </td>
    </tr>
    <tr>
        <td valign="top">`GET_BURST_START_TO_BURST_END_TIME` (ai)</td>
        <td>
            The time from when the start of the burst was detected to when the
            end of the burst was detected, for the last processed burst.
            
            `NAN` if the driver did not provide this information.
        </td>
    </tr>
    <tr>
        <td valign="top">`GET_BURST_END_TO_READ_END_TIME` (ai)</td>
        <td>
            The time from when the end of the burst was detected to when the
            burst has been read to memory, for the last processed burst.
            
            `NAN` if the driver did not provide this information.
        </td>
    </tr>
    <tr>
        <td valign="top">`GET_READ_END_TO_DATA_PROCESSED_TIME` (ai)</td>
        <td>
            The time from when the burst has been read to memory to when the burst data
            has been processed (sent into AreaDetector).
            
            `NAN` if the driver did not provide this information.
        </td>
    </tr>
</table>

## Acquisition Control

These PVs are used to control the acquisition process.
Changes to the values of these PVs are effective immediately
(as opposed to values being captured at the start of arming).

<table>
    <tr>
        <th>PV name, record type</th>
        <th>Description</th>
    </tr>
    <tr>
        <td valign="top">`SET_TEST_READ_SLEEP_TIME` (ao)</td>
        <td>
            Artificial time delay after processing a burst (in seconds).
            
            This allows inserting a time delay to the read loop each time after a
            burst is processed. It is useful for testing hardware buffer overflow.
            
            The default is zero.
        </td>
    </tr>
    <tr>
        <td valign="top">`CH<N>:ENABLE_ARRAY_CALLBACKS` (bo)</td>
        <td>
            Enable AreaDetector NDArray callbacks for the channel (`Off` or `On`).
            
            If callbacks are `Off`, the channels port will not generate NDArray
            callbacks for this channel, and consequently its data will not appear
            to any NDArray plugins connected to the channel, including StdArrays
            plugins supporting the data waveform records.
            
            The default is `On`.
        </td>
    </tr>
    <tr>
        <td valign="top">`CH<N>:ENABLE_UPDATE_ARRAYS` (bo)</td>
        <td>
            Enable updating of the cached NDArray for the channel (`Off` or `On`).
            
            If this is `On`, then `pArrays[channel_index]` in the channels port will
            be updated each time a new NDArray is generated for this channel, otherwise
            it will not be. Also note that the framework will clear all arrays from
            `pArrays` at the start of arming.
            
            Turning this off saves some memory as it allows each NDArray to be freed
            after it is processed.
            
            The default is `On`.
        </td>
    </tr>
    <tr>
        <td valign="top">`CH<N>:SET_BLOCKING_CALLBACKS` (bo)</td>
        <td>
            Enable/disable blocking-callbacks for the StdArrays port of this channel (`Off` or `On`).
            
            **Warning:** disabling blocking callbacks will start the callback processing thread for
            this channel if it has not been started yet. If there is not enough memory, this will
            result in memory allocation failures likely followed by a complete failure of the IOC.
            
            The default is determined by the arguments when initializing the StdArrays ports.
        </td>
    </tr>
    <tr>
        <td valign="top">`CH<N>:GET_BLOCKING_CALLBACKS` (bi)</td>
        <td>
            Readback value of the blocking-callbacks setting for this channels (`Off` or `On`).
            
            This is a readback only and must not be written.
        </td>
    </tr>
</table>
