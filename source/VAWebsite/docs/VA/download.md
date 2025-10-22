---
Title: Download
---

# Download section {style="text-align: center;"}

Get Virtual Acoustics (VA) for Windows
{style="font-size:1.2em;text-align: center;"}
<hr class="accent-hr">


## Full VA package for Windows {style="text-align: center;"}

<div markdown class="grid-container" style="align-items: end;text-align:center">
[![VA for Windows](/assets/VA_Redstart_for_windows.png){ loading=lazy width=40% style="border-radius: .2rem;" }](https://rwth-aachen.sciebo.de/s/9cwxf38ZbyKI8ZS/download)
</div>

**Build name**: [VA_full.v2022a.win64.vc14](https://rwth-aachen.sciebo.de/s/9cwxf38ZbyKI8ZS/download){target="_blank"}<br>
*Includes VAServer, VC14 C++ programming libraries, all bindings and Unity scripts.*

**Quick user guide**:<br>
A quick guide can be found in the [documentation](documentation/index.md#quick-start-guide) section.

**Binary package license / sharing**:<br>
GNU General Public License

**Details**:<br>
+ VABase, VANet and VACore* C++ developer packages for Visual C++ Compiler Version 14 (Visual Studio 2019)<br>
+ VAMatlab and VAC# bindings<br>
+ VAPython bindings for Python 3.8, 3.9 and 3.10<br>
+ VAUnity<br>
+ VAServer command line interface application<br>
- Redstart VA GUI application [DEPRECATED]<br>

*includes FFTW3 (GNU GPL)
{style="font-size:0.6em;"}

----------

## Important changes in 2022
In 2022, we introduced major changes in our build system. This allows us to use compilers newer than VC12 for all VA components. Generally, this lead to the following changes for newly deployed VA versions:

- VAPython is now part of the `VA_full` build. The `VA_bindings` build is not shipped anymore.
- VAPython is now included as *wheels* which makes installing much easier
- We stopped developing the Redstart GUI and removed it from the packages

----------

## Changelog

??? note "v2022a"
    **VACore - Rendering system rework**

    - Now distinguishs between FIR-based and sound-path-based renderers
    - Sound-path-based renderers are not restricted to binaural output anymore but can choose from binaural / Ambisonics / VBAP
    - e.g. `FreeField` renderer replaces `AmbisonicsFreeField` and `VBAPFreeField`

    **VAMatlab**

    - Tracking now works with AR-Tracking (in addition to OptiTrack)

    **VAPython**
    
    - Is now installed using *wheels*
    - Now shipped with version 3.8 to 3.10

    **Bugfixes**

    - Undesired default receiver position (`[0 0 0]`) removed
    - Fixed performance issues of Windows timer used in *VAMatlab*
    - Fixed CTC crash that could occur if utilized HRTF did not cover a full sphere
    - Fixed bug in *ITASimulationScheduler* where mainly a single source-receiver pair was simulated disregarding other sound sources

??? note "v2021a"
    **General**

    - Now a receiver becomes active as soon as it has a valid position. The `set_active_listener` function should not be used anymore was removed from all bindings.

    **VAServer / VACore**

    - Now comes with `VACore.recording.ini` file for offline rendering. Use `run_VAServer_recording.bat` to start VAServer with this configuration.

    **VACore - AirtraficNoiseRenderer**

    - Now can switch between homogeneous and inhomogeneous medium
    - Homogeneous medium: straight sound paths calculated within VA
    - Inhomogeneous medium: curved sound paths simulated with [Atmospheric Ray Tracing](../GA/art.md) framework

    **VAPython**

    - Previously only working for Python 3.6, now only works with Python 3.7 and higher
    - Now shipped with version 3.7 to 3.9

----------

## Snapshots

| Name                                                                                                                   | Sharing                       | Details       |
|------------------------------------------------------------------------------------------------------------------------|-------------------------------|---------------|
| [VA_full.v2022a_dev.win64.vc14](not_available){target="_blank"}         | GPL                           | [CURRENTLY NO DEV VERSION AVAILABLE] Continuously updated developer branch distribution (includes hotfixes and recent feature requests) | <!-- If dev version not available: 1) Prefix in Details: [CURRENTLY NO DEV VERSION AVAILABLE] 2) Link to "website" not_available-->
| [VA_full.v2022a.win64.vc14](https://rwth-aachen.sciebo.de/s/9cwxf38ZbyKI8ZS/download){target="_blank"}             | GPL                           | v2022a full |
| [VA_full.v2021a.win32-x64.vc12](https://rwth-aachen.sciebo.de/s/oGkhZzaqhCFhD4A/download){target="_blank"}             | GPL                           | v2021a full |
| [VA_bindings.v2021a.win32-x64.vc14_static](https://rwth-aachen.sciebo.de/s/qOIpg23cAnZDiE3/download){target="_blank"}  | Apache License, Version 2.0   | v2021a bindings (static) |
| [VA_full.v2020a.win32-x64.vc12](https://rwth-aachen.sciebo.de/s/lyzORELQHVNSRdy/download){target="_blank"}             | GPL                           | v2020a full |
| [VA_bindings.v2020a.win32-x64.vc14_static](https://rwth-aachen.sciebo.de/s/unWOHSO72yJyxog/download){target="_blank"}  | Apache License, Version 2.0   | v2020a bindings (static) |
| [VA_full.v2019a.win32-x64.vc12](https://rwth-aachen.sciebo.de/s/XNdx5jgUZXLVBS2/download){target="_blank"}             | GPL                           | v2019a full |
| [VA_bindings.v2019a.win32-x64.vc14_static](https://rwth-aachen.sciebo.de/s/Rfx4Mj9RhO3WnSa/download){target="_blank"}  | Apache License, Version 2.0   | v2019a bindings (static) |
| [VA_full.v2018b.win32-x64.vc12](https://rwth-aachen.sciebo.de/s/pw9W9yW9vIK0OWs/download){target="_blank"}             | GPL                           | v2018b full |
| [VA_bindings.v2018b.win32-x64.vc14_static](https://rwth-aachen.sciebo.de/s/AovWyvDMGWWIaef/download){target="_blank"}  | Apache License, Version 2.0   | v2018b bindings (static) |
| [VA_full.v2018a.win32-x64.vc12](https://rwth-aachen.sciebo.de/s/GRvBrd7cq7mIn7D/download){target="_blank"}             | GPL                           | v2018a full |
| [VA_bindings.v2018a.win32-x64.vc14_static](https://rwth-aachen.sciebo.de/s/uW4dRhjm5FKA0yZ/download){target="_blank"}  | Apache License, Version 2.0   | v2018a bindings (static) |

----------

## Linux, Mac OSX and other

From the very beginning, Virtual Acoustics has been developed and used under **Windows** platforms. If you do not have a Windows system, you can still use VA. Unfortunately, we are not experienced in building binary packages for other platforms than Windows, hence you will have to build it from the source code. However, we can't guarantee smooth performance on those systems. For more information, read the [developer section](../developer.md).

A common alternative way is to run the VA server application on a dedicated remote Windows PC with a proper ASIO interface, and control it from any other platform. This way, you can use the Windows download package and you only have to build the remote control part of VA (the C++ interface libraries and/or bindings). You will require no further third-party libraries except for the binding interface you choose (usually Matlab or Python). You can also run a Jupyter notebook along with VA on the Windows machine and remotely control everything using a web browser on any mobile device connected to the same network. 

----------

## Some hints on VA packages

VA is always in development and we constantly add new features or components. Therefore, we chose the version identifier to reflect the release year and added an ascending alphabetic character, like `v2018a`. This makes it easier to determine the release time, just like in Matlab and other applications.

We **cannot guarantee compatibility among VA versions!** The reason is, that we still update the interface for new powerful features. This is unfortunate, but can not be achieved with the resources we can afford. If you are interested in new features, you will have to pay the price and update everything, including the bindings you use.

We adopt naming conventions for platforms and compiler versions from [ViSTA VR Toolkit](https://www.itc.rwth-aachen.de/cms/IT-Center/Forschung-Projekte/Virtuelle-Realitaet/Infrastruktur/~fgmo/ViSTA-Virtual-Reality-Toolkit/){target="_blank"}. This way, one can easily extract the target platform, such as `win32-x64` for a Windows operating system using a mixed 32 bit and 64 bit environment, or `vc12` to indicate that the binary package was built using the Microsoft Visual C++ Compiler Version 12. If you are missing redistributable libraries you can identify the required Microsoft installer by this suffix.

The build environment of VA provides configuration of the underlying components. This may or may not result in a binary package that links against GNU GPL licensed libraries. For this reason, VA is either licenses under the favored Apache License, Version 2.0 or under the GNU General Public License, as required by the copyleft nature. 