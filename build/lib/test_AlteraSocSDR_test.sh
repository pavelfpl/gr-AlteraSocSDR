#!/usr/bin/sh
export VOLK_GENERIC=1
export GR_DONT_LOAD_PREFS=1
export srcdir=/home/pavelf/gnuradio_addons/gr-AlteraSocSDR/lib
export GR_CONF_CONTROLPORT_ON=False
export PATH=/home/pavelf/gnuradio_addons/gr-AlteraSocSDR/build/lib:$PATH
export LD_LIBRARY_PATH=/home/pavelf/gnuradio_addons/gr-AlteraSocSDR/build/lib:$LD_LIBRARY_PATH
export PYTHONPATH=$PYTHONPATH
test-AlteraSocSDR 
