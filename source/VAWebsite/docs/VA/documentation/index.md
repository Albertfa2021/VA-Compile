# Documentation

Virtual Acoustics is a powerful tool for the auralization of virtual acoustic scenes and the reproduction thereof. Getting started with VA includes the following important steps

- [**Configuring**](configuration.md) VA before startup
- This includes selecting for your scenario the respective
    - [**Rendering classes**](rendering.md)
    - [**Reproduction classes**](reproduction.md)
- During runtime, use a `Binding` to
    - [**Control**](control.md) VA
    - Set up a [**scene**](scene.md)
- If desired, auralizations can be
    - [**Recorded**](recording.md)
    - Rendered [**offline**](recording.md#offline-rendering-and-capturing)

If you want to get a quick insight on VA by running a basic example on your machine check out the [quick start guide](#quick-start-guide). Otherwise, you should start reading the [preface](#preface), before checking the details in the setions linked in the list above.

----------

## Quick start guide

#### Installation
Since VA is a reasearch software, we currently only offer binary packages for **Windows**. If you are interested in using VA on another operating system look [here](../download.md#linux-mac-osx-and-other). The installation for Windows is straight forward:

1. [Download](../download.md) the *full VA package for Windows*
2. Unzip the respective folder anywhere on your hard drive

#### Starting the VAServer
Go to the root folder of your VA installation. Then run the `run_VAServer.bat` batch file, to start the VAServer with default configuration.


#### Setup and modify a scene using the VAMatlab client
Go to the `VAMatlab_20XXy` (formerly `matlab`) subdirectory in your VA root folder. Run the script `VA_example_simple.m` from Matlab. The first time using VA, depending on the version, you might be asked to point to your VA directory. In this case, use the `Browse` button to point to the root folder of your VA installation. After the binaries have been found, you can close the window and the script will continue running.
This will create a simple scene with one source and receiver under free-field conditions. The output signal is binaural, so using headphones is advised. You should hear a voice repeatedly saying "Welcome to Virtual Acoustics".

Now you can modify the scene. For example you can move the source in front of the receiver:
```
 va.set_sound_source_position( S, [ 0 1.7 -2 ] )
```
Note, that VA uses OpenGL coordinates!

----------

## Preface
The overall design goal aimed at keeping things as simple as possible. However, certain circumstances do not allow further simplicity due to their complexity by nature. VA addresses professionals and is mainly used by scientists. Important features are never traded for convenience if the system's integrity is at stake. Hence, getting everything out of VA will require profound understanding of the technologies involved. It is designed to offer highest flexibility which comes at the price of a demanding configuration. At the beginning, configuring VA is not trivial especially if a loudspeaker-based audio reproduction shall be used.

The usage of VA can often be divided into two user groups

- those who seek experiments with spatial audio using conventional playback over headphones
- those who want to employ VA for a sophisticated loudspeaker setup for (multi modal) listening experiments and Virtual Reality applications

For the first group of users, there are some simple setups that will already suffice for most of the things you aspire. Such setups include, for example, a configuration for binaural audio rendering over a non-equalized off-the-shelf pair of headphones. Another configuration example contains a self-crafted interactive rendering application that exchanges pre-recorded or simulated FIR filters using Matlab or Python scripts for different purposes such as room acoustic simulations, building acoustics, A/B live switching tests to assess the influence of equalization. The configuration effort is minimal and works out of the box if you start a VA command line server with the corresponding core configuration file. If you consider yourself as part of this group of users skip the configuration part and have a look at the [examples](https://git.rwth-aachen.de/ita/VAMatlab/-/tree/master/matlab){target="_blank"}. Thereafter, read the [control](control.md) and the [scene handling](scene.md) section. Additional examples are provided by the [ITA-Toolbox](https://www.ita-toolbox.org/){target="_blank"} (see folder *<...>\applications\VirtualAcoustics\VA*).

If you are willing to dive deeper into the VA framework you are probably interested in how to adapt the software package for your purposes. The following sections will describe how you can set up VA for your goal from the very beginning. 

----------

