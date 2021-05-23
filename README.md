# gr-AlteraSocSDR

**gr-AlteraSocSDR** repository contains blocks for **GnuRadio** (embedded) based on **Intel/Altera mSGDMA kernel driver**  
https://github.com/pavelfpl/altera_msgdma_st

## Building
>This module requires embedded **Gnuradio 3.7.x**, running on SocFPGA (Cyclone V / Arria 10). Blocks can be build on standard Linux desktop too, but can be only used for creating **Gnuradio Companion** flowcharts. Support for 3.8 branch is planned.
>Build is pretty standard:
```
mkdir build
cd build
cmake ..
make
sudo make install
sudo ldconfig
```
## Building local

>**Gnuradio 3.7.x** is installed to `$HOME/gr3.7`:

```
cd gr3.7
source gr3.7-source.env
cd ..
cd gr-PacketDeCoderSDR
mkdir build 
cd build
cmake ../ -Wno-dev -DCMAKE_INSTALL_PREFIX=~/gr3.7.13.4 
make install
sudo ldconfig
```
## Building embedded Gnuradio 3.7.x
>**Gnuradio 3.7.x** could be compiled on target embedded platform (without GUI). Tested with Gnuradio 3.7.13.4 and Debian 9/10 rootfs.

```
sudo apt install python-cheetah python-lxml python-thrift python-sphinx doxygen python2.7-numpy python-numpy libclalsadrv-dev libgsl-dev libzmq5 libfftw3-dev
pkg-config libcppunit-dev swig liblog4cpp5v5 liblog4cpp5-dev liborc-0.4-dev python-mako python-bitarray libboost-all-dev build-essential cmake
sudo aptitude installtexlive-latex-base texlive-latex-recommended ghostscript - Latex Support - documentation 
sudo aptitude install nfs-common - NFS compilation
# sudo aptitude install libvolk1-dev libvolk1.3 libvolk1-bin - older Gnuradio 3.7.11

git clone https://github.com/gnuradio/gnuradio.git
git checkout maint-3.7 [or selected version: git checkout v3.7.13.4]

!!! VOLK 1.4 is required for 3.7.13.x !!!
---------------------------------------------
git clone https://github.com/gnuradio/volk

cmake -DENABLE_DEFAULT=False -DENABLE_VOLK=True -DENABLE_PYTHON=True -DENABLE_TESTING=True -DENABLE_GNURADIO_RUNTIME=True -DENABLE_GR_BLOCKS=True
-DENABLE_GR_FFT=True -DENABLE_GR_FILTER=True -DENABLE_GR_ANALOG=True  -DENABLE_GR_DIGITAL=True -DENABLE_INTERNAL_VOLK=False -DENABLE_GR_FEC=True
-DENABLE_GR_TRELLIS=True -DENABLE_GR_AUDIO=True -DENABLE_GR_UTILS=TRUE -DENABLE_GR_CHANNELS=True -DENABLE_GR_CTRLPORT=True -DENABLE_DOXYGEN=True
-DENABLE_SPHINX=True -DENABLE_GR_DTV=True -DENABLE_GR_LOG=On -DCMAKE_INSTALL_PREFIX=/opt/gnuradio ../

make && sudo make install [target is set to /opt/gnuradio]

Create file with content: /etc/ld.so.conf.d/gnuradio.conf
---------------------------------------------------------
/opt/gnuradio/lib

Modify user .bashrc - add GNU Radio binaries to the search path ...
-------------------------------------------------------------------
GNURADIO_PATH=/opt/gnuradio
export PATH=$PATH:$GNURADIO_PATH/bin

# Add GNU Radio python libraries to python search path
if [ $PYTHONPATH ]; then
        export PYTHONPATH=$PYTHONPATH:$GNURADIO_PATH/lib/python2.7/dist-packages
else
        export PYTHONPATH=$GNURADIO_PATH/lib/python2.7/dist-packages
fi

export LD_LIBRARY_PATH=$GNURADIO_PATH/lib:$LD_LIBRARY_CONFIG
export PKG_CONFIG_PATH=$GNURADIO_PATH/lib/pkgconfig:$PKG_CONFIG_PATH
```
