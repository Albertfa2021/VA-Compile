# Recording

As already pointed out, VA can be used to record simulated acoustic environments. The only requirement is to activate the output recording flag in the configuration and add a target file name and base folder where to store the recordings, as described in the [rendering](configuration.md#optional-rendering-module-parameters) and [reproduction](configuration.md#optional-reproduction-module-parameters) module setup sections.

Outputs from the rendering modules and inputs to reproduction modules can be used to record spatial audio samples (like binaural clips or Ambisonics B-format / HOA tracks). Outputs from reproductions can be used for offline playback over the given loudspeaker setup, e.g. for (audio-visual) demonstrations or non-interactive listening experiments.

Two different approaches can be used:

- **Capturing the real-time audio streams**
- **Emulating a sound card** and **processing the audio stream offline**

>**IMPORTANT**: The recorded audio stream(s) are only written into file(s) once the **VAServer is shutdown**!

## Real-time capturing

If you want to capture the real-time output of rendering and reproduction modules, VA is driven by the sound card and live updates - like tracking device data or HMD movements - are effective. The scene is updated according to the user interaction and are bound to the update rate of the tracking device or control loop timeout. This approach is helpful to record simple scenes with synchronized audio-visual content, e.g. when using Unity3D and preparing a video for demonstration purposes.

## Offline rendering and capturing

For **sound field simulations** with high-precision timing for physics-based audio rendering, using a lot of scene adaptions in combination with a very small audio processing block sizes is a wise desision. Therefore, one should switch to the offline simulation capability of VA.

A virtual sound device can be activated that suspends the timeout-driven block processing and gives the control of the audio processing speed into the user's hands. This allows to change the scene without time limits and every audio processing block can be triggered to process the entire new scene, no matter how small the block size (in real-time mode, typically 10 times more audio blocks are processed during a single scene update, for example a translation of the listener triggered by a tracking device). Additionally, there is no hardware constraint as the rendering and reproduction calculations are not bound to deliver real-time update rates. This makes it possible to set up scenes of arbitrary complexity for the cost of a longer calculation of the processing chain including rendering, reproduction, recording and export to generate the audio files.

Running VA in offline mode requires to make special adjustments to your `VACore.ini` file. Additionally, scene updates need to be actively triggered by the user. In the following a quick guide to run the provided examples is given, before discussing the details of offline audio processing (using `Matlab` examples).

### Quick guide
VA provides a configuration file using which is set up for free-field conditions and binaural processing called `VACore.recording.ini`. Starting the VAServer with the `run_VAServer_recording.bat` script located in VA's main folder will use this ini file. Then you can execute one of the Matlab example scripts, `VA_example_offline_simulation.m` ([open in git](https://git.rwth-aachen.de/ita/VAMatlab/blob/master/matlab/VA_example_offline_simulation.m){target="_blank"}) and `VA_example_offline_simulation_ir.m` ([open in git](https://git.rwth-aachen.de/ita/VAMatlab/blob/master/matlab/VA_example_offline_simulation_ir.m){target="_blank"}), to make a recording.

### Virtual sound card audio driver configuration
In order to use the virtual sound card, the audio driver section has to be adjusted as follows
```
[Audio driver]
Driver = Virtual
Device = Trigger
Samplerate = 44100
Buffersize = 64
Channels = 2
```
Note, that the `Buffersize` and number of `Channels` must be set by the user and cannot be `AUTO`.

### Controlling the virtual card audio processing
To modify the internal timing, set the clock by an increment of the blocks to be processed, e.g.
```matlab
% Clock increment of 64 samples at a sampling rate of 44.1kHz
manual_clock = manual_clock + 64 / 44100;
va.call_module( 'manualclock', struct( 'time', manual_clock ) );
```

To trigger a new audio block to be processed, run
```matlab
% Process audio chain by incrementing one block
va.call_module( 'virtualaudiodevice', struct( 'trigger', true ) );
```
These incrementations are usually called at the end of a simulation processing loop. Any scene change prior to that will be effectively auralized.

For example to implement a dynamic room acoustics situation with an animation path, a generic path renderer can be used and a full room acoustics simulation of 10 seconds runtime can be executed prior and the filter exchange, making every simulation step change audible. 

### Changing recording file paths during runtime
>Note: this feature is available since v2018b.

When recording offline simulations, it is often helpful to store the recorded audio file to a different folder each time a script is executed. To do so, VA accepts a parameter call to modify the base folder and the file name as follows: 
```matlab
% Single-line calls
va.set_rendering_module_parameters( 'MyRenderer', struct( 'RecordOutputFileName', 'modified_reproduction_out.wav' ) );
va.set_rendering_module_parameters( 'MyRenderer', struct( 'RecordOutputBaseFolder', ...
                                    fullfile( 'recordings/MyRenderer', datestr( datetime( now, 'yyyy-mm-dd_HH-MM-SS' ) ) ) ) ); % with date and time folder

% Struct call
mStruct = struct();
mStruct.RecordOutputFileName = 'modified_rendering_out.wav';
mStruct.RecordOutputBaseFolder = fullfile( 'recordings/MyRenderer', datestr( datetime( now, 'yyyy-mm-dd_HH-MM-SS' ) ) ); % with date and time folder
va.set_rendering_module_parameters( 'MyRenderer', mStruct );
```

For reproduction modules, the same parameter update is possible including the input stream file name and base folder: 
```matlab
% Struct call
mStruct = struct();
mStruct.RecordInputFileName = 'modified_reproduction_in.wav';
mStruct.RecordInputBaseFolder = fullfile( 'recordings/MyReproduction', datestr( datetime( 'now' ) ) ); % with date and time folder
mStruct.RecordOutputFileName = 'modified_reproduction_out.wav';
mStruct.RecordOutputBaseFolder = fullfile( 'recordings/MyReproduction', datestr( datetime( 'now' ) ) ); % with date and time folder
va.set_reproduction_module_parameters( 'MyReproduction', mStruct );
```

### VAServer remote control mode
>Note: this feature is available since v2020a.

To run several simulations in a batch system, the VAServer application can be started in a remote control mode that allows remote shutdown with a special client call. Stopping VA with a kill command will not close VA properly and the simulated and recorded data is lost. To enable the remote control mode, add a fourth parameter. A Matlab code snippet to launch it could look like this:

```matlab
vaserver_path = which( 'VAServer.exe' );
vaserver_config = which( 'VACore.ini' );
va_syscall = sprintf('"%s" %s:%i "%s" remote_control_mode &', vaserver_path, 'localhost', 12340, vaserver_config );
disp( [ 'Calling: ' va_syscall ] )
system( va_syscall );
pause( 1 )
va = VA( 'localhost' );
% Run your scene ...
va.shutdown_server
va.disconnect
```

The pause is required to give VA enough time to instantiate all modules (e.g. sound card sometimes needs a while) and reach a status where client connections can be accepted. A pause of 1 second before a connection can be established usually suffices, but adjustments may be required in some cases. Depending on the amount of data that must be exported, another pause after shutdown may be needed until the WAV files are accessible and, e.g., can be loaded in Matlab for further processing.
