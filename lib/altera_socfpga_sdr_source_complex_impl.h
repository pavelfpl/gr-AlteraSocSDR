/* -*- c++ -*- */
/*
 * Copyright 2018 <+YOU OR YOUR COMPANY+>.
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

#ifndef INCLUDED_ALTERASOCSDR_ALTERA_SOCFPGA_SDR_SOURCE_COMPLEX_IMPL_H
#define INCLUDED_ALTERASOCSDR_ALTERA_SOCFPGA_SDR_SOURCE_COMPLEX_IMPL_H

#include <AlteraSocSDR/altera_socfpga_sdr_source_complex.h>
#include <gnuradio/thread/thread.h>
#include "fifo_buffer.h"

namespace gr {
  namespace AlteraSocSDR {

    class altera_socfpga_sdr_source_complex_impl : public altera_socfpga_sdr_source_complex
    {
     private:
       static fifo_buffer m_fifo; 		        // Define FIFO buffer
       static int fd_0;
       static bool m_exit_requested;
       
       std::string m_DeviceName;
       int m_Frequency;
       int m_SampleRate;
       int m_GainRF,m_GainIF;
       unsigned int m_WordLength;
       bool m_ScaleFactor;
       int m_ScaleConstant;
       unsigned int m_BufferLength;
       bool m_swap_iq;

       int m_QUANTIZATION_ONE;	      		    // int ...
       int m_TWO_QUANTIZATION_ONE;	  	        // int ...
       unsigned int m_QUANTIZATION_ONE_MASK; 	// unsigned int ...
       unsigned char m_MSB_MASK;

       gr::thread::thread _thread;
       boost::mutex fp_mutex;
       
       size_t m_itemsize;

       int msgdmaInit();
       void msgdmaDeInit();
    public:
      altera_socfpga_sdr_source_complex_impl(const std::string &DeviceName, int Frequency, int SampleRate, int GainRF, int GainIF, unsigned int WordLength, bool ScaleFactor,int ScaleConstant,unsigned int BufferLength,size_t itemsize, bool swap_iq);
      ~altera_socfpga_sdr_source_complex_impl();

      void altera_msgdma_sdr_source_wait(/* altera_socfpga_sdr_source_complex *obj*/);  // virtual reimplement ...
      bool stop();                         
      
      // Where all the action really happens ...
      int work(int noutput_items,
	       gr_vector_const_void_star &input_items,
	       gr_vector_void_star &output_items);
    };

  } // namespace AlteraSocSDR
} // namespace gr

#endif /* INCLUDED_ALTERASOCSDR_ALTERA_SOCFPGA_SDR_SOURCE_COMPLEX_IMPL_H */
