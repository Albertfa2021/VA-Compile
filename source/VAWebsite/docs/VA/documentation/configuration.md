
# VA configuration
VA can be configured using a section-based key-value parameter collection which is passed on to the core instance during initialization. This is usually done by providing a path to a text-based INI file which will be referred to as `VACore.ini` but can be of arbitrary name. When starting the `VAServer` application, it uses the `VACore.ini` within the `conf` folder per default.

## Using a specific VACore.ini
It is possible to start the VAServer using the filepath to any INI file as additional variable, e.g. using the command line or a batch file:
>bin\VAServer.exe localhost:12340 conf\MyVACore.ini 

It is **highly recommended** to start the VAServer from the VA root folder, since this allows VA to get access to provided configuration and data files. This is the reason why the provided batch files are located within the root folder. You can create your own batch files linking to user-defined INI files to configure VA to your needs.

----------

## Basic configuration
### Search paths
The `Paths` section allows for adding search paths to the core. If resources like head-related transfer functions (HRTFs), geometry files, or audio files, are required these search paths guarantee to locate the requested files. Relative paths are resolved from the execution folder where the VA server application is started from. When using the provided batch start scripts on Windows it is recommended to add `data` and `conf` folders.
```
[Paths]

data = data
conf = conf

my_data = C:/Users/Me/Documents/AuralizationData
my_other_data = /home/me/auralization/input
```

### Additional configuration files
In the `Files` section, you can name files that will be included as further configuration files. This is helpful when certain configuration sections must be outsourced to be reused efficiently. Outsourcing is especially convenient when switching between static sections like hardware descriptions for laboratories or setups, but can also be used for rendering and reproduction modules (see below). Avoid copying larger configuration sections that are re-used frequently. Use different configuration files, instead. 
```
[Files]

old_lab = VASetup.OldLab.Loudspeakers.ini
#new_lab = VASetup.NewLab.Loudspeakers.ini
```

### Marcros

The Macros section is helpful to write tidy scripts. Use macros if it is not explicitly required to use a specific input file. For example, if any HRTF can be used for a receiver in the virtual scene the DefaultHRIR will point to the default HRTF data set, or head-related impulse response (HRIR) in time domain.
Any defined macros will be replaced by the user-defined value during run-time. For example, macros are also very helpful if certain file prefixes are desired, e.g. to get better structured file names for recordings. 

Usage: "$(*MyMacroName*)/file.abc" -> "*MyValue*/file.abc"

Another typical use-case is the definition of default input files. Note, that those macros are not only valid within the *VACore.ini* but can also be used when setting up a scene through a binding. In the VAMatlab example [VA_example_simple.m](https://git.rwth-aachen.de/ita/VAMatlab/-/blob/master/matlab/VA_example_simple.m){target="_blank"}, the `DefaultHRIR` is used to apply an HRTF to a receiver.


```
[Macros]

DefaultHRIR = ITA_Artificial_Head_5x5_44kHz_128.v17.ir.daff
HumanDir = Singer.v17.ms.daff
Trumpet = Trumpet1.v17.ms.daff

# Define some other macros (examples)
ProjectName = MyVirtualAcousticsProject
```

### Debug
The `Debug` section allows to control the log level. Running the VAServer, the log will be printed to the respective command line.

```
[Debug]

# Set log level: 0 = quiet; 1 = errors; 2 = warnings (default); 3 = info; 4 = verbose; 5 = trace;
LogLevel = 3
```

----------

## Audio driver configuration

The audio interface controls the backend driver and the device. In the current version, for the Driver backend key, ASIO is supported on Windows only, whereas Portaudio is available on all platforms. By default, Portaudio with the default driver is used that usually produces audible sound without further ado. However, the block sizes are high and the update rates are not sufficient for real-time auralization using motion tracking. Therefore, dedicated hardware and small block sizes should be used - and ASIO is recommended for Windows platforms.

### Portaudio example
If you do not have any latency requirements you can also use `Portaudio` under Windows and other platforms. The specific device names of Portaudio interfaces can be detected, for example, using the VLC player or with Audacity. But the `default` device is recommended simply because it will pick the audio device that is also registered as the default device of your system. This is, what most people need anyway, and the system tools can be used to change the output device. It is recommended to let the `Buffersize` be automatically detected using the `AUTO` option.
<!---
PSC: Is this still relevant?
If the `Buffersize` is unkown, at least the native buffer size of the audio device should be used (which is most likely 1024 for on-board chips). Otherwise, timing will behave oddly which has a negative side effect on the rendering.
-->
```
[Audio driver]

Driver = Portaudio
Samplerate = 44100
Buffersize = AUTO
Device = default
```

### ASIO example using ASIO4ALL v2
[ASIO4ALL](http://www.asio4all.de/){target="_blank"} is a useful and well-implemented intermediate layer for audio I/O making it possible to use ASIO drivers for the internal hardware (and any other audio device available). It must be installed on the PC, first.
```
[Audio driver]

Driver = ASIO
Samplerate = 44100
Buffersize = AUTO
Device = ASIO4ALL v2
```
Although it appears that the buffer size can be defined for ASIO devices, the ASIO backend will automatically detect the buffer size that has been configured by the driver when the AUTO value is set (recommended). Set the buffer size in the ASIO driver dialog of your physical device, instead. Make sure, that the sampling rates are matching.
ASIO requires a device name to be defined by each driver host. Further common hardware device names are listed in the following.

### Common ASIO hardware device names

| Manufacturer  | Device            | ASIO device name                  |
|-------------- |-------------------|-----------------------------------|
| RME           | Hammerfall DSP    | `ASIO Hammerfall DSP`             |
| RME           | Fireface USB      | `ASIO Fireface USB`               |
| RME           | MADIface USB      | `ASIO MADIface USB`               |
| Focusrite     | 2i2, 2i4, ...     | `Focusrite USB 2.0 Audio Driver` or `Focusrite USB ASIO`    |
| M-Audio       | Fast Track Ultra  | `M-Audio Fast Track Ultra ASIO`   |
| Steinberg     | 6UR22 MK2         | `Yamaha Steinberg USB ASIO`       |
| Realtek       | Realtek Audio HD  | `Realtek ASIO`                    |
| Zoom          | H6                | `ZOOM H and F Series ASIO`        |
| ASIO4ALL      | any Windows device| `ASIO4ALL v2`                     |
| Reaper (x64)  | any Reaper device | `ReaRoute ASIO (x64)`             |

----------

## Audio hardware configuration
The `Setup` section describes the hardware environment in detail. It might seem a bit over the top but the complex definition of hardware groups with logical and physical layers eases re-using of physical devices for special setups and also allows for multiple assignments - similar to the RME matrix concept of TotalMix, except that volume control and mute toggling can be manipulated in real-time using the VA interface instead of the ASIO control panel GUI.<br>
The hardware configuration can be separated into inputs and outputs, but they are basically handled in the same manner. More importantly, the setup can be devided into **devices of specialized types** and **groups that combine devices**. Often, this concept is unnecessary and appears cumbersome, but there are situations where this level of complexity is required.<br>
A **device** is a physical emmitter (`OutputDevice`) or transducer (`InputDevice`) with a fixed number of channels and assignment using (arbitrary but unique) channel indices. A broadband loudspeaker with one line input is a typical representative of the single channel `LS` type `OutputDevice` that has a fixed pose in space. A pair of headphones is assigned the type `HP` and usually has two channels, but no fixed pose in space.
So far, there is only an input device type called `MIC` that has a single channel. 

Physical devices can not directly be used for a playback in VA. A [reproduction module](#reproduction-module-configuration) can rather be connected with one or many `Outputs` - logical groups of `OutputDevices`.<br>
Again, for headphones this seems useless because a headphone device will be represented by a virtual group of only one device. However, for loudspeaker setups this makes sense as, for example, a setup of 7 loudspeakers for spatial reproduction may be used by different groups which combine only 5, 4, 3, or 2 of the available loudspeakers to form an output group. In this case, only the loudspeaker identifiers are required and channels and positions are made available by the physical device description. Following this strategy, repositioning of loudspeakers and re-assignment of channel indices is less error prone due to its organization in one configuration section, only. 

### Headphone setup example
Let us assume you have a pair of Sennheiser HD 650 headphones at your disposal and you want to use it for binaural rendering and reproduction. This is the most common application of VA and will result in the following configuration:
```
[Setup]

[OutputDevice:SennheiserHD650]
Type = HP
Description = Sennheiser HD 650 headphone hardware device
Channels = 1,2

[Output:DesktopHP]
Description = Desktop user with headphones
Devices = SennheiserHD650
```
If you want to use another output jack for some reason change your channels accordingly, say to `3,4`.

### Loudspeaker setup example
Let us assume you have a square-shaped loudspeaker setup of Neumann KH120 at your disposal. You want to use it for binaural rendering and reproduction. This is the a common application of VA for a dynamic listening experiment in a hearing booth. Note, that for loudspeaker-based reproduction, also the `Position` of each loudspeakers within the room is required. The respective origin can be arbitrarily chosen. In the example below, the center of the ground floor is taken as origin. In this case, it is required to know the center of your loudspeaker array which is typically set in the utilized [reproduction module](reproduction.md#loudspeaker-based-reproduction). This step can be avoided, if the loudspeaker array center is taken as origin. Optionally, if you have a directivity of your loudspeakers at hand, you can add the `OrientationYPR` (yaw, pitch, roll in degrees) and point to the respective .daff file.

``` ini
[Setup]

[OutputDevice:NeumannKH120_FL]
Type = LS
Description = Neumann KH 120 in front left corner of square
Channels = 1
Position = -0.8591, 1.522,-0.8591
OrientationYPR = -135, -19.1, 0
DataFileName = MyNeumannKH120Directivity.daff

[OutputDevice:NeumannKH120_FR]
Type = LS
Description = Neumann KH 120 in front right corner of square
Channels = 2
Position = 0.8605, 1.521, -0.8605
OrientationYPR = 135, -19.1, 0
DataFileName = MyNeumannKH120Directivity.daff

[OutputDevice:NeumannKH120_RR]
Type = LS
Description = Neumann KH 120 in rear right corner of square
Channels = 3
Position = 0.8422, 1.537, 0.8422
OrientationYPR = 45, -19.1, 0
DataFileName = MyNeumannKH120Directivity.daff

[OutputDevice:NeumannKH120_RL]
Type = LS
Description = Neumann KH 120 in rear left corner of square
Channels = 4
Position = -0.8485, 1.516, 0.8485 
OrientationYPR = -45, -19.1, 0
DataFileName = MyNeumannKH120Directivity.daff

[Output:HearingBoothLabLS]
Description = Hearing booth laboratory loudspeaker setup
Devices = NeumannKH120_FL, NeumannKH120_FR, NeumannKH120_RR, NeumannKH120_RL
```

Note: The order of devices in the output group is irrelevant for the final result. Each LS will receive the corresponding signal on the channel of the device.

### Microphone setup example

The audio input configuration is similar to the output configuration but is not yet fully included in VA. If you want to use input channels as signal sources for a virtual sound source assign the provided unmanaged signals called `audioinput1, audioinput2, ...` . The number refers to the input channel index beginning with 1 and you can get the signals by using the getters `GetSignalSourceInfos` or `GetSignalSourceIDs`.

``` ini
[Setup]

[InputDevice:NeumannTLM170]
Type = MIC
Description = Neumann TLM 170
Channels = 1

[Input:BodyMic]
Description = Hearing booth talk back microphone
Devices = NeumannTLM170
```

----------

## Calibration
To properly calibrate a rendering and reproduction system, every component in the chain has to be carefully configured. This also holds for the digital signals being rendered. In VA, a digital value of 1.0 refers to 1 Pascal per default. For a fully calibrated system,  using a sine wave with an RMS of 1.0 (peak value of sqrt(2)) as source signal, will retain 94 dB SPL (= 1.0 Pa) at a distance of 1m.
If for your setup the digital output exceeds values of 1.0, which would lead to clipping, you can set the `DefaultAmplitudeCalibrationMode` to **124 dB**. Then a digital level of 1.0 refers to approximately 31.7 Pa. However, this comes at the price of a higher noise floor.

Additionally, there are default settings regarding the distance used for the spreading loss calculation. The `DefaultMinimumDistance` avoids an infinetely large sound pressure if the receiver gets to close to the source. The `DefaultDistance` is used when the [auralization mode](control.md#global-auralization-mode) for the spreading loss is deactivated.

```
[Calibration]

# The amplitude calibration mode either sets the internal conversion from
# sound pressure to an electrical or digital amplitude signal (audio stream)
# to 94dB (default) or to 124dB. The rendering modules will use this calibration
# mode to calculate from physical values to an amplitude that can be forwarded
# to the reproduction modules. If a reproduction module operates in calibrated
# mode, the resulting physical sound pressure at receiver location can be maintained.
DefaultAmplitudeCalibrationMode = 94dB

# Set the minimum allowed distance (m) to a sound source (point source can get infinitely loud).
# This can also be used if sound sources appear too loud near-by, but in the limiting range this
# rendering will not be physically correct.
DefaultMinimumDistance = 0.25

# The default distance is used when the spreading loss is deactivated
DefaultDistance = 2.0
```

## Homogeneous medium
For most [rendering modules](#rendering-modules), VA assumes a homogeneous medium. If you want to adjust this medium, the following lines show the respective variables and its default values.
```
[HomogeneousMedium]

DefaultSoundSpeed = 344.0 # m/s
DefaultStaticPressure = 101125.0 # [Pa]
DefaultTemperature = 20.0 # [Degree centigrade]
DefaultRelativeHumidity = 20.0 # [Percent]
DefaultShiftSpeed = 0.0, 0.0, 0.0 # 3D vector in m/s, currently not used
```

----------

## Rendering module configuration
To instantiate a rendering module, a section with a `Renderer:` suffix has to be included. The statement following `:` will be the **unique identifier** of this rendering instance (e.g. `Renderer:MyBinauralFreeFieldRenderer`). If you want to change parameters during run-time, this identifier is required to call the instance.

As discussed in the  [overview](#overview.md#audio-rendering) section, there are different audio rendering classes. Each class has it's individual settings, which are discussed in the [rendering classes](#rendering-classes) section. However, there are some general renderer settings which are discussed in the following.

### Required rendering module parameters
```
Class = RENDERING_CLASS
Reproductions = REPRODUCTION_INSTANCE(S)
```
The rendering class refers to the *type* of renderer which can be taken from the [rendering class overview](#rendering-class-overview).<br>
The parameter `Reproductions` gets a comma-separated list of reproduction modules which are connected to this renderer. For this purpose, the identifier of the respective reproduction module has to be used (see [reproduction module configuration](#reproduction-module-configuration)). At least one reproduction module has to be connected, but multiple modules of same or different type are allowed. The only restriction is that the rendering output channel number has to match the reproduction module's input channel number.

### Optional rendering module parameters
```
Description = Some informative description of this rendering module instance
Enabled = true
RecordOutputEnabled = false
RecordOutputFileName = renderer_out.wav
RecordOutputBaseFolder = recordings/MyRenderer
```
<!---
TODO: PSC: Still required?
OutputDetectorEnabled = false
For rendering modules, only the output can be observed. A stream detector for the output can be activated that will produce level meter values, for example, for a GUI widget. 
-->
>Note: until version 2018a, the record output file was only controlled by a file path key named `RecordOutputFilePath`. The file name and base folder has been introduced in 2018b. Now, the folder and file name can be modified during runtime. 

Rendering modules can be *enabled* and *disabled* to speed up setup changes without copying & pasting larger parts of a configuration section, as especially reproduction modules can only be instantiated if the sound card provides enough channels. This makes testing on a desktop PC and switching to a laboratory environment easier.<br>
Furthermore, the output of the renderer can be recorded and exported as a WAV file. Recording starts with initialization and is exported to the hard disc drive after finalization impliciting that data is kept in the RAM. If a high channel number is required and/or long recording sessions are planned it is recommended to route the output through a DAW, instead, i.e. with ASIO re-routing software devices like Reapers ReaRoute ASIO driver. To include a more versatile output file name (macros are allowed). 

----------

## Reproduction module configuration
To instantiate a reproduction module, a section with a `Reproduction:` suffix has to be included. Same as for rendering modules, the statement following `:` will be the unique identifier of this reproduction instance (e.g. `Reproduction:MyTalkthroughHeadphones`). If you want to change parameters during run-time, this identifier is required to call the instance.

As discussed in the  [overview](#overview.md#reproduction-modules) section, there are different reproduction classes. Each class has it's individual settings, which are introduced in the [reproduction classes](#reproduction-classes) section. However, there are some general settings which are discussed in the following.

### Required reproduction module parameters
```
Class = REPRODUCTION_CLASS
Outputs = OUTPUT_GROUP(S)
```
The reproduction class refers to the *type* of renderer which can be taken from the [reproduction class overview](#reproduction-class-overview).<br>
The parameter `Outputs` gets a comma-separated list of logical output groups (using the respective identifiers) which will be connected to this reproduction module. At least one reproduction module has to be connected, but multiple modules of same or different type are allowed (e.g. different pairs of headphones).  The only restriction is that the reproduction channel number has to match with the channel count of the output group(s).


### Optional reproduction module parameters
```
Description = Some informative description of this reproduction module instance
Enabled = true
RecordInputEnabled = false
RecordInputFileName = reproduction_in.wav
RecordInputBaseFolder = recordings/MyReproduction
RecordOutputEnabled = false
RecordOutputFileName = reproduction_out.wav
RecordOutputBaseFolder = recordings/MyReproduction
```
<!---
TODO: PSC: Still required?
InputDetectorEnabled = false
OutputDetectorEnabled = false
-->
>Note: until version 2018a, the record input / output file was only controlled by a file path key named `RecordInputFilePath` / `RecordOutputFilePath`. The file name and base folder has been introduced in 2018b. Now, the folder and file name can be modified during runtime. 

Those parameters are similar to the [optional parameters](#optional-rendering-module-parameters) for rendering modules. Reproduction modules can be *enabled* and *disabled*. Other then for the renderers, the output **and** input stream can be recorded.