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


#ifndef INCLUDED_ALTERASOCSDR_ALTERA_SOCFPGA_SDR_SINK_COMPLEX_H
#define INCLUDED_ALTERASOCSDR_ALTERA_SOCFPGA_SDR_SINK_COMPLEX_H

#include <AlteraSocSDR/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
  namespace AlteraSocSDR {

    /*!
     * \brief <+description of block+>
     * \ingroup AlteraSocSDR
     *
     */
    class ALTERASOCSDR_API altera_socfpga_sdr_sink_complex : virtual public gr::sync_block
    {
     public:
      typedef boost::shared_ptr<altera_socfpga_sdr_sink_complex> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of AlteraSocSDR::altera_socfpga_sdr_sink_complex.
       *
       * To avoid accidental use of raw pointers, AlteraSocSDR::altera_socfpga_sdr_sink_complex's
       * constructor is in a private implementation
       * class. AlteraSocSDR::altera_socfpga_sdr_sink_complex::make is the public interface for
       * creating new instances.
       */
      static sptr make(const std::string &DeviceName = "/dev/altera_msgdma_wr0" ,int Frequency = 100000000, double SampleRate = 1000000, int GainRF = 0, int GainIF = 0, unsigned int WordLength = 14, bool ScaleFactor = false, int ScaleConstant = 8192, unsigned int BufferLength = 1048576,size_t itemsize = sizeof(gr_complex), bool swap_iq = false, float gainCorrection = 1.0);
    };

  } // namespace AlteraSocSDR
} // namespace gr

#endif /* INCLUDED_ALTERASOCSDR_ALTERA_SOCFPGA_SDR_SINK_COMPLEX_H */

