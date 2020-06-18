FROM debian:stable

ENV PACKAGES=
RUN apt-get -y update --fix-missing
RUN apt-get -y upgrade 
RUN apt-get -y dist-upgrade
RUN apt-get -y install software-properties-common
RUN apt-get -y update 
RUN apt-get -y install \
git \
gcc g++ \
python3 python3-pip \
cmake \
doxygen \
libflann1.9 libflann-dev \
libboost-all-dev\ 
blitz++ \
shapelib \
libhdf5-dev \
netcdf-bin libnetcdf-dev libnetcdf-c++4 python3-netcdf4 \
zlib1g zlib1g-dev

RUN pip3 install setuptools

# Build NetCDF-CXX (always an extra bloody sausage with this package...)
RUN apt-get -y install wget
RUN wget --quiet https://github.com/Unidata/netcdf-cxx4/archive/v4.2.1.tar.gz
RUN tar xvzf v4.2.1.tar.gz
RUN cd netcdf-cxx4-4.2.1 && ./configure && make install && cd ..
RUN rm -rf netcdf-cxx4-4.2.1 && rm v4.2.1.tar.gz

# Install python-netcdf4 from source
#RUN wget https://pypi.python.org/packages/source/n/netCDF4/netCDF4-1.2.3.1.tar.gz#md5=24fc0101c7c441709c230e76af611d53
#RUN tar xvzf netCDF4-1.2.3.1.tar.gz
#RUN cd netCDF4-1.2.3.1 && python setup.py build && python setup.py build && cd ..
#RUN rm -rf netCDF4*

# Visualisation
RUN pip3 install Cython h5py netcdf4
RUN apt-get -y --fix-missing  install gnuplot vtk6 libvtk6-dev
RUN wget --quiet http://portal.nersc.gov/project/visit/releases/2.10.0/visit2_10_0.linux-x86_64-rhel6-wmesa.tar.gz
RUN wget --quiet http://portal.nersc.gov/project/visit/releases/2.10.0/visit-install2_10_0
RUN chmod a+x visit-install2_10_0
RUN echo "1" | ./visit-install2_10_0 2.10.0 linux-x86_64-rhel6-wmesa /usr/local/visit
ENV VISIT_EXECUTABLE=/usr/local/visit/bin/visit
RUN rm -rf visit*

# Meanie3D
RUN git clone --recurse-submodules --depth=1 https://github.com/meteo-ubonn/meanie3D.git
WORKDIR /meanie3D
RUN cmake -DWITH_OPENMP=1 -DWITH_VTK=1 -DCMAKE_BUILD_TYPE=Release . 
RUN make install 

# Cleanup
WORKDIR /
RUN rm -rf meanie3D
RUN apt autoremove -y

# Prepare for runtime
RUN mkdir /data
ENV LD_LIBRARY_PATH=/usr/local/lib
ENTRYPOINT ["/usr/local/bin/meanie3D"]