## VACS (C# wrapper library)

VACS is a C++ wrapper library for VANet in order to expose a C-style API that can be easily imported as native library for C#. Main application is the VAUnity project (connect Unity with VA).
To use VA in C#, build this project first. After you have generated the dynamic wrapper DLL called VANetCSWrapper.dll, you can either import the exported functions directly, or you can use the convenience 
class VANet within the VA namespace, that can be found in the 'csharp' folder of the build environment you have generated (CMake will configure a C#-project file there).
If you build this C#-compatible dynamic library out of the project, another class with methods comparable to the C++ VANet handling can be used, instead.

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


### Quick build guide

It is recommended to clone and follow the build guide of the parent project [VA](https://git.rwth-aachen.de/ita/VA), which includes this project as a submodule.
However, if you only want to create and deploy VACS, i.e. to create a plugin for Unity, you should build this project directly, after you have build the dependencies (basically VABase and VANet together with some VistaCoreLibs components). This way, only the dependend files are copied to the distribution directory and the rest is skipped.
Select release build type and run the INSTALL project found in the CMakePredefinedTargets group. Find the ready-to-use binaries in the '''dist''' folder of the project.
