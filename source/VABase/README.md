## VABase

VABase provides a (mostly abstract) C++ interface for the Virtual Acoustics framework called [VA](http://git.rwth-aachen.de/ita/VA). 
It is implemented by a core called [VACore](http://git.rwth-aachen.de/ita/VACore) that creates virtual acoustic environments for real-time applications. 
To use VA, you can include this C++ interface directly with the VACore libraries, or via a network protocol called [VANet](http://git.rwth-aachen.de/ita/VANet). With VANet you can connect with a VA server (for example [VAServer](http://git.rwth-aachen.de/ita/VAServer) or [Redstart](http://git.rwth-aachen.de/ita/VA/Redstart)) and remotely control it. VANet is lightweight and fast and is usually the better choice.

There are also binding interfaces for scripting languages, like Matlab with [VAMatlab](http://git.rwth-aachen.de/ita/VAMatlab), Python in [VAPython](http://git.rwth-aachen.de/ita/VAPython), Lua in [VALua](http://git.rwth-aachen.de/ita/VALua) and C# in [VACS](http://git.rwth-aachen.de/ita/VACS).
For the [ViSTA framework](https://devhub.vr.rwth-aachen.de/VR-Group/ViSTA), the [VistaMedia/Audio](http://devhub.vr.rwth-aachen.de/VR-Group/VistaMedia) add-on has an implementation that both can run an internal VA core or control a remote VA server.


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

#### Preface

It is recommended to clone and follow the build guide of the parent project [VA](https://git.rwth-aachen.de/ita/VA), which includes this project as a submodule. You can find comprehensive help on [virtualacoustics.org](http://www.virtualacoustics.org).

#### Building VABase

You can build VABase using a C++11 compiler. Use CMake to generate project files that help building it, for instance Makefiles or Visual Studio solutions. VABase has no dependencies.

If you want to include VA into your application, you either have to link against VANet or VACore (see above).

#### Extending the VA interface

If you want to extend the abstract VA interface, this move will effect VA deeply. Only consider an extension of the interface if you are sure that this new features are absolutely necessary and will improve the usage of VA significantly.

Interface extension workflow:
1. Add the virtual methods to IVAInterface
2. Add classes to VABase, if necessary
3. Implement the methods in VANet by using similar methods as template. Make sure to use a new net message identifier. Add message serializer/deserializer if you introduce a new data class. Do not make mistakes here, the debugging of VANet problems are painful.
4. Add the methods to VACore. Now you can already run VA as a server.
5. Don't forget to add your new methods to the bindings. Here, at least provide the new features for VAPython and VAMatlab as they are heavily used. Again, use similar methods as templates and be careful, because debugging is cumbersome.
