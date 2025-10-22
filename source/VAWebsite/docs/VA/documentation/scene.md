# Scene handling

In VA, everything that is not static is considered part of a dynamic scene. All sound sources, sound receivers, underlying geometry and source/receiver directivities are potentially dynamic and therefore are stored and accessed using a history concept. They can be modified, however, during lifetime. [Renderers](rendering.md) are picking up modifications and react upon the new state, for example, when a sound source is moved or a sound receiver is rotated. Updates are triggered asynchronously by the user or by another application and can also be synchronized ensuring that all signals are started or stopped within one audio frame.

## Sound sources and receivers
This section explains the interfaces which are valid for sources and receiver. The respective function calls are quasi identical. In the examples below, those functions are called for a sound source. The respective receiver functions follow the same syntax - just substitute `source` with `receiver`. Functions that are specific to [sound sources](#sound-sources) or [receivers](#sound-receivers) are discussed further down.

### Creation and deletion

Sound sources can be created (optionally assigning a name) using
```
S = va.create_sound_source()
S = va.create_sound_source( 'Car' )
```
`S` will contain a unique numerical identifier which is required to modify or delete the sound source.

A list of all available sound sources is returned by
```
source_ids = va.get_sound_source_ids()
```
Sound sources can be deleted with
```
va.delete_sound_source( S )
```

>IMPORTANT: A sound source/receiver can only be auralized if it has been **placed somewhere in 3D space**. Otherwise it remains in an **invalid state**.

### Position and orientation
Thus, it is required to specify a position as a three-dimensional vector:
```
va.set_sound_source_position( S, [ x y z ] )
```
The respective orientation can either be set using quaternions or a view and up vector
```
va.set_sound_source_orientation( S, [ a b c d ] )
va.set_sound_source_orientation_view_up( S, [ vx vy vz ], [ ux uy uz ] )
```
It is also possible to set both, position and orienation (quaternions), at once using
```
va.set_sound_source_pose( S, [ x y z ], [ a b c d ] )
```
The corresponding getter functions are
```
p = va.get_sound_source_position( S )
q = va.get_sound_source_orientation( S )
[ p, q ] = va.get_sound_source_pose( S )
[ v, u ] = va.get_sound_source_orientation_view_up( S )
```
with `p = [x y z]'`, `q = [a b c d]'`, `v = [vx vy vz]'`, and `u = [ux uy uz]'`, where `'` symbolizes the vector transpose.

### Directivity
Generally, both sound sources and receiver can have a directivity. For sound sources, VA expects a energetic directivities with a one-third octave band resolution. For sound receivers, VA expects an HRTF data set. This is relevant, if the receiver represents a human listener and a binaural synthesis is included in the renderer / reproduction processing chain.

Sound sources can be assigned a directivity with a numerical identifier (called `D` here) by 
```
va.set_sound_source_directivity( S, D )
D = va.get_sound_source_directivity( S )
```
More information on handling directivities can be found [below](#directivities-including-hrtfs).

### Muting
To mute (`true`) and unmute (`false`) a source, type
```
va.set_sound_source_muted( S, true )
mute_state = va.get_sound_source_muted( S )
```

<!-- TODO: The interface functions below exist but the respective modes are not used within the renderers. So the respective documentation does not make sense yet. Later, we also could think of moving this to a general "auralization mode" section.
PPA: Agree

### Auralization mode
The auralization mode of a sound source can be modified and returned using
```
va.set_sound_source_auralization_mode( S, '+DS' )
am = va.get_sound_source_auralization_mode( S )
```
This call would, for example, activate the direct sound. Other variants include 
```
va.set_sound_source_auralization_mode( S, '-DS' )
va.set_sound_source_auralization_mode( S, 'DS, IS, DD' )
va.set_sound_source_auralization_mode( S, 'ALL' )
va.set_sound_source_auralization_mode( S, 'NONE' )
va.set_sound_source_auralization_mode( S, '' )
```
-->

<!-- TODO: PSC: I don't think prototyping for sources/receivers is used yet, and I don't think we need that.
PPA: Agree
### Prototype parameters
Specific parameter structs can be set or retrieved. They depend on special features and are used for prototyping, for example, if sound sources require additional values for new renderers. 
```
va.set_sound_source_parameters( S, params )
params = va.get_sound_source_parameters( S )
```
-->

## Sound sources

### Source power
To control the level of a sound source, assign the sound power in **watts**
```
va.set_sound_source_sound_power( sound_source_id, P )
P = va.get_sound_source_sound_power( sound_source_id )
```
The default value of **31.67 mW (105 dB re 10e-12 W)** corresponds to **1 Pa (94.0 dB SPL re 20e-6 Pa ) in a distance of 1 m** for spherical spreading. The final gain of a sound source is linked to the input signal, which is explained below. However, a **digital signal with an RMS value of 1.0** (e.g., a sine wave with peak value of sqrt(2)) will retain 94 dB SPL @ 1m. A directivity may alter this value for a certain direction. Here, two approaches are common. A power calibrated directivity database will not change the overall excited sound power of the sound source when integrating over a hull, which may influence the spectrum to the front. A referenced directivity will not affect the spectrum of reference direction, usually the front - the frequency values all become 1.0 (0 dB). 

### Source signal
In contrast to all other sound objects, sound sources can be assigned a [signal source](#signal-sources). It feeds the sound pressure time series for that source and is referred to as the signal (speech, music, sounds). See below for more information on signal sources. The combination with the sound power and the directivity (if assigned), the signal source influences the time-dependent sound emitted from the source. For a calibrated auralization, the combination of the three components have to match physically.
```
va.set_sound_source_signal_source( sound_source_id, signal_source_id )
```

## Sound receivers

### Anthropometric data
Some renderers allow to use anthropometric data to individualize the utilized HRTFs for the binaural filtering, more specifically by adjusting the respective ITD.

The anthropometric parameters of a receiver can be adjusted using a specific key/value layout combined under the key `anthroparams`. All parameters are provided in units of meters
```
in_struct = struct()
in_struct.anthroparams = struct()
in_struct.anthroparams.headwidth = 0.12
in_struct.anthroparams.headheight = 0.10
in_struct.anthroparams.headdepth = 0.15
va.set_sound_receiver_parameters( sound_receiver_id, in_struct )
```

The current parameter values can be displayed using
```
params = va.get_sound_receiver_parameters( sound_receiver_id, struct() )
disp( params.anthroparams )
```

### Head-above-torso orientation
The VA interfaces provides some special features for receivers that are meaningful only in binaural technology. The head-above-torso orientation (HATO) of a human listener can be set and received as quaternion by the methods
```
va.set_sound_receiver_head_above_torso_orientation( sound_receiver_id, [ a b c d ] )
q = va.get_sound_receiver_head_above_torso_orientation( sound_receiver_id )
```
In common datasets like the **FABIAN HRTF** dataset (can be obtained from the [OpenDAFF project website](http://www.opendaff.org/){target="_blank"}), only a certain range within the horizontal plane (around positive Y axis according to right-handed, Cartesian OpenGL coordinates) is present, that accounts for simplified head rotations with a fixed torso. Many listening experiments are conducted in a fixed seat and the user's head orientation is tracked. Here, a HATO HRTF appears more suitable, at least if an artificial head is used.

### Real-world position and orientation

In Virtual Reality applications with loudspeaker-based setups, user motion is typically tracked inside a specific area. Some reproduction systems require knowledge on the exact position of the user's head and torso to apply adaptive sweet spot handling (like cross-talk cancellation). The VA interface therefore includes some receiver-oriented methods that extend the virtual pose with a so called real-world pose. Hardware in a lab and the user's absolute position and orientation (pose) should be set using one of the following setters
```
va.set_sound_receiver_real_world_pose( sound_receiver_id, [ x y z ], [ a b c d ] )
va.set_sound_receiver_real_world_position_orientation_vu( sound_receiver_id, [ x y z ], [ vx vy vz ], [ ux uy uz ] )
```
Corresponding getters are
```
[ p, q ] = va.get_sound_receiver_real_world_pose( sound_receiver_id )
[ p, v, u ] = va.get_sound_receiver_real_world_position_orientation_vu( sound_receiver_id )
```
Also, HATOs are supported (in case a future reproduction module makes use of HATO HRTFs)
```
va.set_sound_receiver_real_world_head_above_torso_orientation( sound_receiver_id, [ x y z w ] )
q = va.set_sound_receiver_real_world_head_above_torso_orientation( sound_receiver_id )
```

### Virtual vs. real-world pose

The aim of this section is to give a better understanding of the differences between virtual and real-world pose of the receiver:<br></br> 
The virtual pose is used to represent the receiver within the virtual world. Generally, the virtual source and receiver poses are used by the [rendering modules](rendering.md) to create the audio, most importantly applying the respective sound propagation effects. In this context, it is important to understand that the virtual receiver does not necessarily represent a listener. In fact, this is just the case if the utilized renderer has a binaural output. For Ambisonics or VBAP encoded signals, it represents the center of the loudspeaker array.

Additionally, the real-world pose of the listener is required for certain [reproduction modules](reproduction.md). Typically, this represents the position and orientation within a loudspeaker array. The most common example is the reproduction of binaural signals via a loudspeaker array using [Crosstalk-Cancellation (CTC)](reproduction.md#nctc). The second example are binaural mixdown reproduction modules ([BinauralMixdown](reproduction.md#binauralmixdown) and [BinauralAmbisonicsMixdown](reproduction.md#binauralambisonicsmixdown)). These allow to renderer a binaural signal based on a signal created for a *virtual* loudspeaker array which can played via headphones.

Here is a summary of the examples above:

- L<sub>v</sub>: Listener in virtual world
- C<sub>v</sub>: Center of loudspeaker array in virtual world
- L<sub>RW</sub>: Listener in real world (e.g. standing within loudspeaker array)

| Renderer output	| Reproduction type					| Virtual pose	| Real-world pose	|
|-------------------|-----------------------------------|:-------------:|:-----------------:|
| Binaural			| Headphones       					| L<sub>v</sub> | -                 |
| Binaural       	| CTC => Loudspeakers				| L<sub>v</sub> | L<sub>RW</sub>    |
| Ambisonics        | HOA decoding => Loudspeakers		| C<sub>v</sub> | -                 |
| VBAP              | Loudspeakers						| C<sub>v</sub> | -                 |
| Ambisonics/VBAP  	| Binaural mixdown => Headphones	| C<sub>v</sub> | L<sub>RW</sub>    |

## Signal sources
Sound signals or signal sources represent the sound pressure time series that are emitted by a source. Some are **unmanaged** and are directly available, others have to be created. To get a list with detailed information on currently available signal sources (including those created at runtime), type
```
va.get_signal_source_infos()
```
In contrast to sources and receiver, signal sources have string-based identifiers.

### Audio files
Audio files that can be attached to sound sources are usually single channel anechoic WAV files. In VA, an audio clip can be loaded as a buffer signal source with special control mechanisms. It supports macros and uses the search paths to locate a file. Using relative paths is highly recommended. Two examples are provided in the following:
```
signal_source_id = va.create_signal_source_buffer_from_file( 'filename.wav' )
demo_signal_source_id = va.create_signal_source_buffer_from_file( '$(DemoSound)' )
```
The `DemoSound` macro points to the 'Welcome to Virtual Acoustics' anechoically recorded file in WAV format, which resides in the common `data` folder. Make sure that the VA application can find the common `data` folder, which is also added as a search path in the default configurations.

Now, the signal source can be attached to a sound source using
```
va.set_sound_source_signal_source( sound_source_id, signal_source_id )
```

Any buffer signal source can be started, stopped and paused. Also, it can be set to looping or non-looping mode (default). 
```
va.set_signal_source_buffer_playback_action( signal_source_id, 'play' )
va.set_signal_source_buffer_playback_action( signal_source_id, 'pause' )
va.set_signal_source_buffer_playback_action( signal_source_id, 'stop' )
va.set_signal_source_buffer_looping( signal_source_id, true )
```

To receive the current state of the buffer signal source, use
```
playback_state = va.get_signal_source_buffer_playback_state( signal_source_id )
```

### Audio input channels
Input channels from the sound card can be directly used as signal sources (microphones, electrical instruments, etc) and are **unmanaged** (can not be created or deleted). All channels are made available individually on startup and are integrated as list of signal sources. The respective IDs are `'audioinput1'` for the first channel, and so on. 
```
va.set_sound_source_signal_source( sound_source_id, 'audioinput1' )
```

### Jet engine
VA also provides a parameter-controlled signal source representing a jet engine. Its implementation is based on the book *Designing Sound* by *Andy Farnell* from 2010. It can be created using
```
JE = va.create_signal_source_prototype_from_parameters( struct( 'class', 'jet_engine' ) );
```

It is possible to set the rotational speed by handing a numerical value in rounds per minutes. In the following example, the corresponding variable is called `JetRPM`:
```
va.set_signal_source_parameters( JE, struct( 'rpm', JetRPM ) )
```
Note, that the RPM value is limited between 1000 and 5000.

### Text-to-speech (TTS)
The TTS signal source allows to generate speech from text input. Because it uses the commercial *CereProc's CereVoice* third party library, it is not included in the VA package for public download. However, if you have access to the *CereVoice* library and can build VA with TTS support, this is how it works in `Matlab`: 
```
tts_signal_source = va.create_signal_source_text_to_speech( 'Heathers beautiful voice' )
tts_in = struct();
tts_in.voice = 'Heather';
tts_in.id = 'id_welcome_to_va';
tts_in.prepare_text = 'welcome to virtual acoustics';
tts_in.direct_playback = true;
va.set_signal_source_parameters( tts_signal_source, tts_in )
```
<!--
TODO: merge this example into VA and point to it https://git.rwth-aachen.de/ita/toolbox/-/blob/master/applications/VirtualAcoustics/VA/itaVA_example_text_to_speech.m
-->

### Other
VA also provides specialized signal sources which can not be covered in detail in this introduction. Please refer to the source code for proper usage.

## Directivities (including HRTFs)
Sound source and receiver directivities are usually made available as a file resource including multiple directions on a sphere for far-field usage. VA currently supports the *OpenDAFF* format with time domain and magnitude spectrum content type. The magnitude spectra are used for source directivities with a one-third octave band resolution. The time domain format is used for receiver directivities, i.e. HRIR / HRTF data sets.

Directivities can be loaded with
```
directivity_id = va.create_directivity_from_file( 'my_source_directivity.daff' )
```

VA ships with the [ITA artificial head HRTF dataset](https://www.akustik.rwth-aachen.de/go/id/pein/lidx/1){target="_blank"} (actually, the DAFF exports this dataset as HRIR in time domain), which is available under Creative Commons license for academic use. The default configuration files include this HRTF dataset as `DefaultHRIR` macro. Make sure that the VA application can find the common `data` folder, which is also added as an include path in default configurations. Then, the directivity object can be created using
```
directivity_id = va.create_directivity_from_file( '$(DefaultHRIR)' )
```

Additionally, VA provides example source directivities for a trumpet and a singer. The respective macros are `Trumpet` and `HumanDir`.


## Homogeneous medium
As introduced in the configuration section, most rendering and reproduction modules work with a [homogeneous medium](configuration.md#homogeneous-medium). The respective parameters can also be adjusted during runtime. The respective setter and getter functions are introduced here.

Speed of sound in m/s
```
va.set_homogeneous_medium_sound_speed( 344.0 )
sound_speed = va.get_homogeneous_medium_sound_speed()
```

Temperature in degree Celsius
```
va.set_homogeneous_medium_temperature( 20.0 )
temperature = va.get_homogeneous_medium_temperature()
```

Static pressure in Pascal
```
va.set_homogeneous_medium_static_pressure( 101325.0 )
static_pressure = va.get_homogeneous_medium_static_pressure()
```

Relative humidity in percentage (ranging from 0.0 to 100.0 or above)
```
va.set_homogeneous_medium_relative_humidity( 75.0 )
humidity = va.get_homogeneous_medium_relative_humidity()
```

<!-- TODO: The following parameters are not used, so the interfaces are not part of the documentation for now
Medium shift / 3D wind speed in m/s
```
va.set_homogeneous_medium_shift_speed( [ x y z ] )
shift_speed = va.get_homogeneous_medium_relative_humidity()
```

Prototyping parameters (user-defined struct)
```
va.set_homogeneous_medium_parameters( medium_params )
medium_params = va.get_homogeneous_medium_relative_humidity()
```
-->

## Solving synchronization issues
Scripting languages like Matlab are problematic by nature when it comes to timing: evaluation duration scatters unpredictability and timers are not precise enough. This becomes a major issue when, for example, a continuous motion of a sound source should be performed with a clean Doppler shift. A simple loop with a timeout will result in audible motion jitter as the timing for each loop body execution is significantly diverging. Also, if a music band should start playing at the same time and the start is executed by subsequent scripting lines, it is very likely that they end up out of sync.

### High-performance timer
To avoid timing problems, the `VAMatlab` binding provides a high-performance timer that is implemented in C++. It should be used wherever a synchronous update is required, mostly for moving sound sources or sound receivers. An example for a properly synchronized update loop at 60 Hertz that incrementally drives a source from the origin into positive X direction until it is 100 meters away:
```
S = va.create_sound_source()

va.set_timer( 1 / 60 )
x = 0
while( x < 100 )
	va.wait_for_timer;
	va.set_sound_source_position( S, [ x 0 0 ] )
	x = x + 0.01
end

va.delete_sound_source( S )
```

### Synchronizing multiple updates
VA can execute updates synchronously in the granularity of the block rate of the audio stream process. Every scene update will be withhold until the update is unlocked. This feature is mainly used for simultaneous playback start.
```
va.lock_update
va.set_signal_source_buffer_playback_action( drums, 'play' )
va.set_signal_source_buffer_playback_action( keys, 'play' )
va.set_signal_source_buffer_playback_action( base, 'play' )
va.set_signal_source_buffer_playback_action( sax, 'play' )
va.set_signal_source_buffer_playback_action( vocals, 'play' )
va.unlock_update
```

It is also useful for uniform movements of spatially static sound sources (like a vehicle with four wheels). However, locking updates will inevitably lock out other clients (like trackers) and should be released as soon as possible. 
```va.lock_update
va.set_sound_source_position( wheel1, p1 )
va.set_sound_source_position( wheel2, p2 )
va.set_sound_source_position( wheel3, p3 )
va.set_sound_source_position( wheel4, p4 )
va.unlock_update
```

## Tracking
VA does not support tracking internally but facilitates the integration of tracking devices to update VA entities. For external tracking, the `VAMatlab` project currently supports [**NaturalPoint's OptiTrack**](https://optitrack.com/){target="_blank"} and [**Advanced Realtime Tracking**](https://ar-tracking.com/de){target="_blank"} (AR-Tracking) devices to be connected to a server instance. It can automatically forward rigid body poses (head and torso, separately) to one sound receiver and one sound source. Another possibility is to use an HMD such as **Oculus Rift and HTC Vive** and update VA through **Unity** or **Unreal**.

### OptiTrack or AR-Tracking via VAMatlab

#### Virtual receiver pose

To connect a rigid body to a VA sound receiver (here, the receiver ID is 1), use
```
va.set_tracked_sound_receiver( 1 )
```

If the rigid body index should be changed (e.g., to index 3 for head and 4 for torso), use
```
va.set_tracked_sound_receiver_head_rigid_body_index( 3 )
va.set_tracked_sound_receiver_torso_rigid_body_index( 4 )
```

The head rigid body (rb) can also be locally transformed using a translation and (quaternion) rotation method, e.g., if the rigid body barycenter is not between the ears or is rotated against the default orientation:
```
va.set_tracked_sound_receiver_head_rb_trans( [ x y z ] )
va.set_tracked_real_world_sound_receiver_head_rb_rotation( [ a b c d ] )
```

#### Real-world receiver pose

As explained [here](#real-world-position-and-orientation), some reproduction modules require specifying the real-world pose of the receiver. It can be controlled by the tracking system using the following methods:
```
va.set_tracked_real_world_sound_receiver( 1 )
va.set_tracked_real_world_sound_receiver_head_rigid_body_index( 3 )
va.set_tracked_real_world_sound_receiver_torso_rigid_body_index( 4 )
va.set_tracked_real_world_sound_receiver_head_rb_trans( [ x y z ] )
va.set_tracked_real_world_sound_receiver_head_rb_rotation( [ a b c d ] )
```

#### Source pose

For tracking a sound source, similar functions are available. Note that there is no real-world position for sound sources.
```
va.set_tracked_sound_source( 1 )
va.set_tracked_sound_source_rigid_body_index( 5 )
va.set_tracked_sound_source_rigid_body_translation( [ x y z ] )
va.set_tracked_sound_source_rigid_body_rotation( [x y z w ] )
```

#### Establish connection

After all tracking settings are adjusted, connect a tracking system to VA using

```matlab
va.connect_tracker
```
By default this will connect to an *OptiTrack* tracker that is running on the same machine and pushes to `localhost` network loopback device.

In case that the tracker is running on another machine, *OptiTrack* requires to both set the remote (in this example `192.168.1.2`) **and** the client machine IP (in this example `192.168.1.143`) like this
```matlab
va.connect_tracker( '192.169.1.2', '192.169.1.143' )
```

If using an *AR-Tracking* system, the client IP is not required. However, the tracking type has to be specified as third input:
```matlab
va.connect_tracker( 'localhost', '', 'ART' )
```

### HMD via VAUnity
To connect an HMD, set up a Unity scene and connect the tracked GameObject (usually the MainCamera) with a `VAUSoundReceiver` instance. For further details, please read the [README files of VAUnity](https://git.rwth-aachen.de/ita/VAUnity/blob/master/README.md){target="_blank"}.

