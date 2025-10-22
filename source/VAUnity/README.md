## VAUnity

VAUnity provides C# scripts to connect a VA auralization server with a Unity scene in order to create a synchronized audio-visual VR environment for an arbitrary combination of audio rendering and reproduction modules. The basic concept is to run a VA server and control it via the C# binding ([VACS](http://git.rwth-aachen.de/ita/VACS)) by adding classes to existing GameObjects in Unity. By enriching GameObjects with VA-specific classes, VA is triggered to create corresponding entities (like sources and receivers). Control information (such as translatory and rotatory movement) is propagated to the audio rendering instance(s). The Unity audio engine should be disabled. However, to enable VA support for Unity, the Unity project resources must be at hand and the scenes have to be extended manually by *VAUnity C# scripts* (mainly drag&drop). It is not possible to use VAUnity as an audio plugin for a deployed Unity project.



### License

Copyright 2015-2022 Institute of Technical Acoustics (ITA), RWTH Aachen University

Licensed under the Apache License, Version 2.0 (the "License");
you may not use files of this project except in compliance with the License.
You may obtain a copy of the License at

<http://www.apache.org/licenses/LICENSE-2.0>

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.



### Quick build guide for the unpatient and experienced users

Clone this project inside your Assets folder and place a copy the VA C# bindings from your VA deploy package as well. Add a `VAU Adapter` component to any existing GameObject and extend further GameObjects with corresponding `VAU*` components. Run a VA server before running the Unity scene. At least a listener with and and attached HRTF and a sound source with an attached signal source is required.



## Unity & VA example scene tutorial


### Objective

This section describes how to create an example scene in Unity that controls a VA server for binaural audio rendering. In the end, the visual camera will act as a binaural sound receiver and can be moved from a first person viewpoint to inspect one static and one circulating sound source.


### Software installation

Obtain a current VA full package from [virtualacoustics.org](http://www.virtualacoustics.org). Make sure that the subfolders `unity` and `csharp` are included.


Run a VA server with a binaural free-field rendering instance, e.g. by executing the `run_VAServer.bat` script (the untouched default VA configuration file will suffice).


### Unity project

Download, install and run Unity. Create a new project. Now, open the project folder in the file browser outside Unity. It should include a folder named `Assets`. Here, create a new directory `Plugins` and a subfolder `VA`. From your VA binary package, copy the directory `csharp` into the newly created folder structure (`Assets/Plugins/VA`). Now, either clone the VAUnity repository into the `VA` directory or copy the files from the binary package (folder name: `unity`).
After Unity has detected the new assets the scripts are available and can be assigned to any GameObject as components. You can easily list the scripts as they all have the prefix `VAU`.


### Set up and connect to VA

To set up a connection to VA on startup, add the `VAU Adapter` script to a newly created empty GameObject named `VA`.


### Prepare resource

Create an empty GameObjects called `HRTF` and add an existing C# script named `VAU Default HRIR` to the `HRTF` object.
Place a WAV audio sample in your assets folder. It will be used as signal source.
Create an empty GameObject called `AudioSignal` and add en existing C# script named `VAU Signal Source Audio File`. Type in the full name (including file ending) of the WAV file as FilePath.


### Listener

Drag&drop the `VAU Listener` script onto the camera rig / ears to create a dynamic listener in VA. The Script will update the pose accordingly based on the transform, i.e. if the movement is connected to a tracking device (e.g. VR glasses), the tracked data will be forwared to VA. To enable binaural rendering, drag&drop the `HRTF` GameObject onto the `Directivity` parameter of the `VAU Listener` component. 


### Sound source

Drag&drop the `VAU Sound Source` script onto any sound object to create a VA sound source. The Script will update the pose accordingly based on the transform, i.e. if the movement is connected to a tracking device (e.g. VR controller), the tracked data will be forwared to VA. Also, drag&trop the `AudioSignal` GameObject onto the `Signal Source` property of the `VAU Sound Source` component.


### Auralization modes

You can control the auralization modes via the `VAU Auralizaton Mode` script. Auralization modes are either controlling sound sources, listeners or are set globally. VA will only auralize a certain effect, if the corresponding mode is activated for all three components, i.e. a logic AND combination is applied.


## Extensions

Have a look at the [VAUnityExtensions](http://git.rwth-aachen.de/ita/VAUnityExtensions) project to get 3D user interfaces that control VA using menus, like muting, sound level control, direct auralization mode manipulation and more.
