## VAPython

VAPython contains is a C++ extension for the Python3 interpreter that provides a (networked) interface to VA.


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


### Requirements

Python 3.7 or higher. The [setuptools](https://setuptools.pypa.io/en/latest/) and the [wheel](https://pypi.org/project/wheel/) package to create or use installer.


### Install

The VA binary packages come with wheel installers for the supported python versions 3.7 - 3.9 which are located in the *python* folder. The files have a similar name to *va-2021b-cp39-cp39-win_amd64.whl*. They can be installed using pip from the respective folder:

`pip install va-2021b-cp39-cp39-win_amd64.whl`

Afterwards you can start a VAServer and try the script in the *example* folder.


### Quick build guide

Before you can build VAPython you have to run CMake with option `ITA_VA_WITH_BINDING_PYTHON` as this among others will prepare the *setup.py* file.


#### Visual Studio

Running CMake creates a Visual Studio project called `VAPython`. "Building" this target will run a batch script (*setuptools_build_va_python.bat*), that will build the `va` package for Python and create a wheel installer file using setuptools (see below). Note, that this might only work in Release mode. You can modify the batch script, e.g. to specify the python version. Per default it uses the Python launcher *py* to create installers for multiple Python versions.


#### Setuptools

After running CMake you can use Pythons *setuptools* to build the module that is called `va`. Check the respective [Requirements](#requirements). Creating an installer is done using the following two steps:

`python setup.py build --force`

`python setup.py bdist_wheel`


#### Trouble shooting

If you get some errors like `module not found`, this usually refers to missing or incompatible shared libraries that the `va` module is trying to load upon import within the Python interpreter. Make sure the following files are available (i.e. copy them next to the va*.pyc file in the dist/lib/	site-packages folder or into the distribution zip file)
Windows:
`VABase.dll VANet.dll vista_base.dll vista_aspects.dll vista_inter_proc_comm.dll`
Other OS: replace `dll` with `so`
