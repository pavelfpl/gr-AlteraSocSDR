/* -*- c++ -*- */
/* 
 * Copyright 2020 Pavel Fiala. 
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "altera_socfpga_fir_decim_accelerator_impl.h"

#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

// ---------------------------------------
// IOCLT include - altera_msgdma_ioctl ...
// ---------------------------------------
#include "altera_msgdma_ioctl.h"

namespace gr {
  namespace AlteraSocSDR {
    
    using namespace std;
    // ------------------------------
    // -- Define static variables ...
    // ------------------------------
    int altera_socfpga_fir_decim_accelerator_impl::fd_0 = -1;
    int altera_socfpga_fir_decim_accelerator_impl::fd_1 = -1;
    

    altera_socfpga_fir_decim_accelerator::sptr
    altera_socfpga_fir_decim_accelerator::make(int decimation, const std::string &DeviceInputName, const std::string &DeviceOutputName, unsigned int WordLength, bool ScaleInputFactor, bool ScaleOutputFactor, int ScaleConstant, size_t itemsizeInput, size_t itemsizeOutput)
    {
      return gnuradio::get_initial_sptr
        (new altera_socfpga_fir_decim_accelerator_impl(decimation, DeviceInputName, DeviceOutputName, WordLength, ScaleInputFactor, ScaleOutputFactor, ScaleConstant, itemsizeInput, itemsizeOutput));
    }

     /*
     * ---------------------------
     * The private constructor ...
     * ---------------------------
     */
    altera_socfpga_fir_decim_accelerator_impl::altera_socfpga_fir_decim_accelerator_impl(int decimation, const std::string &DeviceInputName, const std::string &DeviceOutputName, unsigned int WordLength, bool ScaleInputFactor, bool ScaleOutputFactor,int ScaleConstant, size_t itemsizeInput, size_t itemsizeOutput)
      : gr::sync_decimator("altera_socfpga_fir_decim_accelerator",
              gr::io_signature::make(1, 1, itemsizeInput  /* <+MIN_IN+>, <+MAX_IN+>, sizeof(<+ITYPE+>)  */),
              gr::io_signature::make(1, 1, itemsizeOutput /* <+MIN_OUT+>, <+MAX_OUT+>, sizeof(<+OTYPE+>)*/), 
	      decimation /* <+decimation+> */), m_decimation(decimation), m_DeviceInputName(DeviceInputName), m_DeviceOutputName(DeviceOutputName), m_WordLength(WordLength), m_ScaleInputFactor(ScaleInputFactor), m_ScaleOutputFactor(ScaleOutputFactor), m_ScaleConstant(ScaleConstant), m_itemsizeInput(itemsizeInput),m_itemsizeOutput(itemsizeOutput)
      
    {
      
      if(m_WordLength < 10 || m_WordLength > 16){
         cerr << "WordLength variable was incorrectly set. Defaulting to 16 ..." << endl;
         m_WordLength = 16;
      }

      // Select and set m_MSB_MASK (depends on m_WordLength) ...
      // -------------------------------------------------------
      switch(m_WordLength){
        case 10: m_MSB_MASK = 0x03; break;
        case 12: m_MSB_MASK = 0x0F; break;
        case 14: m_MSB_MASK = 0x3F; break;
        case 16: m_MSB_MASK = 0xFF; break;
        default: m_MSB_MASK = 0x3F; break;
      }

      // Set QUANTIZATION parameters ...
      // -------------------------------
      m_QUANTIZATION_ONE = (1 << m_WordLength-1);   	// unsigned integer -> 2^(WordLength-1) ...
      m_TWO_QUANTIZATION_ONE = 2 * m_QUANTIZATION_ONE;	// unsigned integer	...
      m_QUANTIZATION_ONE_MASK = (1 <<  m_WordLength-1); // unsigned integer ...

      // Init msgdma --> open Linux device driver ...
      // --------------------------------------------
      m_msgdmaStatus = msgdmaInit();
      
      // Reset FIR filter ...
      // --------------------
      if(m_msgdmaStatus == CONST_ALTERA_MSGDMA_OK) resetFIRFilter();  
    }
    
    // msgdmaInit function [private] ...
    // ---------------------------------
    int altera_socfpga_fir_decim_accelerator_impl::msgdmaInit(){

      int blockSize = CONST_MIN_BLOCK_SIZE;
      // int bufferSize = CONST_KMALLOC_MULTIPLE*CONST_MIN_BLOCK_SIZE;

      // Open /dev/altera_msgdma_rdX / O_RDONLY ...
      // ------------------------------------------
      if((fd_0 = open(m_DeviceOutputName.c_str(),O_RDONLY)) == -1){ 						// "/dev/altera_msgdma_rdX"
          cerr << "ERROR: could not open \""<<m_DeviceOutputName<<"\" for reading ..." <<endl;			// \"/dev/altera_msgdma_rdX\"
          return CONST_ALTERA_MSGDMA_READ_FAILED;
      }

      // Open /dev/altera_msgdma_wrX / O_WRONLY ...
      // ------------------------------------------
      if((fd_1 = open(m_DeviceInputName.c_str(),O_WRONLY)) == -1){ 						// "/dev/altera_msgdma_wrX"
          cerr << "ERROR: could not open \""<<m_DeviceInputName<<"\" for writing ..." <<endl;			// \"/dev/altera_msgdma_wrX\"
          return CONST_ALTERA_MSGDMA_WRITE_FAILED;
      }
      
      // Set Altera MSGDMA internal parameters ...
      // -----------------------------------------
      // ioctl(fd_0,IO_ALTERA_MSGDMA_SET_BUFFER_SIZE,&bufferSize);
      // ioctl(fd_1,IO_ALTERA_MSGDMA_SET_BUFFER_SIZE,&bufferSize);
      
      ioctl(fd_0,IO_ALTERA_MSGDMA_SET_MIN_BLOCK_SIZE,&blockSize);
      ioctl(fd_1,IO_ALTERA_MSGDMA_SET_MIN_BLOCK_SIZE,&blockSize);
      
      // Reset Altera MSGDMA - option ...
      // --------------------------------
      ioctl(fd_0,IO_ALTERA_MSGDMA_STOP_RESET);
      ioctl(fd_1,IO_ALTERA_MSGDMA_STOP_RESET);
      
      // Return success ...
      // ------------------
      return CONST_ALTERA_MSGDMA_OK;
    }

    // resetFIRFilter function [private] ...
    // -------------------------------------
    void altera_socfpga_fir_decim_accelerator_impl::resetFIRFilter(){
      
	unsigned char zero_buffer[CONST_ALTERA_RESET_ARRAY_SIZE];
	memset(zero_buffer,0x00,CONST_ALTERA_RESET_ARRAY_SIZE);  // Zero fill buffer 256/4 = 64 ...
	
	// ----------------------------
	// Reset FIR filter - START ...
	// ----------------------------
	if(write(fd_1,zero_buffer,CONST_ALTERA_RESET_ARRAY_SIZE)!=CONST_ALTERA_RESET_ARRAY_SIZE){
	   cerr << "ERROR: write msgdma buffer"<<endl;
	   return;
	}
  
	if(read(fd_0,zero_buffer,CONST_ALTERA_RESET_ARRAY_SIZE)!=CONST_ALTERA_RESET_ARRAY_SIZE){
	   cerr << "ERROR: read msgdma buffer"<<endl; 
	   return; 
	}
	
	// ---------------------------
	// Reset FIR filter - STOP ...
	// ---------------------------
    }
    
    /*
     * Our virtual destructor ...
     * --------------------------
     */
    altera_socfpga_fir_decim_accelerator_impl::~altera_socfpga_fir_decim_accelerator_impl(){
      
        // Close /dev/altera_msgdma_rdX  ...
        // ---------------------------------
        if(fd_0 != -1) close(fd_0);
	
        // Close /dev/altera_msgdma_wrX  ...
        // ---------------------------------
        if(fd_1 != -1) close(fd_1);
      
    }
    
    bool altera_socfpga_fir_decim_accelerator_impl::stop(){
        
        // Only for debug  ...
        // -------------------
        cout<< "Calling STOP - altera_socfpga_fir_decim_accelerator_impl ... "<< endl;
        
        // Close /dev/altera_msgdma_rdX  ...
        // ---------------------------------
        if(fd_0 != -1) close(fd_0);
	
        // Close /dev/altera_msgdma_wrX  ...
        // ---------------------------------
        if(fd_1 != -1) close(fd_1);
        
        return true;
    }
    
    
    int altera_socfpga_fir_decim_accelerator_impl::work(int noutput_items,
			  gr_vector_const_void_star &input_items,
			  gr_vector_void_star &output_items){
      
        /*
	const <+ITYPE+> *in = (const <+ITYPE+> *) input_items[0];
        <+OTYPE+> *out = (<+OTYPE+> *) output_items[0];
	*/

	// Set variables ...
	// -----------------
	size_t count_in = 0, count_iq = 0;
	
	int input_items_decim = noutput_items*m_decimation; // Set decimation factor ...
	
	// Set pointers ...
	// ----------------
	const gr_complex *in_fc32 = 0;
        const complex<int16_t> *in_sc16 = 0; 
	
	gr_complex *out_fc32 = 0;
        complex<int16_t> *out_sc16 = 0; 
	        	
	if(m_itemsizeInput == sizeof(gr_complex)){
	   in_fc32 = reinterpret_cast<const gr_complex *>(input_items[0]);        	     	  // -- <+OTYPE+> *out = (<+OTYPE+> *) output_items[0];
	}else{
	   in_sc16 = reinterpret_cast<const complex<int16_t> *>(input_items[0]);        	  // -- <+OTYPE+> *out = (<+OTYPE+> *) output_items[0];
	}

	if(m_itemsizeOutput == sizeof(gr_complex)){
	   out_fc32 = reinterpret_cast<gr_complex *>(output_items[0]);      			  // -- <+OTYPE+> *out = (<+OTYPE+> *) output_items[0];
	}else{
	   out_sc16 = reinterpret_cast<complex<int16_t> *>(output_items[0]);    		  // -- <+OTYPE+> *out = (<+OTYPE+> *) output_items[0];
	}
	
	// Test Altera MSGDMA status - CONST_ALTERA_MSGDMA_OK ...
	// ------------------------------------------------------
	if(m_msgdmaStatus!=CONST_ALTERA_MSGDMA_OK){
	   consume_each(0);
	   return 0;
	}
	
	// ------------------------------
	// -- Do <+signal processing+> --
	// ------------------------------
	for(int i=0;i<input_items_decim;i++){
	    
	    int i_in_0 = 0, i_in_1 = 0;
	    unsigned short data_out_0 = 0x0000, data_out_1 = 0x0000;
	  
	    if(m_itemsizeInput == sizeof(gr_complex)){
	      
	      // 1] Scale ...
	      // ------------
	      if(m_ScaleInputFactor){
		 i_in_0 = (in_fc32->real() * float(m_ScaleConstant /* m_QUANTIZATION_ONE */));
		 i_in_1 = (in_fc32->imag() * float(m_ScaleConstant /* m_QUANTIZATION_ONE */));
	      }else{
		 i_in_0 = in_fc32->real();
		 i_in_1 = in_fc32->imag();
	      }
	     
	      // 2] Saturate ...
	      // ---------------
	      if(i_in_0 > m_QUANTIZATION_ONE-1) i_in_0 = m_QUANTIZATION_ONE-1;
	      if(i_in_1 > m_QUANTIZATION_ONE-1) i_in_1 = m_QUANTIZATION_ONE-1;
	      if(i_in_0 < -1*m_QUANTIZATION_ONE) i_in_0 = -1*m_QUANTIZATION_ONE;
	      if(i_in_1 < -1*m_QUANTIZATION_ONE) i_in_1 = -1*m_QUANTIZATION_ONE;
	    
	      // Increment pointer ...
	      // ---------------------
	      in_fc32++;
	  }else{
	     i_in_0 = in_sc16->real();
	     i_in_1 = in_sc16->imag();
	     
	     // Increment pointer ...
	     // ---------------------
	     in_sc16++;
	  }
	  
          // 3] 2nd complement conversion ...
          // --------------------------------
          if(i_in_0 < 0){
             data_out_0 = (unsigned short)(i_in_0 + m_TWO_QUANTIZATION_ONE);
          }else{
             data_out_0 = (unsigned short)i_in_0;
          }

          if(i_in_1 < 0){
             data_out_1 = (unsigned short)(i_in_1 + m_TWO_QUANTIZATION_ONE);
          }else{
             data_out_1 = (unsigned short)i_in_1;
          }
 
	  buffer_write[count_in++] = ((data_out_0>>8) & 0xFF);   // DATA_MSB_0;
	  buffer_write[count_in++] = (data_out_0 & 0xFF);        // DATA_LSB_0
	  buffer_write[count_in++] = ((data_out_1>>8) & 0xFF);   // DATA_MSB_1
	  buffer_write[count_in++] = (data_out_1 & 0xFF);        // DATA_LSB_1 
	  
	  /*
	  if (2048!=0 && (2048 % 2048 == 0) 
	  ||  
	  199 == (200-1)
	  */
	  if(count_in!=0 && ((count_in % CONST_ARRAY_SIZE)==0 || (i == input_items_decim-1))){
	      // ------------------------------------------------------------------------------------
	      // The user should assume that the number of input items = noutput_items*decimation ...
	      // ------------------------------------------------------------------------------------
	      int count_in_decim = count_in/m_decimation;	  
	    
	      // Write data buffer ...
	      // ---------------------
	      if(write(fd_1,buffer_write,count_in)!=count_in){
		 cerr << "ERROR: writing msgdma buffer - exiting "<<endl;
		 consume_each(count_iq);
		 return count_iq;
	      }
	      
	      // Read data buffer - response ...
	      // -------------------------------
	      if(read(fd_0,buffer_read,count_in_decim)!=count_in_decim){
		 cerr << "ERROR: reading msgdma buffer - exiting "<<endl;  
		 consume_each(count_iq);
		 return count_iq; 
	      }
	         
	      for(int j=0;j<count_in_decim;j+=4){
		  int i_out_0 = 0, i_out_1 = 0;
		  float f_out_0 = 0.0, f_out_1 = 0.0;
		    
		  unsigned char msb_0 = buffer_read[j];     // DATA_MSB_0 ...
		  unsigned char lsb_0 = buffer_read[j+1];   // DATA_LSB_0 ...
		  unsigned char msb_1 = buffer_read[j+2];   // DATA_MSB_1 ...
		  unsigned char lsb_1 = buffer_read[j+3];   // DATA_LSB_1 ... 
      
		  unsigned short data_out_0 = ((msb_0<<8) | lsb_0);
		  unsigned short data_out_1 = ((msb_1<<8) | lsb_1);
		
		  if(data_out_0 & m_QUANTIZATION_ONE_MASK){               //  --> two's complement - e.g. if 16 [it's 15 bit from ZERO !!!] bit is equal to 1 -> negative value
		     i_out_0 = (int)data_out_0 - m_TWO_QUANTIZATION_ONE;  //  --> -1*(m_TWO_QUANTIZATION_ONE - data_out_0); -- m_QUANTIZATION_ONE must be unsigned integer [othervise OVERFLOW can occur - valid only for 16 bit word !!!]
		  }else{
		     i_out_0 = (int)data_out_0;
		  }

		  if(data_out_1 & m_QUANTIZATION_ONE_MASK){               //  --> two's complement - e.g. if 16 [it's 15 bit from ZERO !!!] bit is equal to 1 -> negative value
		     i_out_1 = (int)data_out_1 - m_TWO_QUANTIZATION_ONE;  //  --> -1*(m_TWO_QUANTIZATION_ONE - data_out_1); -- m_QUANTIZATION_ONE must be unsigned integer [othervise OVERFLOW can occur - valid only for 16 bit word !!!]
		  }else{
		     i_out_1 = (int)data_out_1;
		  }  
		  
		  if(m_itemsizeOutput == sizeof(gr_complex)){
		   // ------------------------------- 
		   // Scale - only for gr_complex ...
		   // -------------------------------
		   if(m_ScaleOutputFactor){
		      f_out_0 = (float(i_out_0) / float(m_ScaleConstant /* m_QUANTIZATION_ONE */));
		      f_out_1 = (float(i_out_1) / float(m_ScaleConstant /* m_QUANTIZATION_ONE */));
		   }else{
		      f_out_0 = float(i_out_0);
		      f_out_1 = float(i_out_1);
		   }
	      
		   // ---------------------------------
		   // gr_complex - I/Q signal parts ...
		   // ---------------------------------
		   out_fc32[count_iq++] = gr_complex(f_out_0, f_out_1);
		  }else{
		   out_sc16[count_iq++] = complex<int16_t>(i_out_0,i_out_1);  
		  }
	        }
	     
	       // Reset count_in variable ...
	       // ---------------------------
	       count_in = 0;
	  }
	}
	
	// Tell runtime system how many input items we consumed on each input stream ...
        // -----------------------------------------------------------------------------
        consume_each (noutput_items);

        // Tell runtime system how many output items we produced.
        return noutput_items;
    }

  } /* namespace AlteraSocSDR */
} /* namespace gr */

