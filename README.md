# meanie3D

This project provides a generic implementation of [mean-shift clustering](https://en.wikipedia.org/wiki/Mean_shift) on [CF-Metadata](http://cfconventions.org) compliant NetCDF data sets. The mean-shift algorithm is a robust unsupervised clustering algorithm with a wide range of applications. Meanie3D provides a multivariate implementation with no limits as to the number of variables that can be used in constructing the Featurespace. 

This software supports 2D as well as 3D data sets and comes with many configuration options. The code 
was written as part of research efforts of the [OASE HErZ group](http://www.herz-tb1.uni-bonn.de) and contains some modules that are specific to this application as a result. 

Meanie3D is implemented largely as a C++ template library with only a few object files (mostly from the numerical recipes library) for performance reason and uses OpenMP for parallelisation. 

The software package comes with a python frontend `meanie3D` which makes it very easy to use. The detection, tracking and post-processing steps (such as statistics, visualisation of tracks, source data and clusters as well as the track graph (a simple, graphical representation of a tracking dictionary for obtaining detailed view on the history of individual clusters and tracks) are all controlled through a single configuration file. 

## License
Meanie3D comes under [[MIT license]]. 

## Version
The latest stable version is v1.6.1. Versions are tagged.  

## Build Instructions
### Dependencies
Meanie3D comes with a number of dependencies that need to be installed prior to attempting installation:
* [Boost 1.56 or better](http://www.boost.org)
* [FLANN 1.8.0 or better](http://www.cs.ubc.ca/research/flann/)
* [Blitz++](http://sourceforge.net/projects/blitz/)
* [OpenMP (libomp)](https://www.openmp.org)
* [NetCDF 4.2 or better](http://www.unidata.ucar.edu/software/netcdf/) *including* the netcdf4-C++ API
* [HDF5](http://www.hdfgroup.org/HDF5/)
* [Python 2.7](https://www.python.org)
* NumPY (try running @pip install numpy@ or download and install from http://www.numpy.org)
* NetCDF4-python (try running @pip install netCDF4@ or download and install from http://unidata.github.io/netcdf4-python)
* [libradolan](http://meteo-ubonn.github.io/radolan/)

The following libraries may be used, if they are switched on with the appropriate flags
* Data format
  * [shapelib v1.3+](http://shapelib.maptools.org) - When present, certain utilities have additional options for writing out data in ESRI shapefile format.
* Visualization:
  * [Imagemagick](https://imagemagick.org)
  * [gnuplot](http://gnuplot.sourceforge.net)
  * [VTK 7.0 or better](http://www.vtk.org)
  * [Visit 3.0.0 or better](https://wci.llnl.gov/simulation/computer-codes/visit)
* Generated source code documentation:
  * [doxygen](https://www.doxygen.nl/download.html)

### Compiler Prerequisites
Meanie3D uses OpenMP by default (-DWITH_OPENMP=1) and requires an OpenMP enabled compiler, such as:
* GNU 9.0 or better (Linux, Mac)
* OpenMP/LLVM (Mac) - a clang implementation supporting OpenMP5 (http://clang-omp.github.io). Note that as of clang 10.0.0 OpenMP is supported out of the box, but you will still have to install the libomp library (`brew install libomp`) 

You can disable the OpenMP implementation adding the flags `-DWITH_OPENMP=0` to your cmake call (does nothing when using presets).

### Build and install the software
Meanie3D uses [cmake](https://cmake.org). Start by cloning the master branch (for an up-to date but possibly unstable version) or one of the stable releases.

  git clone https://github.com/meteo-ubonn/meanie3D.git
  cd meanie3D
  mkdir build
  cd build
  cmake -DPRESET=fast-vtk ..
  make install

### Build presets
A number of presets are provided to make the process easier. Those are selected via the -DPRESET=\<preset name\> 
flag to cmake. (Example: `cmake -DPRESET=dev-all ../meanie3D`). The available presets are:
* `dev-core`
  * Code optimizations for build type 'Debug'
  * Core functions and python frontend
* `dev-vtk`
  * Code optimizations for build type 'Debug'
  * Core functions and python frontend
  * VTK output enabled.
  * Documentation
* `dev-all`
  * Code optimizations for build type 'Debug'
  * Core functions and python frontend
  * VTK output enabled.
  * Visualization enabled.
  * RADOLAN, Satellite and KONRAD utilities 
  * Tests 
  * Documentation
* `prod-core`
  * Code optimizations for build type 'Release'
  * Core functions and python frontend.
* `prod-vtk`
  * Code optimizations for build type 'Release'
  * Core functions and python frontend
  * VTK output enabled.
* `prod-all`
  * Code optimizations for build type 'Release'
  * Core functions and python frontend
  * VTK output enabled.
  * Visualization enabled.
  * RADOLAN, Satellite and KONRAD utilities 
  * Tests 
  * Documentation
* `fast-core`
  * Code optimizations for build type 'MinSizeRel'
  * Core functions and python frontend.
* `fast-vtk`
  * Code optimizations for build type 'MinSizeRel'
  * Core functions and python frontend
  * VTK output enabled.
  * Tests 
  * Documentation
* `fast-all`
  * Code optimizations for build type 'MinSizeRel'
  * Core functions and python frontend
  * VTK output enabled.
  * Visualization enabled.
  * RADOLAN, Satellite and KONRAD utilities 
  * Tests 
  * Documentation
* `docker` 
  * Code optimizations for build type 'Release'
  * Core functions and python frontend
* `docker-vtk` 
  * Code optimizations for build type 'Release'
  * Core functions and python frontend
  * VTK output enabled.

The term "core functions" refers to the detection, tracking and track evaluation code. For local development in most cases, the preset `dev-vtk` is sufficient. For more aggressive optimisations, use `prod-vtk`. If you just need the core functions and none of the visuals, choose the `core` sets (`dev-core` or `prod-core`). For best performance, use the `fast` presets.

### Available build types
The following build types are available:
* Debug (debug symbols, no optimizations)
* Release (some debug symbols, optimized)
* RelWithDebInfo (debug symbols, optimized)
* MinSizeRel (minimal footprint, aggressive optimizations, no debug symbols)

To select a build type, use the flag `CMAKE_BUILD_TYPE` like so:

  cmake -DCMAKE_BUILD_TYPE=Release ../meanie3D

*Note:* there have been some problems on Linux with aggressive optimization and NetCDF. Your mileage may vary. You should try the `Release` build type in any event, since it speeds up performance a lot. If you observe unexpected problems in reading/writing NetCDF files, you may have fallen victim to the problem and revert to standard build (leave the -DCMAKE_BUILD_TYPE=Debug).  

**Note:** When using presets, the preset defines the build type. Setting this flag together with a preset has no effect on the preset's choice.

### Options
If you would like to select your own options, you can leave the PRESET parameter and switch components on and off selectively (Remember: setting any of the following options together with a -DPRESET=\<preset-name\> option does not alter the preset's settings).

#### `-DWITH_VTK=ON/OFF`
Because of the large footprint of the VTK package, the visualization code is disabled by default. While visualization is not necessary to run the algorithm, it can be useful to develop your parameters to have visual queues as to what is happening. Setting this flag will result in the following changes:
* The `meanie3D-cfm2vtk` binary will be compiled. This tool can visualize netCDF files in Visit/VTK
* In several places in the code, visualizable output for intermediary steps becomes available:
  * Cluster boundaries/outlines
  * Cluster 'modes' (algorithmic center of a cluster)
  * Cluster geometrical centers
  * Weight function used in detection
  * Mean-shift vectors 
  * Visualization of search window for mean shift.
  * Cluster weight function response
  * Individual variables of the original netCDF (by cluster)
  * Cluster tracks

#### `-DWITH_VISUALISATION=ON/OFF`
The meanie3D python program can utilize `Visit`, `gnuplot` and `imagemagick` to create images of the source data and results as part of batch processing. If this flag is set, CMAKE will make sure the necessary components are installed on your system. 

#### `-DWITH_OPENMP=ON/OFF`
In order to speed the process up, meanie3D uses OpenMP to parallelize it's computation. This option is switched on by default.

#### `-DWITH_TESTS=ON/OFF`
Meanie3D has a number of regression tests, that cover the core algorithms and collection classes. This will become important to you if you should decide to work on the core algorithms yourself. The tests are a good method of making sure you haven't broken anything critical. The unit tests can then be run by calling

  make test

#### `-DWITH_PYTHON=ON/OFF`
In addition to the C++ binaries, the installation also uses pip to put a python package in place. This adds an executable simply called `meanie3D`, which is a front-end to the core functions. It allows you to put your clustering and tracking parameters down in the form of a configuration file. The entire 
pipeline is handled based on this configuration file. *This is the recommended way to run the software*. For details on the configuration file format, please refer to the user manual. 

TODO: location of user manual

##### Python2 vs Python3

  Because the meanie3D python package contains code that is executed in Visit's own python CLI, you will have to use a python2 version if you want the visualisation code switched on. For your own visualisations from VTK output generated by the software, you may run Python3. In order to configure a specific python2 installation, use the following hint:

    cmake -DPython2_ROOT_DIR=/path/to/python/root ...

  For Python3 use ```-DPython3_ROOT_DIR``` instead. 

#### `-DWITH_DOCS=ON/OFF`
In order to start developing your own Meanie3D code, it might be useful to have API documentation of the various classes in the project. If you have doxygen installed, you can call the following make command to create a browsable HTML documentation in doc/html (open the file index.html). 

  make docs

#### `-DWITH_RADOLAN_UTILS=ON/OFF`
This will result in compilation of the `meanie3D-radolan2cfm` utility, which converts files in RADOLAN format to a cf-metadata compliant netCDF file, which then can be used to run the tracking. 

#### `-DWITH_SATELLITE=ON/OFF`
When this option is switched on, the package comes with binaries to perform some conversion on satellite data. Those binaries were provided in the context of research work for the OASE project. The following binaries will be provided if this flag is set:
* `meanie3D-satconv` - Converts spectral radiance to equivalent brightness temperature or vice versa
* `meanie3D-parallax_correction` - Applies parallax correction to mseviri satellite data in OASE composite files.
In addition the detection code has special options for calculating a convective initiation score (Mecikalski, John R., and Kristopher M. Bedka. “Forecasting Convective Initiation by Monitoring the Evolution of Moving Cumulus in Daytime GOES Imagery.” Monthly Weather Review 134, no. 1 (2006). https://doi.org/10.1175/MWR3062.1.). This can be used to track convective cells in SEVIRI satellite data. 

#### `-DWITH_KONRAD_UTILS=ON/OFF`
The package comes with a tool `meanie3D-trackstats-conrad` which analyses KONRAD tracks in a way that makes the data comparable to meanie3D data. This is a specialized tool developed in the context of the OASE project.

### Additional options for debugging
In addition to runtime flags to the detection program, a few more options exist to write debug output.

#### `-DDEBUG_GRAPH_AGGREGATION=ON`
If this option is selected, a blow by blow description of the algorithms decisions on aggregating clusters from mean-shift vectors is given to stdout. This can sometimes be helpful to understand how certain clusters were formed.

#### `-DWRITE_CI_SCORE=ON`
If this option is selected, details of the CI (convective initiation) score calculation are written out to VTK files. Tracking regions of elevated CI score was one of the ideas for the OASE group. Only useful when running the algorithm on SEVIRI satellite data and the flag `--ci-use-walker-mecikalski` is set on calling `meanie3D-detect`. 

#### `-DWRITE_FEATURESPACE=ON`
The featurespace is the data set constructed from all dimensions plus the variables in question. If this flag is set, this data set is written out as VTK files at construction and throughout the different filtering steps. This information can be useful to understand choices in building your featurespace and in filtering it. NOTE: requires the code to be compiled with -DWITH_VTK=ON or an adequate preset. 

#### `-DWRITE_ZEROSHIFT_CLUSTERS=ON`
Areas with mean-shift vectors that are zero after discretiziation are aggregated and serve as condensation points for the vector graph analysis. Those areas are called 'zero-shift' clusters. When this flag is set, those clusters are written to disk as .vtk files. NOTE: requires the code to be compiled with -DWITH_VTK=ON or an adequate preset. 

#### `-DWRITE_OFF_LIMITS_MASK=ON`
The CF-Metadata standard for NetCDF files allows to have a value for areas outside of the valid measurements. This applies for example to values outside the maximum range of a radar etc. When this flag is set, a file is written out
in VTK format, that contains a mask for such areas (from the original data set). NOTE: requires the code to be compiled with -DWITH_VTK=ON or an adequate preset. 

#### `-DWRITE_MODES=ON`
When this flag is set, a file is written out which contains the cluster modes. NOTE: requires the code to be compiled with -DWITH_VTK=ON or an adequate preset. 

### Uninstall the software
Navigate to the build directory. Invoke the following command:
  
  make uninstall

The command will remove the C++ binaries, meanie3D dynamic library and executables from your system. 

## Frequently Asked Quesions 

### I'm having trouble compiling the code
The code has been tested on MacOS X and Suse Linux. Feel free to report problems using the "New Issue":http://git.meteo.uni-bonn.de/projects/meanie3d/issues/new link above. Please prepend "[BUILD PROBLEM]:" to your subject line. 

### NetCDF/HDF5 trouble
Versions - make sure the NetCDF version is 4.2 or better. Also make certain that you have a HDF5 installation to match it. If you want to make sure, uninstall HDF5 and NetCDF and re-install them from source (HDF5 first, then NetCDF) Commonly there is a problem with the NetCDF C++ API - NetCDF has a legacy C++ API from NetCDF3, which does not work with Meanie3D. Make sure you have the correct one installed (http://www.unidata.ucar.edu/downloads/netcdf/ftp/netcdf-cxx4-4.2.tar.gz) 

### CMAKE does not find your installed packages
If cmake has trouble locating packages you installed, check the files in the cmake_modules directory and adjust the search paths there instead of hardwiring paths into the CMakeLists.txt. If your system layout is not considered and you have to make adjustments to the search paths, please send a copy of your Find\<XZY\>.cmake file to simon@webtecc.com so I can include your changes in the next release and maybe save someone else the trouble.

## Visualisation
The C++ binaries produce certain files when VTK is enabled. In order to generate imagery or tracking movies, meanie3D relies on executing python scripts in VisIt (https://wci.llnl.gov/simulation/computer-codes/visit). You will have to install this software to get access to the visualization. For details on what is possible by scripting VisIt with python, see the following tutorial: https://www.visitusers.org/index.php?title=VisIt-tutorial-Python-scripting. 

## More information
Check out the Main Wiki Page at http://git.meteo.uni-bonn.de/projects/meanie3d/wiki for details on how the method works, file formats and what the individual binaries do. 