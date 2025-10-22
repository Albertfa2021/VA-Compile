---
title: Image edge model
---

# The image edge model {style="text-align: center;"}

A model for finding higher order sound propagation paths including reflection and diffraction
{style="font-size:1.2em;text-align: center;"}

<hr class="accent-hr">

## Overview

The image edge model (IEM) allows finding sound propagation paths in complex environments, such as urban ares, considering reflections and diffractions between a source and a receiver. The method can be considered as an extension of the image source model which also images diffracting edges. Thus, it is capable of considering paths including multiple diffractions.

![Image edge model sound paths](/assets/iem_soundpaths.png){ loading=lazy width=100% style="border-radius: .2rem;" }

_Visualization of propagation paths in an urban area taken from "[The image edge model](https://doi.org/10.1051/aacus/2021010){target="_blank"}"_
{style="text-align: center;"}

The model is designed for the purpose auralization of urban scenarios. To increase the efficiency of the algorithm, it allows to drop irrelevant sound paths at an early stage using back-face culling. Additionally, it provides psychoacoustical culling schemes. These reduce the total number of considered sound paths by estimating their audibility.

Being part of the ITAGeometricalAcoustics C++ library collection, the implementation of the model is open-source and is provided on the respective [git repository](https://git.rwth-aachen.de/ita/ITAGeometricalAcoustics){target="_blank"}. The [pigeon](#pigeon) application allows to run simulations based on the IEM. Those paths can then be visualized or used to derive acoustic filter properties, e.g. propagation delay and air attenuation, for the purpose of auralization.

------

## Pigeon {style="text-align: center;"}

The propagation path finder application based on the image edge model
{style="text-align: center;"}

<hr class="accent-hr">

### Getting started

Pigeon is a propagation path finder application based on the image edge model (IEM). It allows to find sound paths in complex urban environments considering reflections and diffractions. It has a GUI mode for quick testing and a CLI mode for batch processing. In addition to simulating the sound paths, pigeon features the export of a SketchUp file with the considered geometry mesh and a visualisation of the propagation paths.

The usage basically is:

1. Point to a geometry mesh file
1. Define a source and a receiver
1. Set simulation parameters
1. Run the algorithm to export a propagation path list

All simulation parameters are given to the simulation engine using an ini-file. The most convenient way to setup and run a simulation is using the Matlab interface introduced in the following.

### Matlab interface

The [ITA-Toolbox](https://www.ita-toolbox.org/){target="_blank"} for Matlab includes a set of functions and classes to interface the pigeon app. Additionally, it contains methods to derive filter properties from the simulated sound paths. In this context, the two most important classes are:

|Class name         |Description                                                                                           |
|:------------------|:-----------------------------------------------------------------------------------------------------|
|`itaPigeonProject` |The interface to the simulation engine. Used to setup and run simulations.                            |
|`itaGeoPropagation`|Allows to derive acoustic parameters or to generate a transfer function based on a set of sound paths.|

#### Tutorials

Check out the examples provided in the folder "ITA-Toolbox\applications\SoundFieldSimulation\Pigeon\examples\" which is part of your ITA-Toolbox directory:

- `itaPigeon_example.m`
- `itaGeoPropagation_example_paths.m`

### Download

|Name                                                                                      |Version|Changes|
|:-----------------------------------------------------------------------------------------|:------|:------|
|[pigeon_v2021a](https://rwth-aachen.sciebo.de/s/5oiF85H0INRWLbM/download){target="_blank"}|v2021a |Newest release. Several bugfixes now allow a proper usage of the ITA-Toolbox interface classes.|
|[pigeon_v2020b](https://rwth-aachen.sciebo.de/s/F9S8S21ASGss933/download){target="_blank"}|v2020b |Initial release used in the paper [The image edge model](https://doi.org/10.1051/aacus/2021010){target="_blank"}.|

------

## Scientific publications {style="text-align: center;"}

<hr class="accent-hr">

_A. Erraji, J. Stienen and M. Vorländer_<br>
___The image edge model___<br>
Acta Acustica, Volume 5, __2021__<br>
[https://doi.org/10.1051/aacus/2021010](https://doi.org/10.1051/aacus/2021010){target="_blank"}
