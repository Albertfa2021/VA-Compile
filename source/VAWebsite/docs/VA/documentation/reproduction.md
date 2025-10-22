# Reproduction classes

As introduced in the [overview](../overview.md#reproduction-modules), the reproduction modules link between the rendering modules and the physical hardware.
VA provides different classes each being useful for different combinations of renderer and hardware setups.



In the following, an overview of all reproduction classes will be given.
Then, the individual reproduction classes are discussed in detail.
This starts with [general reproduction](#general-reproduction) classes which are not restricted to a specific hardware type, followed by classes designed for [headphone-based](#headphone-based-reproduction) and [loudspeaker-based](#loudspeaker-based-reproduction) reproduction.

## Reproduction class overview

| Class name                                                | Input stream {style="width:9rem"} | Output stream {style="width:9rem"} | Description                               |
|-----------------------------------------------------------|-----------------------------------|------------------------------------|-------------------------------------------|
| [Talkthrough](#talkthrough)                               | channel-based                     | same as input                      | Forwards the incoming stream directly to the audio hardware. Used a lot for plain headphone playback and channel-based renderings for loudspeaker setups. |
| [Headphones](#headphones)                                 | two-channel                       | equalized two-channel              | Forwards the incoming stream after euqalization of headphones (convolution with inverse HpTF). |
| [LowFrequencyMixer](#lowfrequencymixer)                   | arbitrary                         | variable                           | Mixes all channels or routes a specified channel to a single subwoofer or a subwoofer array. Handy for simple LFE support. |
| [NCTC](#nctc)                                             | binaural two-channel              | variable                           | Uses static or dynamic binaural cross-talk cancellation for arbitrary number of loudspeakers. |
| [HOA](#hoa)                                               | Ambisonics, any order             | variable                           | Calculates and applies gains for a loudspeaker setup using Higher Order Ambisonics methods (HOA). |
| [BinauralMixdown](#binauralmixdown)                       | channel-based                     | binaural, two-channel              | [UNDER CONSTRUCTION] Uses dynamic binaural technology with FIR filtering to simulate channel-based sound playback from a virtual loudspeaker setup. |
| [BinauralAmbisonicsMixdown](#binauralambisonicsmixdown)   | Ambisonics, any order             | binaural, two-channel              | Calculates and applies gains for a loudspeaker setup using Higher Order Ambisonics methods, then spatialises the directions of the loudspeakers using HRTFs. |

----------
<!--
TODO: Add at least one example per class
-->

## General reproduction

### Talkthrough
This class simply forwards the output stream of a renderer directly to the connected hardware channels without modifying it. Typical use-cases are plain headphone playback or channel-based renderings for loudspeaker setups, e.g. VBAP.

### LowFrequencyMixer
<!--
TODO: This is designed for subwoofers = loudspeakers. In theory, it could be used with headphones, but does this make sense? => Move to LS-based reproduction section?
 -->
This class is designed for the reproduction of low frequency content (e.g. using a subwoofer array). For this purpose, the channels from the rendering output are mixed using the sample based average. The result is forwarded to every attached hardware channel. The user can specifiy which renderer channels are used for the mixing process in the VACore.ini:

| Parameter name    | Description                                                                                   | Default   |
|-------------------|-----------------------------------------------------------------------------------------------|-----------|
| `MixingChannels`  | Specify the renderer's channel to be mixed. Channel numbers start with 1 (e.g. `1, 2, 3` ).   | 1         | <!-- Note, that the actual default is ALL but this seems to just use the first channel -->

----------

## Headphone-based reproduction
The following reproduction classes are designed for headphone reproduction. Thus, all have a **two-channel output** stream.

### Headphones
This class takes a two-channel input stream and applies an inverse Headphone-to-ear transfer function (HpTF) using FIR convolution to equalize the headphones.
The data must be provided using an audio file which is specified in the VACore.ini:

| Parameter name                    | Description                                                                                   | Default       |
|-----------------------------------|-----------------------------------------------------------------------------------------------|---------------|
| `HpIRInvFile`                     | Audio file including the impulse response of the inverse HpTF.                                | [REQUIRED]    |
| `HpIRInvCalibrationGain`          | Allows to specify an additional gain factor for calibration.                                  | 1.0           |
| `HpIRInvCalibrationGainDecibel`   | Do not use together with `HpIRInvCalibrationGain`. Specifies the same gain using a dB scale.  | 0.0           |
| `HpIRInvFilterLength`             | Can be used to change the utitlized filter length in samples for the given inverse HpTF. Might result in cropping parts of the filter. | Length of inverse HpTF |

### BinauralMixdown

>**Attention**: *This reproduction module is currently under construction and does not work yet*

The idea behind binaural downmixing is taking a **channel-based input** signal rendered for a loudspeaker array and reduce this to a **two-channel binaural output** signal which can be played via headphones. For this purpose, the loudspeaker array is placed in a virtual scene (free-field). Each loudspeaker can be interpreted as a virtual source with position and orientation defined in the hardware configuration. The real-world pose of the receiver is used to specify position and orientation of the listener within this array. Typically, it is assumed that the listener resides within the center of the array and only rotates its head. Then, the binaural output signal is created convolving each loudspeaker signal with the respective HRTF and mixing all signals. Playing this via headphones, the result should sound similar as standing within the actual loudspeaker array.

For this class the user has to specify two hardware output groups in the VACore.ini: The `Output` parameter refers to the real hardware used for the actual playback (typically headphones). The `VirtualOutput` refers to the "hardware setup" describing the virtual loudspeaker array.

| Parameter name                    | Description                                                                                       | Default       |
|-----------------------------------|---------------------------------------------------------------------------------------------------|---------------|
| `VirtualOutput`                   | ID of `Output` used as virtual loudspeaker array.                                                 | [REQUIRED]    |
| `TrackedListenerID`               | ID of the receiver (listener) which is used for the tracking. If set to -1, tracking is disabled. | 1             |
| `HRIRFilterLength`                | Filter length used for the HRIR convolution in samples.                                           | 128           |<!--TODO: change to 256? -->


### BinauralAmbisonicsMixdown
This class is similar to [BinauralMixdown](#binauralambisonicsmixdown) but does an ambisonics decoding before doing the actual downmix. Thus, it requires an **ambisonics encoded input** signal and has a **binaural two-channel output** stream. In addition, to the VACore.ini parameters of the BinauralMixdown class, there are parameters for the Ambisonics decoder:

| Parameter name            | Description                                                                                                           | Default       |
|---------------------------|-----------------------------------------------------------------------------------------------------------------------|---------------|
| `TruncationOrder`         | Maximum ambisonics order used for reproduction. Input channels referring to higher orders will not be driven.         | [REQUIRED]    |
| `ReproductionCenterPos`   | Center position of the virtual loudspeaker array. It is recommended to specify a 3D vector (e.g. `0, 0, 0`). Otherwise, VA will try calculating the center position based on the loudspeaker positions.   | AUTO  |
| `RotationMode`            | Rotations of the virtual listener can be effeciently implemented by rotating the scene in Ambisonics format instead of updating the HRIRs. In future versions "ViewUp" will include the virtual listeners translation, while "Quaternion" or "BFormat" only account for listener rotation.                                                  | BFormat       |
| `HRIR`                    | Path to the HRIR dataset that should be used for binaural downmixing.                                                 | DefaultHRIR   |
| `TrackingDelaySecond`     | Defer motion updates in the virtual scene by a given amount of seconds.                                               | 0             |
| `Decoder`                 | **to be implemented** Select one of the following decoder algorithms: `MMAD`, `EPAD`, `AllRAD`, `AllRAD+`.            | `MMAD`        |
| `UseRemax`                | **to be implemented** Use to enable ReMAx decoding (Only works up to ambisonics order 20).                            | false         |

<!--
PSC: It would be good if the parameters would be the same as for the HOA reproduction class.
     To me this class would be a combination of HOA and BinauralMixdown (see respective Issues in VACore git repo)
-->

----------

## Loudspeaker-based reproduction

### NCTC

This class uses a cross-talk cancellation (CTC) implementation to reproduce a **binaural two-channel input** with an arbitrary number of loudspeakers. For a dynamic CTC the real-world pose of the listener, i.e. position and orientation within the array, can be tracked. The following parameters can be specified in the VACore.ini:

| Parameter name            | Description                                                                                                   | Default               |
|---------------------------|---------------------------------------------------------------------------------------------------------------|-----------------------|
| `TrackedListenerID`       | ID of the receiver (listener) which is used for the tracking. If set to -1, tracking is disabled.             | 1                     |
| `UseTrackedListenerHRIR`  | File pointing to the inverse HpTF.                                                                            | false                 |
| `CTCDefaultHRIR`          | Used if `UseTrackedListenerHRIR` is set to false. Point to a HRIR daff file.                                  | -                     |
| `CTCFilterLength`         | Length in samples used for the CTC filters.                                                                   | 4096                  |
| `DelaySamples`            | Sets a delay in samples which is used for all channels.                                                       | `CTCFilterLength` / 2 |
| `RegularizationBeta`      | Regularization factor (&#946 &#8805 0, 0 = no regularization) used to stabilize the inversion during CTC filter calculation.  | 0.001 |
<!--
TODO: List of parameters which are not really used. 
PPA: As far as I can tell they can be omitted here. If they are used in the future we can add them.
Optimization = OPTIMIZATION_NONE
Algorithm = reg

Add sweet spot widing parameters?
PPA: We should ask MKO about this.
| `CrossTalkCancellationFactor`             | ???       | 1.0   |
| `WaveIncidenceAngleCompensationFactor`    | ???       | 1.0   |
-->

### HOA
<!--
TODO: Review by LVO
-->
This class takes an **ambisonics encoded signal as input** and decodes it for the connected loudspeaker array. The user can set a truncation order for the ambisonics decoding and select between different decoder algorithms in the VACore.ini:

| Parameter name            | Description                                                                                                           | Default               |
|---------------------------|-----------------------------------------------------------------------------------------------------------------------|-----------------------|
| `TruncationOrder`         | Maximum ambisonics order used for reproduction. Loudspeaker channels referring to higher orders will not be driven.   | [REQUIRED]            |
| `ReproductionCenterPos`   | Center position of loudspeaker array. It is recommended to specify a 3D vector (e.g. `0, 0, 0`). Otherwise, VA will try calculating the center position based on the loudspeaker positions.   | AUTO  |
| `Decoder`                 | Select one of the following decoder algorithms: `MMAD`, `EPAD`, `AllRAD`, `AllRAD+`.                                  | `MMAD`                |
| `UseRemax`                | Use to enable ReMAx decoding (Only works up to ambisonics order 20).                                                  | false                 |
| `VirtualOutput`           | The virtual loudspeaker array(s) is required when using AllRAD(+) decoders. The ambisonics signals will be decoded to the `VirtualOutput`, afterwards these virtual loudspeaker signals will be mapped to the real `Outputs` using VBAP. If you provided multiple `Outputs` you can specify one virtual output per real output, or use the same `VirtualOutput` for all `Outputs`.      | -                     |

<!--
TODO: List of parameters which are not used.
| `UseNearFieldCompensation`| ???                        | false                 |
LVO: not yet implemented, no need to document yet

Also since there can be multiple virtual outputs, shouldn't the parameter be called VirtualOutputs?
LVO: This goes back to the discussion if we actually want to allow multiple outputs for each reproduction. If yes, then it should be VirtualOutputs, otherwise no.
-->