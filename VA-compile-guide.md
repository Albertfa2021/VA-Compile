一定注意！我要编译的是VA2022a版本！
https://git.rwth-aachen.de/ita/VA/-/tree/VA_v2022a?ref_type=tags

Quick build guide
VA is a collective of repositories. Clone VA using the --recursive flag, since it mostly consists of submodules.
Use CMake and the top-level CMakeLists.txt of the VA project. It will generate a poject file with alle sub-projects included.
VA makes heavy use of ViSTA（https://sourceforge.net/projects/vistavrtoolkit/files/）, the Virtual Reality Toolkit developed by the Virtual Reality Group of the IT Center, RWTH Aachen University.
Hence, the build environment requires the VistaCMakeCommon extension for CMake in order to define and resolve all required dependencies.
For more information, see the Wiki pages of ITACoreLibs.

Working with submodules
If you do not want to make any changes but update to the latest HEAD revision, use the command git submodule update --remote in order to also update the submodules.
If you have already made changes, updating will fail.
If you want to make changes to the submodule project, browse into the directory and checkout a branch since a submodule is per default detached from HEAD, i.e. the master branch:
git checkout master
Now make your changes, add, commit and push from this location as usual.
For convenience, GIT also provides a batch command that performs the call for each submodule:
git submodule foreach git checkout master
For switching branches while skipping if a specific branch is not present, use
git submodule foreach "git checkout develop || true"

Switching to a specific version
To use a spefic version of a VA submodule, say VACore, you have to cd VACore and git checkout v2017.a. From now on, take care when updating.

Cleaning up
Sometimes, submodules remain in a dirty state because files are created by the build environment - which are not under version control.
If you want to clean up your working directory, use git submodule foreach git clean -f -n, and remove the -n if your are sure to remove the listed files in the submodules.
Afterwards, a git submodule status should return a clean copy.:

Deploy
To generate a VA binary package (deploy), the following steps must be performed:

Check your VA configuration using CMake. You can modify the install target directory if you want, otherwise it is set to the dist folder of the VA source tree.
Check / uncheck core, bindings, modules, extensions etc. (according to what you want to include) and generate the project file.
Run make install, i.e. Open Visual Studio and build the INSTALL project (Release Build). It will automatically assemble all required libraries (and more) and copies them to the install target directory / the dist folder. It might be necessary to re-run this project until no errors occur, this can happen if the dependencies were not set in the correct order.
In case the Matlab bindings were included, the VA Matlab class must be generated manually. This can be done by running the VA_build.m script in Matlab afterwards. A copy of the script can be found in the matlab folder of the distribution in the install target directory.
Finally, the distribution can be zipped manually and is ready-to-use. Users just have to unzip the VA binary package anywhere, it is a portable distribution that can be executed right away.