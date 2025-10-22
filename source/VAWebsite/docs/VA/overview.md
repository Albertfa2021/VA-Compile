# Overview

## Real-time philosophy for VR applications
Virtual Acoustics (VA) creates audible sound from a purely virtual situation. To do so, it uses digital input data that is pre-recorded, measured, modelled or simulated. However, VA creates dynamic auditory worlds that can be interactively endevoured, because it accounts for modifications of the virtual situation. In the simplest case, this means that sound sources and listeners can move freely, and sound is changed accordingly. This real-time auralization approach can only be achieved, if certain parts of the audio processing are updated continuously and fast. We call this audio rendering, and the output is an audio stream that represents the virtual situation. For more complex situations like rooms or outdoor worlds, the sound propagation becomes highly relevant and very complex. VA uses real-time simulation backends or simplified models to create a physics-based auditory impression. If update rates are undergoing certain perceptive thresholds, this method can be readily used in Virtual Reality applications.

>NOTE: It is also possible to do [offline renderings](documentation/recording.md#offline-rendering-and-capturing) where the audio output is written to the hard drive.

## General auralization concept
The auralization in VA basically is a three step process:

1. Description of a [virtual scene](#virtual-scene) including moving sources and receivers
2. [Rendering](#audio-rendering) a **virtual** *N*-channel audio signal based on the scene
3. [Audio reproduction](#reproduction-modules) converting the virtual *N*-channel audio to an *M*-channel audio which can be playback by physical hardware

### Virtual scene
The starting point of every auralization in VA is setting up a virtual scene. All scenes are composed of at least two freely movable audio objects, namely one source and one receiver. Sources represent objects in the virtual world emitting sound. They are represented by point sources and can have a directivity for direction-dependent sound emission. On the other hand, the receiver represents the position for which the sound is actually rendered. Also, the receiver's orientation is important in order to render a spatial impression (e.g. using binaural filtering via HRTFs). In many cases, the receiver represents the listener within the virtual world. However, it can also represent other entities like the center of a loudspeaker array or a virtual microphone. Additional objects and meta data can be used for more complex scenes. This could include the geometry of the scene to consider reflections, scattering and/or diffraction.

Note, that while the complexity of the scene (inlcuding the number of sources and receivers) is not restricted, it is indirectly limited by the computational power. Further details on the scene concept of VA are given [here](documentation/scene.md).

### Audio rendering
Based on the virtual scene described above, an N-channel audio stream is rendered. The rendering process is done for each source-receiver pair individually and then superposed. VA provides multiple rendering modules, each considering different acoustic effects (e.g. free-field assumption vs. consideration of a room geometry for reflections). Using the same scene, multiple renderers can be run in parallel to combine certain acoustic effects, thereby increasing the computational load. For a full list of available rendering modules and additional details, check the respective [section](documentation/rendering.md) in the documentation.

Typically, the output stream of a renderer is already spatialized. VA supports three types of **spatial econding**: *Binaural filtering*, *Ambisonics* and *VBAP*. Some renderers use a fixed type of encoding, for others it is configurable. Generally, the number of output channels depends on the spatial encoding. Using binaural filtering, there are always 2 output channels. For ambisonics encoding, the number of channels depends on the respective ambisonics order. For VBAP encoding, the number of channels depends on the referenced loudspeaker setup.

### Reproduction modules
As mentioned above, the output stream of the audio renderer is indicated as *virtual* as it is decoupled from actual physical hardware. Generally, it might be desired to route the same renderer output to multiple sets of hardware (e.g. multiple headphones). Additionally, there might be more than one way to reproduce the respective signal at the listeners ears. For example, binaural signals could be reproduced via headphones or via loudspeakers using cross-talk cancellation (CTC). Finally, using renderers with ambisonics-encoded output signals, it is not possible to simply route the signal to a set of hardware channels. In this case, the output must be decoded.

For those reasons, VA uses the concept of reproduction modules. Similar to the rendering modules, multiple can be active in parallel, again increasing the computational load. The output of one audio renderer can be routed to multiple reproduction modules. However, the number of output channels of the renderering module must match the number of input channels of the connected reproduction module. More details are given in the respective [documentation section](documentation/reproduction.md).

## Server and client concept

The VA frame is designed using a server/client structure. The main application is the `VAServer`, which processes the auralization steps discussed above. There are multiple client applications which can connect to this server in order to adjust the [virtual scene](#virtual-scene), e.g. change the position of a sound source. Those client applications are called `Bindings` and are available for the following programming languages:

<!--- If we rather want a table:
| Programming language | Binding            | Included in installer? |
|----------------------|--------------------|------------------------|
| C++                  | *VANet*            | Yes                    |
| C#                   | *VANetCSWrapper*   | Yes                    |
| Matlab               | *VAMatlab*         | Yes                    |
| Python               | *VAPython*         | Yes                    |
| Unity                | *VAUnity*          | Yes                    |
| Unreal               | *UnrealVAPlugin*   | No: Externally developed by [Visual Computing Institute](https://devhub.vr.rwth-aachen.de/VR-Group/unreal-development/plugins/unreal-va-plugin) |
-->

- C++ (*VANet*)
- C# (*VANetCSWrapper*)
- Matlab (*VAMatlab*)
- Python (*VAPython*)
- Unity (*VAUnity*)
- Unreal (*UnrealVAPlugin* hosted by [Visual Computing Institute](https://devhub.vr.rwth-aachen.de/VR-Group/unreal-development/plugins/unreal-va-plugin){target="_blank"})


## Real-time auralization: Some technical notes
Input-output latency is crucial for any interactive application. VA tries to achieve minimal latency wherever possible, because latency of subsequent components add up. As long as latency is kept low, a human listener will not notice small delays during scene updates, resulting in a convincing live system, where interaction directly leads to the expected effect (without waiting for the system to process).

VA supports real-time capability by establishing flexible data management and processing modules that are lightweight and handle updates efficiently. For example, the FIR filtering modules use a partitioned block convolution resulting in update latencies (at least for the early part of filters) of a single audio block - which usually means a couple of milliseconds. Remotely updating long room impulse responses using Matlab can easily hit 1000 Hz update rates, which under normal circumstances is about three times more a block-based streaming sound card provides - and by far more a dedicated graphics rendering processor achieves, which is often the driving part of scene modifications.

However, this comes at a price: VA is not trading computational resources over update rates. Advantage is taken by improvement in the general purpose processing power available at present as well as more efficient software libraries. Limitations are solely imposed by the provided processing capacity, and not by the framework. Therefore, VA will plainly result in audio dropouts or complete silence, if the computational power is not sufficient for rendering and reproducing the given scene with the configuration used. Simply put, if you request too much, VA will stop auralizing correctly. Usually, the number of paths between a sound source and a sound receiver that can effectively be processed can be reduced to an amount where the system can operate in real-time. For example, a single binaural free field rendering can calculate roughly up to 20 paths in real-time on a modern PC, but for room acoustics with long reverberation times, a maximum of 6 sources and one listener is realistic (plus the necessity to simulate the sound propagation filters remotely). If reproduction of the rendered audio stream also requires intensive processing power, the numbers go further down. 