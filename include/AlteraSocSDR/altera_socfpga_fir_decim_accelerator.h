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


#ifndef INCLUDED_ALTERASOCSDR_ALTERA_SOCFPGA_FIR_DECIM_ACCELERATOR_H
#define INCLUDED_ALTERASOCSDR_ALTERA_SOCFPGA_FIR_DECIM_ACCELERATOR_H

#include <AlteraSocSDR/api.h>
#include <gnuradio/sync_decimator.h>

namespace gr {
  namespace AlteraSocSDR {

    /*!
     * \brief <+description of block+>
     * \ingroup AlteraSocSDR
     *
     */
    class ALTERASOCSDR_API altera_socfpga_fir_decim_accelerator : virtual public gr::sync_decimator
    {
     public:
      typedef boost::shared_ptr<altera_socfpga_fir_decim_accelerator> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of AlteraSocSDR::altera_socfpga_fir_decim_accelerator.
       *
       * To avoid accidental use of raw pointers, AlteraSocSDR::altera_socfpga_fir_decim_accelerator's
       * constructor is in a private implementation
       * class. AlteraSocSDR::altera_socfpga_fir_decim_accelerator::make is the public interface for
       * creating new instances.
       */
      static sptr make(int decimation, const std::string &DeviceInputName = "/dev/altera_msgdma_wr0", const std::string &DeviceOutputName = "/dev/altera_msgdma_rd0", unsigned int WordLength = 16, bool ScaleInputFactor = false, bool ScaleOutputFactor = false, int ScaleConstant = 8192, size_t itemsizeInput = sizeof(gr_complex), size_t itemsizeOutput = sizeof(gr_complex));
    };

  } // namespace AlteraSocSDR
} // namespace gr

#endif /* INCLUDED_ALTERASOCSDR_ALTERA_SOCFPGA_FIR_DECIM_ACCELERATOR_H */

