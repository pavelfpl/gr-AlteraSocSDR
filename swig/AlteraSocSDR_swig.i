/* -*- c++ -*- */

#define ALTERASOCSDR_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "AlteraSocSDR_swig_doc.i"

%{
#include "AlteraSocSDR/altera_socfpga_sdr_source_complex.h"
#include "AlteraSocSDR/altera_socfpga_sdr_sink_complex.h"
#include "AlteraSocSDR/altera_socfpga_fir_accelerator.h"
#include "AlteraSocSDR/altera_socfpga_fir_decim_accelerator.h"
#include "AlteraSocSDR/altera_socfpga_fir_interp_accelerator.h"
%}


%include "AlteraSocSDR/altera_socfpga_sdr_source_complex.h"
GR_SWIG_BLOCK_MAGIC2(AlteraSocSDR, altera_socfpga_sdr_source_complex);
%include "AlteraSocSDR/altera_socfpga_sdr_sink_complex.h"
GR_SWIG_BLOCK_MAGIC2(AlteraSocSDR, altera_socfpga_sdr_sink_complex);
%include "AlteraSocSDR/altera_socfpga_fir_accelerator.h"
GR_SWIG_BLOCK_MAGIC2(AlteraSocSDR, altera_socfpga_fir_accelerator);
%include "AlteraSocSDR/altera_socfpga_fir_decim_accelerator.h"
GR_SWIG_BLOCK_MAGIC2(AlteraSocSDR, altera_socfpga_fir_decim_accelerator);
%include "AlteraSocSDR/altera_socfpga_fir_interp_accelerator.h"
GR_SWIG_BLOCK_MAGIC2(AlteraSocSDR, altera_socfpga_fir_interp_accelerator);
