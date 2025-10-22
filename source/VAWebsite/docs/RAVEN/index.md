---
title: Overview
---

# RAVEN {style="text-align: center;"}

Room Acoustics for Virtual ENvironments
{style="font-size:1.2em;text-align: center;"}

<hr class="accent-hr">

Room Acoustics for Virtual ENvironments (RAVEN) is a GA-based room acoustic simulation environment implemented in the C++ programming language.
Its simulation core is based on a hybrid simulation model combing the image source model and a ray tracing algorithm.
The extension of the ray tracing algorithm with the diffuse rain technique that calculates the amount of scattered energy projected to the receiver for each wall reflection, can also be regarded as a first order radiosity implementation.

Advantages of RAVEN are the efficiency of the simulation and the filter synthesis of BRIRs, which makes it possible to create convincing binaural auralization in real-time, e.g. in virtual acoustic environments.
However, it also allows to create RIRs for other reproduction techniques such as vector based amplitude panning (VBAP) or higher-order ambisonics (HOA).

Furthermore, the MATLAB interface of RAVEN allows script-based configuration and operation of simulations, which is attractive for researchers in room acoustics and virtual acoustics.
The introduction of an interface with the 3D modeling software SketchUp allows real-time visualization of parameters and auralization while the user is able to modify the virtual scene inside the SketchUp tool.

------

## Access {style="text-align: center;"}

For academic research and education
{style="text-align: center;"}

<hr class="accent-hr">

Today, the RAVEN software is used for research by various groups and in university courses about room acoustics on five continents.
A software installer including the RAVEN simulation environment is freely available for academic purposes.
Please contact [raven@akustik.rwth-aachen.de](mailto:raven@akustik.rwth-aachen.de) for details.

------

## Interfaces  {style="text-align: center;"}

<hr class="accent-hr">

### SketchUp interface

To easily design and modify the room acoustic scenario, a SketchUp plug-in was implemented, including an interface to a real-time simulation and visualization of results (SketchUp Visualiser), a real-time auralization tool for one active sound source (SketchUp Auralizer) and the possibility to directly run or export the current scene for the RavenConsole application.

In the video below, Lukas presents the SketchUp interface tutorial, which is also included in the RAVEN installation.

![type:video](https://www.youtube.com/embed/P27z3PWBNpM)

_RAVEN SketchUp Interface tutorial_
{style="text-align: center;"}

### MATLAB interface

Using [RAVEN's MATLAB interface](https://git.rwth-aachen.de/ita/toolbox/-/tree/master/applications/SoundFieldSimulation/Raven){target="_blank"}, it is possible to load and configure a RAVEN project file, and systematically run and evaluate simulations directly from MATLAB's command line interface.
It includes various demo scripts and the option to adjust a room simulation to target parameters and is part of the [ITA-Toolbox](https://www.ita-toolbox.org/){target="_blank"}.

### Python interface

Coming soon...

------

## Auralization {style="text-align: center;"}

Based on RAVEN simulations
{style="text-align: center;"}

<hr class="accent-hr">

In addition to static auralizations based on the convolution of (binaural) room impulse responses with anechoic sound files, it is also possible to define acoustic trajectories for sound sources and the receiver and to (binaurally) render the corresponding acoustics scene.
This can be achieved using the Acoustic Animation tool of RAVEN.

Simulated Room Impulse Responses by RAVEN can also be applied in the real-time auralization framework VirtualAcoustics, e.g., [a dynamic binaural synthesis of a room acoustic situation](https://git.rwth-aachen.de/ita/toolbox/-/blob/master/applications/SoundFieldSimulation/Raven/ita_raven_demo_VABinauralDaffv17.m){target="_blank"} based on full sphere simulation.
Another approach is simply [loading a RAVEN generated RIR in the generic path renderer](https://git.rwth-aachen.de/ita/toolbox/-/blob/master/applications/VirtualAcoustics/VA/itaVA_experimental_RAVEN.m){target="_blank"} of [VirtualAcoustics](../VA/index.md).

![type:video](https://www.youtube.com/embed/r8DTyUODgrY)

_Binaural tour to the Eurogress Aachen_
{style="text-align: center;"}

![type:video](https://www.youtube.com/embed/vZaeYA6q-8U)

_Auralization of a classroom with different level of details_
{style="text-align: center;"}

------

## Scientific publications {style="text-align: center;"}

<hr class="accent-hr">

### General documentation of the framework

_D. Schröder_<br>
___Physically based real-time auralization of interactive virtual environments___<br>
PhD thesis, RWTH Aachen, Aachen, Germany, 2011<br>
[http://publications.rwth-aachen.de/record/50580/files/3875.pdf](http://publications.rwth-aachen.de/record/50580/files/3875.pdf)

_D. Schröder and M. Vorländer_<br>
___Raven: A real-time framework for the auralization of interactive virtual environments___<br>
Forum Acusticum, pages 1541-1546, Aalbord Denmark, 2011<br>
[https://www2.ak.tu-berlin.de/~akgroup/ak_pub/seacen/2011/Schroeder_2011b_P2_RAVEN_A_Real_Time_Framework.pdf](https://www2.ak.tu-berlin.de/~akgroup/ak_pub/seacen/2011/Schroeder_2011b_P2_RAVEN_A_Real_Time_Framework.pdf)

### Regarding the SketchUp plug-in in general and the integration of room acoustics simulation

_S. Pelzer, L. Aspöck, D. Schröder and M. Vorländer_<br>
___Integrating real-time room acoustics simulation into a cad modeling software to enhance the architectural design process___<br>
Buildings, 4(2), pages 113-138, 2014<br>
[https://www.mdpi.com/2075-5309/4/2/113/pdf](https://www.mdpi.com/2075-5309/4/2/113/pdf)

### Regarding the real-time auralization in SketchUp

_L. Aspöck, S. Pelzer, F. Wefers and M. Vorländer_<br>
___A Real-Time Auralization Plugin for Architectural Design and Education___<br>
Proc. of the EAA Joint Symposium on Auralization and Ambisonics, pages 156-161., 2014<br>
[https://depositonce.tu-berlin.de/bitstream/11303/183/1/25.pdf](https://depositonce.tu-berlin.de/bitstream/11303/183/1/25.pdf)

### Regarding the informed simulation of reference scenes (Chapter 5)

_L. Aspöck_<br>
___Validation of room acoustic simulation models___<br>
PhD thesis, RWTH Aachen, Aachen, Germany, 2020<br>
[https://publications.rwth-aachen.de/record/808553](https://publications.rwth-aachen.de/record/808553)
