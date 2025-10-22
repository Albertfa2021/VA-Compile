# Rendering classes

The audio rendering in VA is done on the basis of source-receiver pairs. This means, in every processing step, one audio block is rendered for each source-receiver pair which are then superposed to create the final output stream. This, for example, allows muting sources and/or receivers during runtime. The output depends on the position and orientation of the respective sources and receivers as well as the utilized rendering class and its parameters. Generally, those classes are based on two rendering concepts: [Sound-path-based](#sound-path-based-renderers) and [FIR-based](#fir-based-renderers) rendering, which are introduced below.

In version `2022a`, there has been a revision of the internal concept of the renderers. One of the goals was to lose the restriction of sound-path-based renderers using a fixed type of spatial encoding, which was typically binaural filtering. Now many renderers allow the user to specify this type in the `VACore.ini` - i.e. binaural filtering, Ambisonics, VBAP. In the context of this revision, some renderers were marked [deprecated, removed or renamed](#deprecated-removed-or-renamed-renderers). For example, the `VBAPFreeField` renderer has become obsolete as the `FreeField` renderer allows the user to select a VBAP encoding. Some renderers are still restricted to a binaural processing but might be changed in the future.

In the following, an [overview](#rendering-class-overview) of all rendering classes will be given. Then, some [general rendering concepts](#general-rendering-concepts) are explained before discussing the individual rendering classes in detail. This starts with the "renderers" which are simply [routing](#routing-based-renderers) an input signal before introducing the [sound-path-based](#sound-path-based-renderers) and the [FIR-based](#fir-based-renderers) renderers.

----------

## Rendering class overview
### *N*-channel renderers
The following renderers are not restricted to a fixed number of channels. The number of channels can be set by the user and typically depends on the utilized spatial encoding technique (binaural synthesis, Ambisonics-encoding, VBAP-encoding).

| Class name {.table-column}                 | Basis {.table-column} | Description                               |
|--------------------------------------------|-----------------------|-------------------------------------------|
| [FreeField](#freefield)                    | Sound paths           | Basic but efficient renderer assuming free-field conditions. Applies source directivities, delay, spreading loss, Doppler shift and air attenuation. |
| [AirTrafficNoise](#airtrafficnoise)        | Sound paths           | Designed for aircraft flyovers. Considers direct sound and reflection at a flat ground. Applies source directivities, delay, spreading loss, Doppler shift, air attenuation and turbulence. In addition to working with straight sound paths (homogeneous medium), the rendering can be based on curved paths simulated with the [Atmospheric Ray Tracing](../../GA/art.md) framework (inhomogeneous, moving medium). |
| [GenericFIR](#genericfir)                  | FIR                   | Convolves a user-defined FIR filter for a configurable number of channels for each source-receiver pair. FIR filter can be updated in real-time using the binding interface. |
| [RoomAcoustics](#roomacoustics-not-public) | FIR                   | Uses the [ITASimulationScheduler](../../ITASimulationScheduler/index.md) backend to simulate room impulse responses (RIRs) for each source-receiver-pair. Also applies Doppler shift using a VDL and the number of leading zeros in the RIR. [NOT PUBLIC, since it requires [RAVEN](../../RAVEN/index.md)] |
| [AmbientMixer](#ambientmixer)              | Routing               | Not a renderer per se: Routes sound directly to all channels of reproductions and applies gains of sources. Can be used to add pre-rendered or pre-recorded ambient sounds to a scene. |

### Binaural renderers
All of the following renderers have a binaural two-channel output stream.

| Class name {.table-column} | Basis {.table-column} | Description                       |
|----------------------------|-----------------------|-----------------------------------|
| BinauralFreeField          | Sound paths           | Applies source directivities, delay, spreading loss and Doppler shift. Also allows to individualize the ITD depending on anthropometic data. |
| BinauralOutdoorNoise       | Sound paths           | Uses OutdoorNoise base renderer and processes incidence waves using binaural technology for spatialization. [BETA] |
| BinauralArtificialReverb   | FIR                   | Mixes reverberation at receiver side using reverberation time, room volume and surface area with a binaural approach. |
<!-- TODO: PSC: Not part of old documentation. Should we add those? Or rather wait until these are fixed
| PrototypeImageSource      | FIR           | 
| BinauralClustering        | Sound paths   |
-->

### Deprecated, removed or renamed renderers

Due to a new renderer structure introduced in version `2022a`, some renderers are marked as deprecated and might be removed in future versions.

| Class name                | Deprecated since  | Removed in    | Alternative class |
|---------------------------|:-----------------:|:-------------:|:-----------------:|
| AmbisonicsFreeField       | v2022a            | -             | FreeField         |
| VBAPFreeField             | v2022a            | -             | FreeField         |
| BinauralAirTrafficNoise   | v2022a            | -             | AirTrafficNoise   |
| BinauralRoomAcoustics     | -                 | v2022a        | RoomAcoustics     |

The following renderers were renamed to better reflect on their purpose.

| Old class name        | Renamed in    | New class name    |
|-----------------------|:-------------:|:-----------------:|
| PrototypeGenericPath  | v2022a        | GenericFIR        |

----------

## General rendering concepts
Most renderers share some general concepts. This includes a motion model for source and receiver and a variable-delay line allowing to model the Doppler shift. Both can be configures in the VACore.ini.

### Motion model
Since VA is primarily designed for real-time auralization, motion of sources and receivers is handed as discrete samples of position and orientation which are called motion keys. In this way, it is possible to handle arbitrary movement including data from tracking devices. However, this requires to interpolate the discrete data within VA to guarantee a smooth movement. For this purpose, the rendering modules in VA use a motion model which interpolates the motion keys using a triangular sliding window. As a result, the motion data used for the rendering process slightly varies from the actual data. Most important to mention is the slight delay being introduced to the motion data.

### Variable delay line
VA's rendering modules use variable delay lines (VDLs) to apply the Doppler shift to the input signals. A VDL is a long buffer with a reading curser which can be moved during run-time. In this case, the reading curser refers to the sound propagation delay. By not hard-switching between curser positions but virtually stretching/compressing the audio samples via interpolation, a Doppler shift is modelled.

### General configuration parameters

Most renderers share some general parameters which can be configures in the VACore.ini. Typically, all renderers already use proper default values for the parameters and should only be modified if really necessary.

| Parameter name {style="width:12rem"} |   Description     | Default {style="width:7rem"} |
|--------------------------------------|-------------------|------------------------------|
| `AdditionalStaticDelaySeconds`       | Allows adding a static delay to the input stream. This might be used to synchronize two reproduction systems. | 0.0 |
| `VDLSwitchingAlgorithm`              | Sets how the variable-delay line (VDL) reacts on a delay change. Can select between the following options: Directly move the read curser doing a hard `switch`, doing a `crossfade`, or interpolate between the positions, doing a `linear`, `cubicspline` or `windowedsinc` interpolation. | `cubicspline` |
| `SourceMotionModelNumHistoryKeys`    | Maximum number motion keys to be stored in the motion model buffer. | 1000 |
| `SourceMotionModelWindowSize`        | Size of one half of the triangular window in seconds. | 0.1 |
| `SourceMotionModelWindowDelay`       | Specifies how many seconds the window is delayed. Should be at least as high as `SourceMotionModelWindowSize`. | 0.1 |
| `SourceMotionModelLogInput`          | Enables logging the position and orientation keys that are inserted into the motion model. | false |
| `SourceMotionModelLogOutput`         | Enables logging the estimated position and orientation of the motion model | false |

Similar to the source motion model parameters there are parameters for the receiver, e.g. `ReceiverMotionModelWindowSize`.

----------

<!---
TODO: Link to at least one example per renderer.
-->

## Routing-based renderers
### AmbientMixer
The AmbientMixer is not a rendering module in the original sense. It does not consider source and receiver motion at all but only uses the sound source power to apply a gain to the input signal. Thus, the [general rendering concepts](#general-rendering-concepts) do not apply to this renderer. Its purpose is to add ambient sound to virtual scenes by combining it with additional rendering modules. Typically, those ambient sounds are taken from .wav files which are pre-recorded or pre-simulated.

>**Important**: Although the actual positions of source and receiver are not relevant to this renderer, an initial position must still be set so that the source and receiver become valid.

----------

## Sound-path-based renderers
For sound-path-based rendering in VA, each source-receiver pair holds a set of sound paths, which are processed separately. The number of considered sound paths depends on the respective rendering class. Instead of using FIR-based convolution, most acoustic parameters are applied using filterbanks based on one-third octave magnitude spectra (typically IIR filtering). This includes source directivities or air attenuation. Only for binaural rendering, a full FIR convolution of the rather short head-related impulse responses (HRIRs) is done.

### Common configuration parameters
Most sound-path-based renderers share the following VACore.ini parameters. Most importantly this includes the settings for the spatial encoding. Note, that for [binaural renderers](#binaural-renderers), the spatial encoding type cannot be changed by the user which makes the settings for ambisonics and VBAP obsolete.

| Parameter name {style="width:12rem"} | Description   | Default {style="width:7rem"} |
|--------------------------------------|---------------|------------------------------|
| `FilterBankType`                     | Filter bank type used to apply one-third octave band magnitude spectra to audio signal. Either allows FIR-filters with linear phase and spline interpolation (`fir_spline_linear_phase` / `FIR`) or IIR filters based on 10th order biquads (`iir_biquads_order10`) or Burg coefficient design of 4th (`iir_burg_order4`) or 10th order (`iir_burg_order10` / `IIR`). | `IIR` |
| `SpatialEncodingType`                | Specifies the encoding type used for spatialization of the signal at the receiver. Can either be set to `Binaural`, `Ambisonics` or `VBAP`. Depending on the setting selected here, additional parameters become relevant (see below). | `Binaural` |
| **Binaural**                         |
| `HRIRFilterLength`                   | Maximum length for binaural filtering. | 256 |
| **Ambisonics**                       |
| `AmbisonicsOrder`                    | Order for ambisonics encoding | 4 |
| **VBAP**                             |
| `VBAPLoudspeakerSetup`               | ID of loudspeaker setup (see [Output configuration](configuration.md#loudspeaker-setup-example)) including loudspeaker positions. | [REQUIRED] |
| `VBAPTriangulationFile`              | Filename including triangulation info for the loudspeaker setup. | [REQUIRED] |
| `VBAPCenterPos`                      | Center position of loudspeaker setup | 0, 0, 0 |
| `MDAPNumSpreadingSources`            | Number of spreading sources if using MDAP extension for VBAP (0 = disabled) | 0 |
| `MDAPSpreadingAngleDegrees`          | Spreading angle if using MDAP extension for VBAP (degrees) | 0.0 |

### FreeField
As the name suggests, this renderer assumes free-field conditions for the sound propagation based on the [homogeneous medium](configuration.md#homogeneous-medium). Therefore, it only considers a single, straight sound path per source-receiver pair which makes it extremely efficient. Based on the direction and length of this sound path, the following auralization parameters are evaluated and applied to the input signal:

- Source directivity
- Spherical spreading loss
- Propagation delay / Doppler shift
- Air attenuation (ISO 9613-1)
- Spatial encoding at the receiver (Binaural / Ambisonics / VBAP)

**Examples**

The following examples are based on the *VAMatlab* binding:

- Simple static scene
- Dynamic scene with a moving source
    - Real-time auralization
    - Offline auralization

### BinauralFreeField
This renderer has three major differences compared to the [FreeField renderer](#freefield-renderer). Most importantly it is restricted to a two-channel binaural output stream. Additionally, it does not support the consideration of air attenuation. However, the special feature of this renderer is to use individualized HRIRs adjusting the ITD. For this purpose, the receiver has to get additional [anthropometric data](scene.md#anthropometric-data) which can be applied while setting up the scene.

### AirTrafficNoise
In addition to the direct sound, this renderer considers a second sound path for the ground reflection. Assuming a flat ground at a certain altitude (typically *y* = 0), this sound path is calculated based on the image source method. The renderer is primarily designed for aircraft flyovers as the assumption of a half-space is quite reasonable for sound sources at high altitutdes. However, it can also be applied to other scenarios where this assumption is suitable (e.g. car pass-by*).

The special feature of this renderer is, that the sound paths can either be based on the homogeneous medium in VA or simulations by the [Atmospheric Ray Tracing (ART)](../../GA/art.md) framework. The latter considers an inhomogeneous, moving medium which leads to curved sound paths. This is particularly relevant for those flyover scenarios. However, it should be noted that the simulation has certain limitations cannot be applied to every combination of source-receiver positions (see *Limitations* section in [this paper](https://doi.org/10.1051/aacus/2021018){target="_blank"}).

Based the respective sound paths - either straight or curved - the following auralization parameters are evaluated and applied to the input signal:

- Source directivity
- Spreading loss
- Propagation delay / Doppler shift
- Air attenuation (ISO 9613-1)
- Turbulence (temporal amplitude-variations)
- Ground reflection factor
- Spatial encoding at the receiver (Binaural / Ambisonics / VBAP)

<sub>*When dealing with short distance scenarios like a car pass-by, it might be a good idea to disable the turbulence (see [auralization modes](control.md#global-auralization-mode)).</sub>

**Special configuration parameters**

| Parameter name {style="width:12rem"} | Description                                                               | Default {style="width:7rem"} |
|--------------------------------------|---------------------------------------------------------------------------|------------------------------|
| `MediumType`                         | Switch for medium type. Can either be `Homogeneous` or `Inhomogeneous`    | `Homogeneous` |
| `GroundPlanePosition`                | Altitude (*y*-value) of the ground plane in meters. Sources and receiver should **not be placed below the ground**! | 0.0 |
| `StratifiedAtmosphereFilepath`       | Only relevant for `Inhomogeneous` medium type. Used to point to a .json file describing the atmosphere parameters which can be created using the [ARTMatlab](../../GA/art.md#artmatlab) interface. Uses a default atmosphere if unset. | - |
| `ExternalSoundPathSimulation`        | Can be used to switch to external simulation. If activated, auralization parameters are not simulated in VA but must be set using a binding. | false |

**Examples**

The following examples of **aircraft flyovers** are based on the *VAMatlab* binding:

- Real-time auralization using homogeneous medium
- Offline auralization based on ART simulations
- Auralization based on external simulation

### BinauralOutdoorNoise [BETA]
This renderer is currently on a beta version. Documentation will be updated in the future...

<!---
The binaural outdoor noise renderer creates a two-channel binaural output stream based on a set of sound paths. It is created for outdoor scenarios (e.g. urban noise) and can be used together with the [pigeon app](../../GA/iem.md#download) which is based on the [image edge model](../../GA/iem.md). It is primarily designed for offline auralizations, i.e. non-real-time auralizations creating a .wav file.
-->


----------

## FIR-based renderers
FIR-based rendering considers all relevant sound paths - i.e. connecting a source with a receiver - using a single FIR filter. This makes sense, when the number of sound paths is extremely large (e.g. in room acoustics). A limitation, however, is that the individual acoustic properties cannot be separated anymore.

### Common configuration parameters
Most FIR-based renderers share the following VACore.ini parameter.

| Parameter name {style="width:12rem"} | Description                                   | Default {style="width:7rem"} |
|--------------------------------------|-----------------------------------------------|------------------------------|
| `NumChannels`                        | Number of output channels of the renderer (and the FIR-filter internally used).   | [REQUIRED]    |
| `MaxFilterLengthSamples`             | Length of impulse response (IR) in samples used by the convolution engine. Longer IRs will be cropped, shorter ones will be zero-padded. | 22100 |

### GenericFIR
This renderer convolves a user-defined FIR filter for a configurable number of channels for each source-receiver pair. Consequently, the source and receiver position are irrelevant for this renderer and the [general rendering concepts](#general-rendering-concepts) do not apply. The filters are set for each source-receiver pair using the respective binding and can be updated in real-time.
<!--
TODO: ...as shown in this [example](#TODO){target="_blank"}.
-->

>**Important**: Although the actual positions of source and receiver are not relevant to this renderer, an initial position must still be set so that the source and receiver become valid.

### BinauralArtificialReverb
<!---
TODO: @LAS check and rework this section
    FYI: I have renamed the parameter MaxReverbFilterLengthSamples to MaxFilterLengthSamples to match the base function. However, the old name still works for backwards compatibility.
-->
This renderer implements an efficient method to add a reverberation tail a virtual scene. It does not consider the direct sound or early reflections and should therefore be combined with other renderers. For each given reverberation time (defined for a corresponding frequency range), exponentially decaying room impulse responses (RIR) are created with increasing reflection density over time (~t²). Spatial information is applied separately to each inserted impulse by randomly selecting a HRTF. Eventually, to generate one broadband RIR each frequency dependent RIR is combined in the frequency domain by using low pass, band pass and high pass filters. Parameters regarding the position and angle can be chosen to control when a new artificial reverberation filter (new random directions) should be generated.

**Special configuration parameters**

Note, that the `NumChannels` parameter is obsolete since this renderer's output is restricted to a two-channel binaural stream.

| Parameter name {style="width:12rem"} | Description                                                                       | Default {style="width:12rem"} |
|--------------------------------------|-----------------------------------------------------------------------------------|-------------------------------|
| `MaxFilterLengthSamples`             | see [common configuration parameters](#common-configuration-parameters-1)         | 2 * *f*<sub>sample</sub>      |
| `ReverberationTimes`                 | Frequency-dependent reverberation times in seconds. Either enter 3 values for low, mid, high or 8 values for octave bands.    | 1, 0.71, 0.3      |
| `RoomVolume`                         | Volume of the considered room [m<sup>3</sup>].                                    | 200.0                         |
| `RoomSurfaceArea`                    | Surface area of the considered room [m<sup>2</sup>].                              | 88.0                          |
| `SoundPowerCorrectionFactor`         | As the sound power refers to the direct sound but this is not considered in this renderer, this linear damping factor is applied to the signal.   | 0.05 |
| `PositionThreshold`                  | Threshold for receiver translation in meters before filter update is requested.   | 1.0                           |
| `AngleThresholdDegree`               | Threshold for receiver rotation in degrees before filter update is requested.     | 30.0                          |
| `TimeSlotResolution`                 | Time resolution used in filter synthesis.                                         | 0.005                         |
| `MaxReflectionDensity`               | Maximum allowed reflection density [1/s] used for filter synthesis.               | 12000.0                       |
| `ScatteringCoefficient`              | Average scattering coefficient of the room.                                       | 0.1                           |

**Examples**

TODO...


### RoomAcoustics [NOT PUBLIC]
This renderer is designed for real-time auralizations of room acoustic scenarios using physics-based simulations. For each source-receiver pair, a room impulse responses (RIR) is requested frequently. The respective requests are scheduled and processed by the [ITASimulationScheduler](../../ITASimulationScheduler/index.md). While not all simulation requests might actually be executed, a real-time processing is feasible if the respective update rate is high enough.
To increase the efficiency, the RIR is split into **direct sound**, **early reflections** and **late reverberation** which are simulated separately. On the one hand this allows, to have higher update rates for the early parts of the RIR which are most crucial for the localization of sound sources. On the other hand, the individual parts of the RIR can be en-/disabled in real-time which is a nice feature for demonstrations.

Currently, the only available room acoustic simulation backend for the ITASimulationScheduler is [RAVEN](../../RAVEN/index.md). RAVEN is **not** open-source but available for [academic purposes](../../RAVEN/index.md#access). Thus, the public version of VA does not include this renderer. In future versions, the ITASimulationScheduler might support additional simulation backends. Then, this renderer will become public (but still not include RAVEN).

**Special configuration parameters**

| Parameter name {style="width:12rem"} | Description                                                                       | Default {style="width:12rem"} |
|--------------------------------------|-----------------------------------------------------------------------------------|-------------------------------|
| `SimulationConfigFilePath`           | Points to the .json configuration file for the *ITASimulationScheduler*. This includes the setup of the underlying simulation engine (e.g. room geometry file).  | `VASimulationRAVEN.json`  |

**Examples**

TODO...
