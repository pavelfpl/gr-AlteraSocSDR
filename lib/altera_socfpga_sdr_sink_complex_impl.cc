/* -*- c++ -*- */
/*
 * Copyright 2018 Pavel Fiala.
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
#include "altera_socfpga_sdr_sink_complex_impl.h"

#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <cstring>
#include <limits>
#include <thread>

// ---------------------------------------
// IOCLT include - altera_msgdma_ioctl ...
// ---------------------------------------
#include "altera_msgdma_ioctl.h"

// ---------------------------------------------------
// Altera MSGDMA internal configuration parameters ...
// ---------------------------------------------------
#define CONST_KMALLOC_MULTIPLE 128              // Default value - 128 ...
#define CONST_MIN_BLOCK_SIZE 1024               // Default value - 1024 ..

#define CONST_ALTERA_MSGDMA_OK 1
#define CONST_ALTERA_MSGDMA_FAILED -1

#define CONST_OUTPUT_MULTIPLE 256
#define CONST_WRITE_MSGDMA_BUFFER_SIZE 16384    // Test values - 16384 or 32768 ...
#define CONST_GR_STORAGE_DIV 4
#define CONST_WRITE_STORAGE_BUFFER_SIZE CONST_WRITE_MSGDMA_BUFFER_SIZE/CONST_GR_STORAGE_DIV
#define CONST_SLEEP_INTERVAL_MICRO_SECONDS 1
#define CONST_SLEEP_INTERVAL_MILI_SECONDS 1

// ----------------
// TX state machine
// ----------------
#define CONST_WRITE_WORD_0 0
#define CONST_WRITE_WORD_1 1
#define CONST_WRITE_WORD_2 2
#define CONST_WRITE_WORD_3 3

// #define CONST_DEBUG_MSG
// #define CONST_DEBUG_DATA_FLOW

namespace gr {
  namespace AlteraSocSDR {

    using namespace std;

    // Define static variables ...
    // ---------------------------
    fifo_buffer altera_socfpga_sdr_sink_complex_impl::m_fifo;
    int altera_socfpga_sdr_sink_complex_impl::fd_1 = -1;
    bool altera_socfpga_sdr_sink_complex_impl::m_exit_requested;

    altera_socfpga_sdr_sink_complex::sptr
    altera_socfpga_sdr_sink_complex::make(const std::string &DeviceName,int Frequency, double SampleRate, int GainRF, int GainIF, unsigned int WordLength, bool ScaleFactor, int ScaleConstant,unsigned int BufferLength,size_t itemsize,bool swap_iq, float gainCorrection)
    {
      return gnuradio::get_initial_sptr
        (new altera_socfpga_sdr_sink_complex_impl(DeviceName,Frequency, SampleRate, GainRF, GainIF, WordLength, ScaleFactor,ScaleConstant, BufferLength,itemsize,swap_iq, gainCorrection));
    } 

    /*
     * The private constructor
     */
    altera_socfpga_sdr_sink_complex_impl::altera_socfpga_sdr_sink_complex_impl(const std::string &DeviceName,int Frequency, double SampleRate, int GainRF, int GainIF, unsigned int WordLength, bool ScaleFactor, int ScaleConstant,unsigned int BufferLength,size_t itemsize, bool swap_iq, float gainCorrection)
      : gr::sync_block("altera_socfpga_sdr_sink_complex",
        gr::io_signature::make(1,1,itemsize), 		// - sizeof(gr_complex) - gr::io_signature::make(<+MIN_IN+>, <+MAX_IN+>, sizeof(<+ITYPE+>)),
        gr::io_signature::make(0,0,0)),                 // - gr::io_signature::make(<+MIN_OUT+>, <+MAX_OUT+>, sizeof(<+OTYPE+>))),
        m_DeviceName(DeviceName), m_Frequency(Frequency), m_SampleRate(SampleRate), m_GainRF(GainRF), m_GainIF(GainIF), m_WordLength(WordLength), m_ScaleFactor(ScaleFactor), m_ScaleConstant(ScaleConstant), m_BufferLength(BufferLength), m_itemsize(itemsize),m_swap_iq(swap_iq),m_gainCorrection(gainCorrection)
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
      m_QUANTIZATION_ONE = (1 << m_WordLength-1); 	 		          // unsigned integer -> 2^(WordLength-1) ...
      m_TWO_QUANTIZATION_ONE = 2 * m_QUANTIZATION_ONE;			      // unsigned integer	...
      m_QUANTIZATION_ONE_MASK = (1 <<  m_WordLength-1);		  	      // unsigned integer ...

      set_output_multiple(CONST_OUTPUT_MULTIPLE); 			          // TEST this setting - e.g. for 8192, 16384, 32768, 65536, ...

      // Init msgdma --> open Linux device driver ...
      // --------------------------------------------
      if(msgdmaInit() == CONST_ALTERA_MSGDMA_OK){
         m_fifo.fifo_changeSize(m_BufferLength);        		          // Default buffer size is 67108864
         m_exit_requested = false;
      }else{
         m_exit_requested = true;
      }
      
      // Set sample rate ...
      // -------------------
      set_sample_rate(SampleRate);

      /* Create write stream THREAD [http://antonym.org/2009/05/threading-with-boost---part-i-creating-threads.html]
         and [http://antonym.org/2010/01/threading-with-boost---part-ii-threading-challenges.html]
      */

      cout << "Threading - using up to CPUs/Cores: "<< boost::thread::hardware_concurrency() <<endl;
      _thread = gr::thread::thread(&altera_socfpga_sdr_sink_complex_impl::altera_msgdma_sdr_sink_wait,this);
    }

    /*
     * Our virtual destructor.
     */
    altera_socfpga_sdr_sink_complex_impl::~altera_socfpga_sdr_sink_complex_impl()
    {
      
       // Only for debug  ...
      // -------------------
      cout<< "Calling destructor - altera_socfpga_sdr_sink_complex_impl ... "<< endl;
        
        
      // De-init msgdma --> close Linux device driver ...
      // ------------------------------------------------
      msgdmaDeInit();
      
      // Close /dev/altera_msgdma_wr0  ...
      // ---------------------------------
      if(fd_1 != -1) close(fd_1);
    }
    
    bool altera_socfpga_sdr_sink_complex_impl::stop(){
        
        // Only for debug  ...
        // -------------------
        cout<< "Calling STOP - altera_socfpga_sdr_sink_complex_impl ... "<< endl;
        
        // De-init msgdma --> close Linux device driver ...
        // ------------------------------------------------
        msgdmaDeInit();
      
        // Close /dev/altera_msgdma_rd0  ...
        // ---------------------------------
        if(fd_1 != -1) close(fd_1);
        
        return true;
    }
    
    
    bool altera_socfpga_sdr_sink_complex_impl::start(){
        d_start = std::chrono::steady_clock::now();
        d_total_samples = 0;
        return block::start();
    }
    
    void altera_socfpga_sdr_sink_complex_impl::set_sample_rate(double rate){
        // changing the sample rate performs a reset of state params
        d_start = std::chrono::steady_clock::now();
        d_total_samples = 0;
        d_sample_rate = rate;
        d_sample_period = std::chrono::duration<double>(1 / rate);
    }
    
    double altera_socfpga_sdr_sink_complex_impl::sample_rate() const { return d_sample_rate; }
    
    // msgdmaInit function [private] ...
    // ---------------------------------
    int altera_socfpga_sdr_sink_complex_impl::msgdmaInit(){

      int blockSize = CONST_MIN_BLOCK_SIZE;
      // int bufferSize = CONST_KMALLOC_MULTIPLE*CONST_MIN_BLOCK_SIZE;

      // Open /dev/altera_msgdma_wr0 / O_WRONLY ...
      // ------------------------------------------
      if((fd_1 = open(m_DeviceName.c_str() ,O_WRONLY)) == -1){				// "/dev/altera_msgdma_wr0"
         cerr << "ERROR: could not open \""<<m_DeviceName<<"\"..." <<endl;
         return CONST_ALTERA_MSGDMA_FAILED;
      }

      // Set Altera MSGDMA internal parameters ...
      // -----------------------------------------
      // ioctl(fd_0,IO_ALTERA_MSGDMA_SET_BUFFER_SIZE,&bufferSize);
      ioctl(fd_1,IO_ALTERA_MSGDMA_SET_MIN_BLOCK_SIZE,&blockSize);

      // Reset Altera MSGDMA - option ...
      // --------------------------------
      ioctl(fd_1,IO_ALTERA_MSGDMA_STOP_RESET);

      // Return success ...
      // ------------------
      return CONST_ALTERA_MSGDMA_OK;
    }

    // msgdmaDeInit function [private] ...
    // -----------------------------------
    void altera_socfpga_sdr_sink_complex_impl::msgdmaDeInit(){

        gr::thread::scoped_lock lock(fp_mutex);   // shared resources ...
        m_exit_requested = true;
        lock.unlock();

        _thread.join();

        // m_fifo.~fifo_buffer();
    }

    // altera_msgdma_sdr_sink_wait function [provides new THREAD] - [public static] ...
    // --------------------------------------------------------------------------------
    void altera_socfpga_sdr_sink_complex_impl::altera_msgdma_sdr_sink_wait(/*altera_socfpga_sdr_sink_complex *obj*/){

      int niutput_items_real = 0;
      int length = 0;
      int n = 0, count = 0;

      int tx_state = CONST_WRITE_WORD_0;
      
      unsigned char buffer_write[CONST_WRITE_MSGDMA_BUFFER_SIZE];       // MSGDMA write buffer - CONST_WRITE_MSGDMA_BUFFER_SIZE
      gr_storage buffer_write_storage[CONST_WRITE_STORAGE_BUFFER_SIZE];

      // std::chrono::time_point<std::chrono::steady_clock> m_start;
      // uint64_t m_total_samples=0;	

      cout << "altera_msgdma_sdr_sink_wait (sink) SDR processing THREAD was started (writing data to FPGA MGSDMA ...)" << endl;

      if(m_exit_requested){
         cout<< "altera_msgdma_sdr_source_wait processing THREAD ends - clearing resources ... "<< endl;   
         return; 
      }

      // sleep(1);	

      // m_start = std::chrono::steady_clock::now();	
      
      // While TRUE loop ...
      // -------------------
      while(true){
        // Read from internal circular FIFO buffer ...
        // -------------------------------------------
        niutput_items_real = m_fifo.fifo_read_storage(buffer_write_storage, CONST_WRITE_STORAGE_BUFFER_SIZE);
        length = CONST_GR_STORAGE_DIV*niutput_items_real;

        if(niutput_items_real > 0){
          // ---------------------------------------------------------
          // Extract gr_storage container - optimized version 1.0  ...
          // ---------------------------------------------------------
          count = 0; // Reset counter ...
          
          for(n=0;n<length;n++){
              switch(tx_state){
                case CONST_WRITE_WORD_0:
                     buffer_write[n] = buffer_write_storage[count].getMsbByte0(); tx_state = CONST_WRITE_WORD_1; break;   
                case CONST_WRITE_WORD_1:
                     buffer_write[n] = buffer_write_storage[count].getLsbByte0(); tx_state = CONST_WRITE_WORD_2; break;
                case CONST_WRITE_WORD_2:
                     buffer_write[n] = buffer_write_storage[count].getMsbByte1(); tx_state = CONST_WRITE_WORD_3; break;
                case CONST_WRITE_WORD_3:  
                     buffer_write[n] = buffer_write_storage[count++].getLsbByte1(); tx_state = CONST_WRITE_WORD_0; break;
                default:
                    break;
              }
          }
            
          // ------------------------------------------------------
          // Extract gr_storage container - first version 0.1  ...
          // -----------------------------------------------------
          
          /*  
          count = 0; // Reset counter ...
          for(n=0;n<niutput_items_real;n++){
              buffer_write[count++] = buffer_write_storage[n].getMsbByte0();
              buffer_write[count++] = buffer_write_storage[n].getLsbByte0();
              buffer_write[count++] = buffer_write_storage[n].getMsbByte1();
              buffer_write[count++] = buffer_write_storage[n].getLsbByte1();
          }
          */
          
          
          // --------------------------------------------------
          // Write data to Altera MSGDMA using Linux driver ...
          // --------------------------------------------------
          if(write(fd_1,buffer_write,length)!=length){
            cerr << "ERROR: write MSGDMA buffer - buffer fill level ..." <<endl;
            break;
          }
          
#ifdef CONST_DEBUG_MSG
        cout<< "Number of bytes written: "<<length<<endl;       
#endif

        }

	if(niutput_items_real == 0){
           // Optional sleep time ...
           // -----------------------
           boost::this_thread::sleep(boost::posix_time::microseconds(CONST_SLEEP_INTERVAL_MICRO_SECONDS));
           // boost::this_thread::sleep(boost::posix_time::milliseconds(CONST_SLEEP_INTERVAL_MILI_SECONDS));
        }
        gr::thread::scoped_lock lock(fp_mutex);   // shared resources ...
        if(m_exit_requested) break;               // shared global variable - exit requested if m_exit_requested == TRUE --> break ...  
        
	// Calculate sample flow ...
        // -------------------------
       
	/*
        m_total_samples += (niutput_items_real);

        auto now = std::chrono::steady_clock::now();
        auto expected_time = m_start + d_sample_period * m_total_samples;
    
        if (expected_time > now) {
            auto limit_duration = std::chrono::duration<double>(std::numeric_limits<long>::max());
            if (expected_time - now > limit_duration) {
               GR_LOG_ALERT(d_logger,
                         "WARNING: Throttle sleep time overflow! You "
                         "are probably using a very low sample rate.");
            }
        
            std::this_thread::sleep_until(expected_time);
        }
      */
      }
      	

      // End of THREAD event loop ...
      // ----------------------------
      cout<< "altera_msgdma_sdr_sink_wait processing THREAD ends - clearing resources ... "<< endl;
    }

    // GRC work function
    // -----------------
    /*
    http://stackoverflow.com/questions/32305813/gnuradio-how-to-change-the-noutput-items-dynamically-when-writing-oot-block
    -----------------------------------------------------------------------------------------------------------------------
    For example, it might call work with 4000 input items and 4000 output items space (you have a sync block, therefore number of input==number of output).
    Your work processes the first 1000 of that, and therefore return 1000. So there's 3000 items left.
    Now, the upstream block does something, so there's 100 new items. Because the 3000 from before are still there, your block's work will get called with 3100 items.
    Your work processes any number of items, and returns that number. GNU Radio makes sure that the "remaining" items stay available and will call your work again
    if there is enough in- our output.
    */

    int
    altera_socfpga_sdr_sink_complex_impl::work(int noutput_items,
			  gr_vector_const_void_star &input_items,
			  gr_vector_void_star &output_items)
    {
        // const gr_complex *in = (const gr_complex *) input_items[0];                             // -- Old casting style ...
        // const gr_complex* in_data = reinterpret_cast<const gr_complex *>(input_items[0]);       // -- const <+ITYPE+> *in = (const <+ITYPE+> *) input_items[0];
      	
        const gr_complex *in_data_fc32 = 0;
        const complex<int16_t> *in_data_sc16 = 0; 
      
        if(m_itemsize == sizeof(gr_complex)){
           in_data_fc32 = reinterpret_cast<const gr_complex *>(input_items[0]);        	     	  // -- <+OTYPE+> *out = (<+OTYPE+> *) output_items[0];
        }else{
           in_data_sc16 = reinterpret_cast<const complex<int16_t> *>(input_items[0]);        	  // -- <+OTYPE+> *out = (<+OTYPE+> *) output_items[0];
        }
      
        // <+OTYPE+> *out = (<+OTYPE+> *) output_items[0];
        int items_consumed = 0;

        // Create buff_storage buffer [gr_storage] ...
        // -------------------------------------------
        gr_storage *buff_storage = new gr_storage[noutput_items];

        // Do <+signal processing+>
        // ------------------------
        // gr::thread::scoped_lock lock(fp_mutex); // shared resources ...

        for(int n = 0; n < noutput_items; n++){

          int i_in_0 = 0, i_in_1 = 0;
          unsigned char msb_0 = 0x00, lsb_0 = 0x00, msb_1 = 0x00, lsb_1 = 0x00;
          unsigned short data_out_0 = 0x0000, data_out_1 = 0x0000;
	  
          if(m_itemsize == sizeof(gr_complex)){
            // 1] Scale ...
            // ------------
            if(m_ScaleFactor){
               i_in_0 = (in_data_fc32->real() * float(m_ScaleConstant /* m_QUANTIZATION_ONE */) * m_gainCorrection);
               i_in_1 = (in_data_fc32->imag() * float(m_ScaleConstant /* m_QUANTIZATION_ONE */) * m_gainCorrection);
               
               // 2.1] Saturate ...
               // --------------- 
               if(i_in_0 > m_ScaleConstant-1) i_in_0 = m_ScaleConstant-1;
               if(i_in_1 > m_ScaleConstant-1) i_in_1 = m_ScaleConstant-1;
               if(i_in_0 < -1*m_ScaleConstant) i_in_0 = -1*m_ScaleConstant;
               if(i_in_1 < -1*m_ScaleConstant) i_in_1 = -1*m_ScaleConstant; 
            }else{
               i_in_0 = in_data_fc32->real();
               i_in_1 = in_data_fc32->imag();
               
               // 2.2] Saturate ...
               // --------------- 
               if(i_in_0 > m_QUANTIZATION_ONE-1) i_in_0 = m_QUANTIZATION_ONE-1;
               if(i_in_1 > m_QUANTIZATION_ONE-1) i_in_1 = m_QUANTIZATION_ONE-1;
               if(i_in_0 < -1*m_QUANTIZATION_ONE) i_in_0 = -1*m_QUANTIZATION_ONE;
               if(i_in_1 < -1*m_QUANTIZATION_ONE) i_in_1 = -1*m_QUANTIZATION_ONE; 
            }
	     
            // Increment pointer ...
            // ---------------------
            in_data_fc32++;
        }else{
            i_in_0 = in_data_sc16->real();
            i_in_1 = in_data_sc16->imag();
	     
            // Increment pointer ...
            // ---------------------
            in_data_sc16++;
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

        msb_0 = ((data_out_0>>8) & 0xFF);   // DATA_MSB_0
        lsb_0 = (data_out_0 & 0xFF);        // DATA_LSB_0
        msb_1 = ((data_out_1>>8) & 0xFF);   // DATA_MSB_1
        lsb_1 = (data_out_1 & 0xFF);        // DATA_LSB_1
        
        if(m_swap_iq)
            buff_storage[n] = gr_storage(msb_1,lsb_1,msb_0,lsb_0);
        else    
            buff_storage[n] = gr_storage(msb_0,lsb_0,msb_1,lsb_1);

    }
    
    // Write data to FIFO buffer ...
    // -----------------------------
    items_consumed = m_fifo.fifo_write_storage(buff_storage, noutput_items);

    // Calculate sample flow ...
    // -------------------------
    d_total_samples += (noutput_items-(noutput_items/100)*0);

    auto now = std::chrono::steady_clock::now();
    auto expected_time = d_start + d_sample_period * d_total_samples;

#ifdef CONST_DEBUG_DATA_FLOW
     cout<< "Get samples: "<<noutput_items<< endl;
#endif    
    
    if (expected_time > now) {
        auto limit_duration = std::chrono::duration<double>(std::numeric_limits<long>::max());
        if (expected_time - now > limit_duration) {
            GR_LOG_ALERT(d_logger,
                         "WARNING: Throttle sleep time overflow! You "
                         "are probably using a very low sample rate.");
        }
        
        std::this_thread::sleep_until(expected_time);
    }
    
    // Tell runtime system how many input items we consumed on each input stream.
    // --------------------------------------------------------------------------
    consume_each(items_consumed);

    // Delete temporary buffer ...
    // ---------------------------
    if(buff_storage!=0) delete[] buff_storage; buff_storage=0;

    // Tell runtime system how many output items we produced ...
	// ---------------------------------------------------------
    return 0;  // noutput_items;
    }

  } /* namespace AlteraSocSDR */
} /* namespace gr */
