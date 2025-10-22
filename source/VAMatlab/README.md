## VAMatlab

VAMatlab is a binding to the VA interface for Matlab. It uses the VA network connection to forward commands and receive information to a VA server, usually the VAServer application.


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

#### Visual Studio

It is recommended to clone and follow the build guide of the parent project [VA](https://git.rwth-aachen.de/ita/VA), which includes this project as a submodule.
Generate VA with the Matlab binding flag activated. It will generate the Matlab executable. You will then have to generate the itaVA Matlab facade class, see below.


#### Matlab executable

Open Matlab and run the script `VAMatlabExecutable_build.m`. 
It will create the *VAMatlab* Matlab executable, for win23-x64 the binding will have the name `VAMatlab.mexw64`.
There is a switch that can link the VACore into the Matlab executable, hence no external VA server application is required.
To activate this, the preprocessor flag `VAMATLAB_WITH_LOCAL_CORE` has to be defined and set to 1. Use the switch in the build script configuration section.
To also include a lightweight Optitrack tracker connection using the NatNetSDK, define the preprocessor flag `VAMATLAB_WITH_OPTITRACK` and set it to 1. Use the switch in the build script configuration section.


#### itaVA class

To ease usage, a facade class called `itaVA` can be generated that will also add volatile help on the available functionality of VA.
To create this Matlab class, run either `itaVA_build.m` after deploy of VA, or `itaVA_build_absolute.m` from the repository directory.
Both scripts will create a class file named `itaVA.m` based on stubs that are filled by calls to the VAMatlab executable, which has to be build prior.
> Make sure that Matlab will use the correct VA Matlab executable and not an older version of it which might have been added to the PATH of Matlab.


#### Deploy

If the deploy script is not used, manual deploy will include the following:

Deploying the Matlab executable will require to copy all necessary DLLs into a deploy directory (next to the VAMatlab executable file).
On Windows platform, provide the following files
 - VistaAspects.dll
 - VistaBase.dll
 - VistaInterProcComm.dll
 - VABase.dll
 - VANet.dll
 - NatNetSDK.dll (if with tracking)
 - VACore.dll (if with local core, as well as many more ... use DependencyWalker to identify missing files)
 