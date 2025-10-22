---
title: Support
---

# Support section {style="text-align: center;"}

Get help
{style="font-size:1.2em;text-align: center;"}

<hr class="accent-hr">

## Frequently Asked Questions (FAQ)

??? quote "VA keeps nagging that the file I want to load can not be found"

    If you load a file from hard drive, it has to be placed on the computer where the VA server is running - not necessarily on the client side, if you are using a remote connection.
    Use relative paths for loading files only, avoid absolute paths.
    Add search paths (folders where your files can be found) to help VA find the files.
    You can do this in the configuration file of the VA server, or you can interactively add the search path over the VA interface.

??? quote "I can't connect to my VA server, although I am certain that the IP and port is correct. "

    It is very likely, that your firewall has blocked the program due to insufficient privileges.
    This happens a lot in managed networks like universities, where you are not allowed to open a TCP/IP port.
    Please contact your network administration or check, if your firewall settings are blocking VA.

    Also, check if the versions are matching and you are not mixing different VA binary packages of different compiler versions. Especially if dynamic linking to ViSTA and VA libraries is used, this can lead to unpredictable behavior, because the DLLs will have the same name - but are incompatible.
    It is strongly recommended to use static linking for bindings like Python or Matlab for this reason.

??? quote "My audio output sounds strange"

    Check your sampling rates and buffer sizes.

    Do not overload your computer.
    There are limits on how much sound sources and sound receivers can be processed depending on the number of rendering and reproduction modules you've instantiated.
    Use ASIO drivers. 

??? quote "Can a VA server run on a mobile device, for example on an Android platform?"

    No.

??? quote "I am trying to use a VA binding, but I get a 'module not found' or 'library not found' error."

    You are missing a dependent shared library (dll on Windows).
    Make sure to include the dependencies if you copy libraries.
    If you are a developer, the Dependency Walker is an excellent supporter for solving these problems on Windows platforms.
    Otherwise consider switching to static linking to avoid this problem with the VA bindings.

??? quote "Can I use `SOFA` HRTFs in VA?"

    Not directly, but you can convert `SOFA` HRTF files into `OpenDAFF` using the [ITA-Toolbox for Matlab](http://www.ita-toolbox.org/){target="_blank"}.
    Create an `itaHRTF` and load the `SOFA` file, then export using `ita_write_daff`.

## Issue tracker

Issues are very likely to appear.
VA is a prototyping software, and the usage can hardly be foreseen by the developers.
However, it is important to identify if the issues you have are usage-related or require a bug fix in the code.
If you are insecure, go through the [FAQ](#frequently-asked-questions-(FAQ)).

If you have trouble configuring and using VA, there is a whole section about that in the [documentation page](documentation/index.md), which should be thoroughly read first.

If you are sure it is a bug that should be fixed, then

- does it affect the interface? File it here: [VABase issues](https://git.rwth-aachen.de/ita/VABase/issues)
- does it affect the Python binding? File it here: [VAPython issues](https://git.rwth-aachen.de/ita/VAPython/issues)
- does it affect the Matlab binding? File it here: [VAMatlab issues](https://git.rwth-aachen.de/ita/VAMatlab/issues)
- does it affect the functionality[^1] of VA? File it here: [VACore issues](https://git.rwth-aachen.de/ita/VACore/issues)

[^1]:
    In most cases, VA and its rendering or reproduction modules will behave oddly, which usually points to a problem in the core of VA.
    Also, if you do not know where to file your issue, use the VACore as it is the most active part of development.

## VA has no official support team

We are very sorry to inform you, that there is no official support team in place.
We, the developers of VA, are scientists and are paid for research, not commercial or private support activity.
However, if you are willing to start a research alliance or fund a joint project that gives us opportunities to spend time on your ideas, we will be open to discuss your proposal.
Please contact the administration of the [Institute for Hearing Technology and Acoustics (IHTA), RWTH Aachen University](http://www.akustik.rwth-aachen.de/){target="_blank"}, in this case.
