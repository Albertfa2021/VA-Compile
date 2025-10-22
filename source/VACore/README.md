## VACore project

VACore implements the core functionality for real-time auralization of virtual environments and sophisticated reproduction of various types. It can be
created using a factory method and exposes the IVACore abstract class methods. Manage the core by the C++ interface (directly or over TCP/IP network in VANet) or via various binding interfaces, i.e. for Matlab (VAMatlab).


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


### Additional software

Some applications like the VAServer and the VAGUI are using the VACore to manage the auralization and reproduction. Also, bindings for C++ and Matlab exist, where you can connect to the VACore via network connection.


### Help

VACore is an in-house development of ITA and not for use to anyone else. Help is only provided within projects where ITA is involved.


### Quick build guide

It is recommended to clone and follow the build guide of the parent project [VA](https://git.rwth-aachen.de/ita/VA), which includes this project as a submodule.


### Deployment & publication

Please do NOT deploy and publish a VA core version with TTS (text-to-speech) activated, as it would also include the CereVoice SDK. This is not in compliance with the license agreement (requires account with academic e-mail address). For your research project, use it strictly interenally.

*The developers of VA will not be hold responsible for license violations.*


### Additional safety warning

High sound pressure levels can do severe harm to the human auditory system. Please double check if the system you are using is absolutely safe, because VA will not take any responsibility in case of an accident, as stated in the license agreement. This is only a precaution list as a suggestion for typical situations, but may not be applicable in others.

To counteract an hazardous environment, make yourself familiar with all components included. We suggest that you double check the following conditions:

1. Know the maximum sound power that your emitters (loudspeakers, headphones, amplifiers) are capable of.
2. Know the minimum distance to the ear canal entries of your subject or user. Estimate the maximum possible sound pressure and assure that the range is not harmful to humans.
3. Never trust the audio device input stream (coming from VA or any other software). If an uninitialized buffer is physically played back, it is likely that signals at maximum capacity are emitted.
4. Start at low volumes for initial tests. Be suspicious if a gain or volume control is operating at maximum or over the usual working range (i.e. over 0dB or over a factor of 1.0).
5. Put a personal emergency plan into action: which gain control or muted button will you use in case of an emergency? Also, consider to install an emergency off switch.
6. Use limiters and configure them appropriately.
