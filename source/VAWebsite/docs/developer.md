---
Title: Developers
---

# Developer section {style="text-align: center;"}

<hr class="accent-hr">

## Preface

The presented projects are based on C++17 source code and will compile on any modern Microsoft compiler[^1] with a current C/C++ compiler set.

They use [CMake](http://cmake.org/){target="_blank"} to configure their build environments.
It allows to activate and deactivate rendering and reproduction modules and offers options to control implementation details of the core.

[^1]: Other compilers might work as well, this however is not tested.

## How to build

The source code is hosted on the RWTH gitlab server [here](https://git.rwth-aachen.de/ita/){target="_blank"}.
It is recommended to download the source code via [git](https://git-scm.com/){target="_blank"}.
Most of repositories use submodules, as such they have to be cloned using the `--recursive` flag.
For example of VA, the command looks like this:

```cmd
git clone --recursive http://git.rwth-aachen.de/ita/VA
```

If you want to build any of the projects for yourself, please refer to the build guide in the [ITACoreLibs wiki](https://git.rwth-aachen.de/ita/ITACoreLibs/-/wikis/home){target="_blank"}.
The build process is very similar across the projects.
Note, that most of the dependencies will be downloaded automatically.

The primary repositories are:

- [VA](https://git.rwth-aachen.de/ita/VA){target="_blank"} : `https://git.rwth-aachen.de/ita/VA`
- [ITAGeometricalAcoustics](https://git.rwth-aachen.de/ita/ITAGeometricalAcoustics){target="_blank"} : `https://git.rwth-aachen.de/ita/ITAGeometricalAcoustics`

### Configuration options

The presented projects can be configured by a variety of options to meet the needs for different purposes.
This is done via CMake options.
Please refer to either the CMake GUIs option reference or the CMakeLists directly.

All CMake configuration options will have an `ITA_` prefix. If "Grouped" mode is activated in CMake GUI, the group name is `ITA` and all relevant options will be listed there.

### API documentation

If you want to familiarize yourself with the C++ API, some projects include the option to build a [doxygen documentation](https://www.doxygen.nl/index.html){target="_blank"}.
Usually this will also be buildable via a CMake target.
In case this is not available, you have to refer to the source code and its documentation.
