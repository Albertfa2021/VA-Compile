# Controlling VA

Once your VA application is running as configured, you eventually want to create a virtual scene and modify its entities.
This scene control is possible via multiple interfaces from different other coding languages.
In general, the interfaces can be roughly grouped into two categories, [*scripting based*](#scripting-based-interfaces) and [*game engines*](#game-engine-interfaces).
The scripting based interfaces, as the name suggests, allow you to control VA through scripts.
These interfaces provide a list of methods which lets you trigger updates and control settings.
This often allows for more granular control over the scene.
The game engine control allows for a more playful interaction with VA through the use of game engines.

## Scripting based interfaces

Currently there are four well supported interfaces: Matlab, Python, C++ and C#.
Here, the access to VA is given via a VA object.
This can be a global object, as in the case for Python, or as local objects, like in Matlab or C#.
The VA object exposes the methods required for controlling VA.

Note, in the following the Matlab style interface will be used to show the examples.
The interface for the other languages are very similar and primarily differ in their naming convention (underscores vs camelCase).

The following example show most of the concepts behind the interface:

1. After creating the VA object, one has to connect to a VAServer via the `connect` method.
2. Entities can be created via `create_*` methods.
3. Properties can be accessed via `set_*` and `get_*` methods.

For further detail on how to create a scene see the [scene handling section](scene.md).

```matlab
% Create VA
va = VA;

% Connect to VA application (start the application first)
va.connect( 'localhost' )

% Reset VA to clear the scene
va.reset()

% Control output gain
va.set_output_gain( .25 )

% Create a signal source and start playback
X = va.create_signal_source_buffer_from_file( '$(DemoSound)' );
va.set_signal_source_buffer_playback_action( X, 'play' )
va.set_signal_source_buffer_looping( X, true );

% Create a virtual sound source and set a position
S = va.create_sound_source( 'VA_Source' );
va.set_sound_source_position( S, [ 2 1.7 2 ] )

% Create a listener with a HRTF and position him
L = va.create_sound_receiver( 'VA_Listener' );
va.set_sound_receiver_position( L, [ 0 1.7 0 ] )

H = va.create_directivity_from_file( '$(DefaultHRIR)' );
va.set_sound_receiver_directivity( L, H );

% Connect the signal source to the virtual sound source
va.set_sound_source_signal_source( S, X )
```

### Global gain and muting

To control the global input gains (sound card software input channels), use

```matlab
va.set_input_gain( 1.0 ) % for values between 0 and 1
```

To mute the input, use

```matlab
va.set_input_muted( true ) % or false to unmute
```

The same is true for the global output gain (sound card software output channels)

```matlab
va.set_output_gain( 1.0 )
va.set_output_muted( true ) % or false to unmute
```

### Global auralization mode

Making acoustic effects audible is one of the central aspects of auralization.
The deactivation of an acoustic phenomenon such as, for example, the spreading loss, can be useful for research and demonstration purposes.
This way, influences can be investigated intuitively.

VA provides a set of phenomena that can be toggled, and they are called auralization modes.
The auralization mode is combined in the renderers by a logical AND combination of global auralization mode, sound receiver auralization mode and sound source auralization mode.

```matlab
va.set_global_auralization_mode( '-DS' ) % ... to disable direct sound
va.set_global_auralization_mode( '+SL' ) % ... to enable spreading loss, e.g. 1/r distance law
```

The following phenomena can be toggled in VA:

| Name               | Aconym | Description                                                                                                               |
| ------------------ | ------ | ------------------------------------------------------------------------------------------------------------------------- |
| Direct sound       | DS     | Direct sound path between a sound source and a sound receiver                                                             |
| Early reflections  | ER     | Specular reflections off walls, that correspond to early arrival time of a complex source-receiver-pair.                  |
| Diffuse decay      | DD     | Diffuse decay part of a the arrival time of a complex source-receiver-pair. Mostly used in the context of room acoustics. |
| Source directivity | SD     | Sound source directivity function, the angle dependent radiation pattern of an emitter.                                   |
| Medium absorption  | MA     | Acoustic energy attenuation due to absorbing capability of the medium                                                     |
| Temporal variation | TV     | Statistics-driven fluctuation of sound resulting from turbulence and time-variance of the medium (the atmosphere).        |
| Scattering         | SC     | Diffuse scattering off non-planar surfaces.                                                                               |
| Diffraction        | DF     | Diffraction off and around obstacles.                                                                                     |
| Near field         | NF     | Acoustic phenomena caused by near field effects (in contrast to far field assumptions).                                   |
| Doppler            | DP     | Doppler frequency shifts based on relative distance changes.                                                              |
| Spreading loss     | SL     | Distance dependend spreading loss, i.e. for spherical waves. Also called 1/r-law or (inverse) distance law.               |
| Transmission       | TR     | Transmission of sound energy through solid structures like walls and flanking paths.                                      |
| Absorption         | AB     | Sound absorption by material.                                                                                             |

### Log level

The VA log level at server side can be changed using

```matlab
va.set_log_level( 3 ) % 0 = quiet; 1 = errors; 2 = warnings (default); 3 = info; 4 = verbose; 5 = trace;
```

Increasing the log level is potentially helpful to detect problems if the current log level is not high enough to throw an indicative warning message.

### Search paths

At runtime, search paths can be added to the VA server using

```matlab
va.add_search_path( './your/data/' )
```

Note, that the search path has to be available at server side if you are not running VA on the same machine.
Wherever possible, add search paths and use file names only.
Never use absolute paths for input files. If your server is not running on the same machine, consider adding search paths via the configuration at startup.

### Query registered modules

To retrieve information on the available modules, use

```matlab
modules = va.get_modules()
```

This method will return any registered VA module, including all renderer and reproduction modules as well as the core itself.

All modules can be called using

```matlab
out_args = va.call_module( 'module_id', in_args )
```

where `in_args` and `out_arg` are structs with specific fields which depend on the module you are calling.
Usually, a struct field with the name `help` or `info` returns useful information on how to work with the respective module:

```matlab
va.call_module( 'module_id', struct('help',true) )
```

If this is not the case, usually the quickest way is to look in the source code.

To work with renderers, use

```matlab
renderers = va.get_rendering_modules()
params = va.get_renderer_parameters( 'renderer_id' )
va.set_renderer_parameters( 'renderer_id', params )
```

Again, all parameters are returned as structs.
More information on a parameter set can be obtained using structs containing the field `help` or `info`.
It is good practice to use the parameter getter and inspect the key/value pairs before modifying and re-setting the module with the new parameters.

For reproduction modules, use

```matlab
reproductions = va.get_reproduction_modules()
params = va.get_reproduction_parameters( 'reproduction_id' )
va.set_reproduction_parameters( 'reproduction_id', params )
```

Querying and re-setting parameters works in the same way as described for rendering and reproduction modules.

## Game engine interfaces

The game engine interfaces for VA make it much simpler to setup and control a virtual scene.
They are designed to fit into the concept of the game engines objects.
As such the control of VA is done through game objects, for example, their position and rotation are aromatically passed to the VAServer.

At the moment, two game engines are supported: Unity and Unreal.
Note, that the Ureal interface is primally developed by the [Visual Computing Institute](https://www.vr.rwth-aachen.de/){target="_blank"}.
For further information about the Unity interface, please refer to the [projects README page](https://git.rwth-aachen.de/ita/VAUnity){target="_blank"}.
