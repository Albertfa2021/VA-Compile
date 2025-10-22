---
title: Atmospheric Ray Tracing
---

# Atmospheric Ray Tracing {style="text-align: center;"}

An efficient, open-source framework for finding eigenrays in a stratified, moving medium
{style="font-size:1.2em;text-align: center;"}

<hr class="accent-hr">

## Overview

The Atmospheric Ray Tracing (ART) framework allows to simulate curved sound propagation through an three-dimensional, inhomogeneous, moving atmosphere. The atmosphere is assumed to be stratified meaning the weather parameters are only altitude-dependent and wind direction is purely horizontal. In addition to predefined analytic models, arbitrary data vectors can be used for the weather data (e.g. from [atmospheric soundings](https://weather.uwyo.edu/upperair/sounding.html){target="_blank"}). In accordance to this stratified model, the ground is assumed to be flat.

![type:video](https://www.youtube.com/embed/AHOG6LCeZp8){target="_blank"}

_Visualization of eigenrays of an aircraft flyover during take-off_
{style="text-align: center;"}

The framework is designed for the purpose of aircraft noise auralization. Thus, in addition to tracing rays into defined directions, it provides an efficient method for finding eigenrays connecting a source with a receiver. This approach is called adaptive ray zooming. The methods used in this framework are well documented in this open-access [publication](https://doi.org/10.1051/aacus/2021018){target="_blank"}.

Being part of the ITAGeometricalAcoustics C++ library collection, the framework is open-source. The code is provided on the respective [git repository](https://git.rwth-aachen.de/ita/ITAGeometricalAcoustics){target="_blank"}. The ART framework can be interfaced in Matlab using the [ARTMatlab](#ARTMatlab) application. It allows a visualization of the simulated sound paths and derivation of acoustic parameters based on those paths.

------

## ARTMatlab {style="text-align: center;"}

The MEX file application to interface the Atmospheric Ray Tracing (ART) framework via Matlab
{style="text-align: center;"}

<hr class="accent-hr">

### Getting started

ARTMatlab is a MEX file application including the Atmospheric Ray Tracing framework.
It contains a set of classes allowing to interface the ART framework fast and easily.
In addition to the simulation of rays and eigenrays, it also provides methods to derive acoustic parameters based on those sound paths.

#### Requirements

The [ITA-Toolbox](https://www.ita-toolbox.org/){target="_blank"} for Matlab must be installed before ARTMatlab can be used.

#### Matlab classes

|Class name              |Description|
|:-----------------------|:----------|
|`AtmosphericRayTracer`  |The interface to the simulation engine and can either be used to trace rays into given directions or to find eigenrays.|
|`StratifiedAtmosphere`  |Allows to configure the weather parameters, such as temperature, wind velocity/direction and relative humidity of the medium. It is able to import data from [atmospheric soundings](https://weather.uwyo.edu/upperair/sounding.html){target="_blank"}.|
|`AtmosphericRay`        |Represents one of the resulting sound paths. The class comes with convenience functions e.g. for plotting|
|`AtmosphericPropagation`|Allows to derive acoustic parameters or to generate a transfer function based on a set of eigenrays|

#### Tutorials

Check out the tutorial scripts provided in the main folder of ARTMatlab to get some examples of how to use the framework:

- `ARTTutorial_Overview.m`
- `ARTTutorial_StratifiedAtmosphere.m`
- `ARTTutorial_PropagationModel.m`

### Download

|Name                                                                                         |Version|Changes|
|:--------------------------------------------------------------------------------------------|:------|:------|
|[ARTMatlab_v2021b](https://rwth-aachen.sciebo.de/s/1ddX6BHvD1siDSD/download){target="_blank"}|v2021b |Newest release. Added PropagationModel class. Eigenrays now have a property indicating whether receiver sphere was hit. AtmosphericRayTracer class now can display ARTMatlab version.|
|[ARTMatlab_v2021a](https://rwth-aachen.sciebo.de/s/rIxsisrdTK8Q358/download){target="_blank"}|v2021a |Initial release used in the [paper](https://doi.org/10.1051/aacus/2021018){target="_blank"} introducing the framework.|

------

## Auralization {style="text-align: center;"}

Based on the ART framework and Virtual Acoustics (VA)
{style="text-align: center;"}

<hr class="accent-hr">

![type:video](https://www.youtube.com/embed/Zn26naG_e24)

_Auralization of an aircraft flyover during take-off based on the ART framework_
{style="text-align: center;"}

![type:video](https://www.youtube.com/embed/U0haSITYkgk)

_Changing the wind direction during aircraft flyover auralization - High quality visual model (IHTAPark)_
{style="text-align: center;"}

------

## Scientific publications {style="text-align: center;"}

<hr class="accent-hr">

_P. Schäfer and M. Vorländer_<br>
___Atmospheric Ray Tracing: An efficient, open-source framework for finding eigenrays in a stratified, moving medium___<br>
Acta Acustica, Volume 5, 2021<br>
[https://doi.org/10.1051/aacus/2021018](https://doi.org/10.1051/aacus/2021018){target="_blank"}
