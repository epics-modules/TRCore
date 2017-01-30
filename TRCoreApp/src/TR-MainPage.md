Main Page       {#mainpage}
============

This is the documentation of the Transient Recorder Framework for AreaDetector.

Transient recorders, commonly call "digitizers",
are devices which capture analog data in response to specific trigger events.
The Transient Recorder Framework aims to simplify the integration of digitizers into AreaDetector and EPICS.

The first driver developed using the framework is for General Standards 16AI64SSA/C digitizers.
Actually the framework is the result of refactoring an older EPICS integration of these digitizer models.
While this documentation aims to document the framework extensively and sufficiently to develop a new driver,
the General Standards driver may still be useful as a reference.

Note that the much of information provided on this page is only a summary;
the class and method descriptions provide more detailed documentation.
All the relevant documentation must be consulted when implementing a driver,
as otherwise important details may be missed.
This is especially important due to various synchronization requirements,
such as which methods are or must be called with the asyn port lock and which without.

The most important features provided by the framework are
management of the **arming and disarming sequence**,
management of **configuration** used for acqusition,
and an implementation of a generic burst **reading and processing loop**.

# Main Concepts

A Transient Recorder **driver** is a set of source code that implements support for a group of digitizers using the Transient Recorder framework.
In order to use the framework, a driver needs to implement a class derived from @ref TRBaseDriver. 
Since this derived class is how the driver presents itself to the framework, the term *driver* is often used in this
documentation to refer to this derived class.

A **channel** is a single source of analog time-lapse data from the digitizer.
The framework supports an arbitrary number of channels,
but the maximum number of channels must be specified at construction (using @ref TRBaseConfig).
The framework does not itself provide configuration for selection of active channels
because each hardware has its own capabilities.

A **burst** is the data resulting from a trigger event, spanning one or more channels.
It is the responsibility of the driver to obtain the data associated with a burst from the hardware
and provide it to the framework.
On the other hand, the framework is responsible for distributing this data into AreaDetector and EPICS.

To **arm** the device means to configure it to be ready to receive triggers and generate data.
To **disarms** the device means to bring it back to an idle state where it will not react to triggers
or generate data.
Arming and disarming consists not only of actions performed on hardware but also any internal
setup withing the driver to initiate and cease relevant operations.

A (Transient Recorder) **configuration parameter** is an abstract variable controllable through the external (EPICS)
interface which controls some aspect of operation while the device is armed.
A configuration parameter is represented by the class @ref TRConfigParam.
The defining feature of Transient Recorder configuration parameters is that the framework takes a snapshot
of all configuration parameters at the beginning of arming.
For details, refer to section [Configuration Parameters](#configuration-parameters).

# Arming, Acquisition and Disarming    {#arming-acquisition-disarming}

The **arming sequence** includes all actions associated with arming, data processing and disarming.
The framework drives the arming sequence and the driver only implements its device-specific parts.

All parts of the arming sequence are executed on a single thread managed by the framework.
This is seen to be a major benefit because it absolves the driver of certain  (but not all)
synchronization considerations.

The arming sequence beings with an external request to arm the device.
From that point on, it consists of the actions listed below.

- **Waiting for preconditions**: @ref TRBaseDriver::waitForPreconditions
  In this function the driver has the chance to wait for any preconditions for arming
  to be satisfied. This is meant to be used to wait for internal asynchronous operations
  within the driver to complete.

- **Verifying the configuration**: @ref TRBaseDriver::checkSettings.
  In this function the driver verifies that the desired configuration for acquisition
  is valid and provides certain information to the framework. The framework itself
  also performs basic verification of a few generic configuration parameters before
  calling this function.

- **Starting the acqusition**: @ref TRBaseDriver::startAcquisition.
  In this function the driver configures the hardware to be ready to react to triggers
  and capture data, as well as prepares itself to be ready to receive burst data
  from the hardware.

- **Reading bursts**. The framework implements the infrastructure for a generic
  data reading and processing loop (called simply the **read loop**).
  For more information, see rection [Read Loop](#read-loop).

- **Stopping the acquisition**: @ref TRBaseDriver::stopAcquisition.
  Different events will trigger stopping the acquisition, such as a disarm request
  from the external interface, the requested number of bursts having been processed,
  or a disarm request from the driver.

# Clock Calculation

The driver is responsible for calculating the achievable sample rate based on
the requested sample rate. When the framework calls the virtual function
@ref TRBaseDriver::requestedSampleRateChanged, the driver must calculate the
achievable sample rate corresponding to TRBaseDriver::getRequestedSampleRate and
report it to to framework by calling @ref TRBaseDriver::setAchievableSampleRate.
If the calculation is time-consuming, it should be done asynchronously on a
separate thread (@ref TRWorkerThread can be used) and @ref TRBaseDriver::waitForPreconditions
must be implemented to wait for any ongoing calculation to complete.
See the General Standards driver for an example of asynchronous clock calculation.

# Common Configuration

After @ref TRBaseDriver::checkSettings is called, the driver will be able to
query certain common configuration parameters and must behave according their values.

- @ref TRBaseDriver::getNumBurstsSnapshot : The desired number of bursts to be processed.
  If the driver is using the framework's read loop, there is no need to handle this.
- @ref TRBaseDriver::getNumPostSamplesSnapshot : Number of post-trigger samples.
- @ref TRBaseDriver::getNumPrePostSamplesSnapshot : Total number of samples for prePostTrigger mode.
  Zero means pre-samples are to be disabled. If the driver does not declare support for
  pre-samples (@ref TRBaseConfig::supports_pre_samples was not set to true), this will
  be always be zero.
- @ref TRBaseDriver::getRequestedSampleRateSnapshot and
  @ref TRBaseDriver::getAchievableSampleRateSnapshot : The requested and corresponding
  calculated achievable sample rate.

# Configuration Parameters     {#configuration-parameters}

Configuration parameters (@ref TRConfigParam) should be used for runtime-configurable
parameters which affect operation while the device is armed (or being armed) but whose
modification during acqusition or arming is not supported.

Drivers are free to define any number of configuration parameters.
This is done by including TRConfigParam classes as members of the driver class
and initializing them using @ref TRBaseDriver::initConfigParam in the constructor.

Just after the function @ref TRBaseDriver::waitForPreconditions returns successfully,
the framework will make snapshots of the current set of configuration parameters.
Hence, the framework differrentiates between the **desired** value of the parameter,
which can be chaned at any time through the external (EPICS) interface,
and the **snapshot** value of the parameter, which is meant to be used by the driver.

The desired value is implemented as an asyn parameter while the snapshot value
is simply a (private) variable in TRConfigParam.

There is another asyn parameter associated with each configuration parameter:
the **effective** value parameter, which reports the current value of the parameter
that the device is armed with.

When the device is not armed, the effective value will be an *invalid value*,
which is specified when initializing the configuration parameter.
After successfully calling @ref TRBaseDriver::checkSettings,
the framework will set the effective values of all configuration parameters
to the snapshot values.
However, drivers may call @ref TRConfigParam::setIrrelevant to mark
specific parameters as irrelevent. The effective values of those will remain
at invalid values instead of being set to the snapshot values.

After the device is disared, the framework will revert all the effective values
back to invalid values.

# Read Loop     {#read-loop}

The read loop is the central part of the arming sequence.
It is executed on the same thread as the rest of the arming sequence,
between @ref TRBaseDriver::startAcquisition and @ref TRBaseDriver::stopAcquisition.

The read loop is optional; refer to @ref TRBaseDriver::readBurst for more detailed
documentation on how to not use the read loop.
However it is assumed that most drivers will leverage the framework's read loop,
and this should be possible for any hardware which uses FIFO buffering.

If the driver uses the read loop, the framework will internally implement
the feature to disarm after a specific number of bursts have been processed.
Otherwise, drivers should implement this internally, with the assistance of
the functions @ref TRBaseDriver::getNumBurstsSnapshot and
@ref TRBaseDriver::requestDisarmingFromDriver.

Additionally, the read loop provides some assistance for implementation
of handling overrun of the hardware FIFO buffer, in the form of the
virtual function @ref TRBaseDriver::checkOverflow and automatically
restarting acquisition after the remaining buffered bursts have been read.

The read loop performs the following actions repeatedly:

- Waiting for and **reading a single burst**: @ref TRBaseDriver::readBurst.
  The driver is supposed to store the burst data internally at this point.

- Optionally, checking if **buffer overflow** has occurred:
  @ref TRBaseDriver::checkOverflow. This is called only if buffer overflow
  has not yet occurred.

- **Processing the data** of the burst: @ref TRBaseDriver::processBurstData.
  From this function, the driver should use the @ref TRChannelDataSubmit
  class to submit burst data for different channels to the framework.

# Channels Port

Channel data is submitted into the AreaDetector framework through the **channels port**.
The channels port is based on the class @ref TRChannelsDriver and is a separate port
from the main port that is based on @ref TRBaseDriver.

The reason that a separate port is used is that the channels port needs to be an
asyn multi-device port since it provides channel-specific data and configuration,
while the parameters of the main port are not channel-specific.
Also note that TRBaseDriver is based on asynPortDriver while TRChannelsDriver
is based on asynNDArrayDriver.

The framework will itself create an instance of the channels driver.
It is possible (but not in any way required) for a driver to define its own class derived
from TRChannelsDriver; this is done by overriding @ref TRBaseDriver::createChannelsDriver.

# Port Initialization

The driver will need to provide its own initialization function that creates an
instance of the driver.
Note that the initialization function must call TRBaseDriver::completeInit
after constructing the driver.

In order to bring channel data to waveform records, the standard AreaDetector StdArrays plugins
need to be used, one for each channel.
The framework provides a database template for this (`TRChannelWaveforms.db`, see below),
but the StdArrays plugins need to be initialized manually.

# Database Templates     {#database-templates}

The database template `TRBase.db` defines the common records corresponding to
functions implemented by @ref TRBaseDriver.
This template requires the following macros:
- `PREFIX`: Prefix of records (a colon after the prefix is implied).
- `PORT`: Port name of the driver based on TRBaseDriver.
- `DEVICE_NAME`: Arbitrary device name for the `name` record.
- `SIZE` - Waveform size (NELM) for the time array.
- `PRESAMPLES` - "#" if presamples are not supported, empty if supported

Each driver will need to provide one or more database templates of its own,
supporting configuration parameters and other functions specific to the driver.

For each configuration parameter defined by the driver (initialized by @ref TRBaseDriver::initConfigParam),
two records should be defined: an output record for the desired value and an input
record for the effective value.
The asyn parameter names of these will based on the base parameter name,
by prepending the prefix `DESIRED_` or `EFFECTIVE_`.
The records in `TRBase.db` can be used as an example.

The database template `TRChannel.db` provides channel-specific configuration records
supported by the class @ref TRChannelsDriver.
It requires the following macros:
- `PREFIX`: Prefix of records (a colon is implied), this should include identification of the channel.
- `CHANNELS_PORT`: Port name of the channels driver. This is the name of the base
  driver with the suffix `_channels`.
- `CHANNEL`: Channel number (asyn address for TRChannelsDriver).

The database template `TRChannelWaveforms.db` provides waveform records for channel data,
relying on the AreaDetector StdArrays plugin.
It requires the following macros:
- `PREFIX`: Prefix of records (a colon is implied), this should include identification of the channel.
- `STDARRAYS_PORT`: Name of the stdarrays port for the channel.
- `SIZE`: Waveform size (NELM).
- `REFRESH_SNAPSHOT_SCAN`: SCAN rate for updating the data snapshot (e.g. "1 second").

# List of PVs

- @ref framework-pvs
- @ref genstds-pvs

