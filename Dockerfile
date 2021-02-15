FROM debian:stable

RUN apt-get -y update --fix-missing
RUN apt-get -y upgrade 
RUN apt-get -y dist-upgrade
RUN apt-get -y install software-properties-common
RUN apt-get -y update 
RUN apt-get -y install \
wget git cmake \
gcc g++ libomp5 \
python python-pip \
libboost-all-dev libflann1.9 libflann-dev blitz++ \
shapelib libhdf5-dev netcdf-bin libnetcdf-dev libnetcdf-c++4 libnetcdf-c++4-dev zlib1g zlib1g-dev 
RUN pip install setuptools netcdf4 external utils

# libradolan
RUN git clone https://github.com/JuergenSimon/radolan.git
RUN cd radolan && cmake . && make install && cd .. && rm -rf radolan

# Meanie3D
RUN git clone --recurse-submodules --depth=1 https://github.com/JuergenSimon/meanie3D
WORKDIR /meanie3D
RUN git config remote.origin.fetch "+refs/heads/*:refs/remotes/origin/*" && git fetch --all
RUN git checkout --track remotes/origin/dockerize && git pull
RUN git pull && cmake -DPRESET=docker . && make install 

# Cleanup
WORKDIR /
RUN rm -rf meanie3D
RUN apt-get remove -y wget git cmake
RUN apt autoremove -y

# Prepare for runtime
RUN mkdir /data
ENV LD_LIBRARY_PATH=/usr/local/lib
ENTRYPOINT ["/usr/local/bin/meanie3D"]