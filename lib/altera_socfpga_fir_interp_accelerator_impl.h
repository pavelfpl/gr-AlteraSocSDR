/* -*- c++ -*- */
/* 
 * Copyright 2020 <+YOU OR YOUR COMPANY+>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifndef INCLUDED_ALTERASOCSDR_ALTERA_SOCFPGA_FIR_INTERP_ACCELERATOR_IMPL_H
#define INCLUDED_ALTERASOCSDR_ALTERA_SOCFPGA_FIR_INTERP_ACCELERATOR_IMPL_H

#include <AlteraSocSDR/altera_socfpga_fir_interp_accelerator.h>

// ---------------------------------------------------
// Altera MSGDMA internal configuration parameters ...
// ---------------------------------------------------
#define CONST_KMALLOC_MULTIPLE 128              // Default value - 128 ...
#define CONST_MIN_BLOCK_SIZE 512                // Default value - 512 ...
#define CONST_ARRAY_SIZE 2048                   // 4*CONST_MIN_BLOCK_SIZE 

#define CONST_ALTERA_MSGDMA_OK 1
#define CONST_ALTERA_MSGDMA_FAILED -1
#define CONST_ALTERA_MSGDMA_WRITE_FAILED -2
#define CONST_ALTERA_MSGDMA_READ_FAILED -3

#define CONST_SLEEP_INTERVAL_MICRO_SECONDS 1
#define CONST_SLEEP_INTERVAL_MILI_SECONDS 1

#define CONST_ALTERA_RESET_ARRAY_SIZE 256

namespace gr {
  namespace AlteraSocSDR {

    class altera_socfpga_fir_interp_accelerator_impl : public altera_socfpga_fir_interp_accelerator
    {
     private:
       static int fd_0;
       static int fd_1;	    
       
       int m_interpolation;
       std::string m_DeviceInputName, m_DeviceOutputName;
       unsigned int m_WordLength;
       bool m_ScaleInputFactor, m_ScaleOutputFactor;
       size_t m_itemsizeInput, m_itemsizeOutput;
       
       int m_QUANTIZATION_ONE;	      		// int ...
       int m_TWO_QUANTIZATION_ONE;	  	// int ...
       unsigned int m_QUANTIZATION_ONE_MASK; 	// unsigned int ...
       unsigned char m_MSB_MASK;
       int m_ScaleConstant;
       int m_msgdmaStatus;
       
       unsigned char buffer_write[CONST_ARRAY_SIZE];
       unsigned char buffer_read[CONST_ARRAY_SIZE];
       
       int msgdmaInit();
       void resetFIRFilter();
     public:
      altera_socfpga_fir_interp_accelerator_impl(int interpolation, const std::string &DeviceInputName, const std::string &DeviceOutputName, unsigned int WordLength, bool ScaleInputFactor, bool ScaleOutputFactor,int ScaleConstant, size_t itemsizeInput, size_t itemsizeOutput);
      ~altera_socfpga_fir_interp_accelerator_impl();
      bool stop();                       // virtual reimplement ... 

      // Where all the action really happens ...
      // ---------------------------------------
      int work(int noutput_items,
	       gr_vector_const_void_star &input_items,
	       gr_vector_void_star &output_items);
    };

  } // namespace AlteraSocSDR
} // namespace gr

#endif /* INCLUDED_ALTERASOCSDR_ALTERA_SOCFPGA_FIR_INTERP_ACCELERATOR_IMPL_H */

