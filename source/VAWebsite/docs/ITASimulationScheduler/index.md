---
title: Overview
---

# ITA Simulation Scheduler {style="text-align: center;"}

Open source simulation scheduling framework for acoustic simulations
{style="font-size:1.2em;text-align: center;"}

<hr class="accent-hr">

For real-time auralizations it has to be ensured that the processing time for the audio does not exceed a certain threshold that is specified by the audio hardware.
The simulation time for some acoustic simulations exceeds this threshold, meaning that they cannot be run in the audio processing thread itself.
These simulations have to be run in separate threads.
For that reason, the requirement to schedule these simulations arises.

The ITA Simulation Scheduler is a C++ library designed to schedule acoustic simulations.
It's a deeply configurable, modular framework that should be able to adapt to almost any situation where simulations have to be scheduled.
Furthermore, it allows running multiple simulations in parallel, reducing the effective simulation duration.

As, under real-time conditions, there are typically more simulation request than can be simulated, the ITA Simulation Scheduler has to decide which requests are handled and which are dropped.
The most simple approach is to use a replacement scheme that removes outdated scene updates from its queue, i.e the scheduler always handles the newest request and removes all others.

Special to this framework is the option to compare the latest simulated scene with the scene of a new request.
The goal is to only consider simulation requests, which are expected to lead to an audible difference in the simulation result.
For this purpose, the scenes -  especially source and receiver poses - are compared from an acoustic point of view. For example, if a listener in a room only moves its head by a few centimeter the perceived sound does not change significantly.
For this purpose, the framework provides so-called *update selectors* which filter simulation requests using different acoustically motivated criteria.
They can be combined into more complex *selection networks*.
By carefully configuring this network, the amount of simulations can be drastically reduced without significantly reducing the quality of the resulting audio.

------

## Features {style="text-align: center;"}

<hr class="accent-hr">

<div markdown class="tile-container">
<div markdown class="simple-tile">
:material-view-module:{ style="font-size:5em" }

Modular frame work
{style="font-weight:bold;"}
</div>
<div markdown class="simple-tile">
:material-file-check:{ style="font-size:5em" }

Fully configurable
{style="font-weight:bold;"}
</div>
<div markdown class="simple-tile">
:material-clipboard-check-multiple:{ style="font-size:5em" }

Parallel simulations
{style="font-weight:bold;"}
</div>
<div markdown class="simple-tile">
:material-file-replace:{ style="font-size:5em" }

Replacement scheme
{style="font-weight:bold;"}
</div>
<div markdown class="simple-tile">
:material-filter-check:{ style="font-size:5em" }

Acoustically motivated<br>update selection
{style="font-weight:bold;"}
</div>
<div markdown class="simple-tile">
:material-arrow-expand:{ style="font-size:5em" }

Expandable
{style="font-weight:bold;"}
</div>
</div>

------

## Scientific publications {style="text-align: center;"}

<hr class="accent-hr">

_P. Palenda, P. Schäfer, J. Stienen and M. Vorländer_<br>
___Open-Source Simulation Scheduling Framework for Real-Time Auralization___<br>
Proc. of the DAGA 2022, pages 1451-1454., 2022<br>
<!-- [https://doi.org/10.1051/aacus/2021018](https://doi.org/10.1051/aacus/2021018){target="_blank"} -->