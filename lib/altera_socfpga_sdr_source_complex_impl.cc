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

#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <at86rf215.h>
#include <at86rf215_radio.h>
#include <io_utils/io_utils.h>

#include "altera_socfpga_sdr_source_complex_impl.h"

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
#define CONST_READ_MSGDMA_BUFFER_SIZE 32768     // Test values - 16384 or 32768 ...
#define CONST_GR_STORAGE_DIV 4
#define CONST_READ_STORAGE_BUFFER_SIZE CONST_READ_MSGDMA_BUFFER_SIZE/CONST_GR_STORAGE_DIV
#define CONST_SLEEP_INTERVAL_MICRO_SECONDS 1
#define CONST_SLEEP_INTERVAL_MILI_SECONDS 1

#define CONST_SYNC_MAX_FPGA_COUNT 256
#define CONST_SYNC_MAX_COUNT ((CONST_SYNC_MAX_FPGA_COUNT*4)-4)  //  Default was - 252 ...
#define CONST_DUMMY_READS 5

// ----------------
// RX state machine
// ----------------
#define CONST_SEARCH_SYNC_WORD_0 0
#define CONST_SEARCH_SYNC_WORD_1 1
#define CONST_SEARCH_SYNC_WORD_2 2
#define CONST_SEARCH_SYNC_WORD_3 3

#define CONST_SEARCH_READ_WORD_0 4
#define CONST_SEARCH_READ_WORD_1 5
#define CONST_SEARCH_READ_WORD_2 6
#define CONST_SEARCH_READ_WORD_3 7

// --------------------------------
// AT86RF215 physical layer setting
// --------------------------------
#define GPIO_CS_OFFSET 0
#define GPIO_RESET_OFFSET 1 
#define GPIO_EXT_INT_OFFSET 26 // 2 --> default on GPIOCHIP 2, 26 --> for GPIOCHIP 0

#define CONST_AT86RF215_INIT_OK 1
#define CONST_AT86RF215_INIT_FAILED -1

#define CONST_FPGA_FW_SIDE_INIT_OK 1
#define CONST_FPGA_FW_SIDE_INIT_FAILED -1

// -------------------------
// Define DEBUG messages ...
// -------------------------
// #define CONST_DEBUG_MSG

namespace gr {
  namespace AlteraSocSDR {

    using namespace std;
    
    // AT86RF215 device configuration ...
    // ----------------------------------
    at86rf215_st dev_rx = {
            .cs_pin = GPIO_CS_OFFSET,
            .reset_pin = GPIO_RESET_OFFSET,
            .irq_pin = GPIO_EXT_INT_OFFSET,
                
            .spi_mode = 0,        // SPI mode (0)
            .spi_bits = 0,        // SPI bits mode (0 ... 8 bits) 
            .spi_speed = 1000000, // SPI baud rate
    };
    
    
    // Define static variables ...
    // ---------------------------
    fifo_buffer altera_socfpga_sdr_source_complex_impl::m_fifo;
    int altera_socfpga_sdr_source_complex_impl::fd_0 = -1;
    bool altera_socfpga_sdr_source_complex_impl::m_exit_requested;

    altera_socfpga_sdr_source_complex::sptr
    altera_socfpga_sdr_source_complex::make(const std::string &DeviceName, double Frequency, int SampleRate, int AnalogBw, const std::string DigitalBw, bool AgcEnable, int RxGain, unsigned int WordLength, bool ScaleFactor,int ScaleConstant,unsigned int BufferLength,size_t itemsize,bool swap_iq)
    {
      return gnuradio::get_initial_sptr
        (new altera_socfpga_sdr_source_complex_impl(DeviceName, Frequency, SampleRate, AnalogBw, DigitalBw, AgcEnable, RxGain, WordLength, ScaleFactor, ScaleConstant, BufferLength, itemsize, swap_iq));
    }

    /*
     * The private constructor ...
     * ---------------------------
     */
    altera_socfpga_sdr_source_complex_impl::altera_socfpga_sdr_source_complex_impl(const std::string &DeviceName, double Frequency, int SampleRate, int AnalogBw, const std::string DigitalBw, bool AgcEnable, int RxGain, unsigned int WordLength, bool ScaleFactor, int ScaleConstant, unsigned int BufferLength, size_t itemsize, bool swap_iq)
      : gr::sync_block("altera_socfpga_sdr_source_complex",
              gr::io_signature::make(0,0,0),	    	     // - gr::io_signature::make(<+MIN_IN+>, <+MAX_IN+>, sizeof(<+ITYPE+>)),
              gr::io_signature::make(1,1,itemsize)),         // - default is - sizeof(gr_complex) gr::io_signature::make(<+MIN_OUT+>, <+MAX_OUT+>, sizeof(<+OTYPE+>)))
              m_DeviceName(DeviceName), m_Frequency(Frequency), m_SampleRate(SampleRate), m_AnalogBw(AnalogBw), m_DigitalBw(DigitalBw), m_AgcEnable(AgcEnable), m_RxGain(RxGain), m_WordLength(WordLength), m_ScaleFactor(ScaleFactor),m_ScaleConstant(ScaleConstant), m_BufferLength(BufferLength), m_itemsize(itemsize), m_swap_iq(swap_iq)
    {
      // Only for debug - disable in production ...
      // ------------------------------------------
      // cout << "Item-size is: "<<itemsize<<endl;
      
      if(m_WordLength < 10 || m_WordLength > 16){
         cerr << "WordLength variable was incorrectly set. Defaulting to 16 ..." << endl;
         m_WordLength = 16;
      }

      // Select and set m_MSB_MASK (depends on m_WordLength) ...
      // -------------------------------------------------------
      switch(m_WordLength){
        case 10: m_MSB_MASK = 0x03; break;
        case 11: m_MSB_MASK = 0x07; break;
        case 12: m_MSB_MASK = 0x0F; break;
        case 13: m_MSB_MASK = 0x1F; break;
        case 14: m_MSB_MASK = 0x3F; break;
        case 15: m_MSB_MASK = 0x7F; break;
        case 16: m_MSB_MASK = 0xFF; break;
        default: m_MSB_MASK = 0x3F; break; 
      }

      // Set QUANTIZATION parameters ...
      // -------------------------------
      m_QUANTIZATION_ONE = (1 << m_WordLength-1); 	 		          // unsigned integer -> 2^(WordLength-1) ...
      m_TWO_QUANTIZATION_ONE = 2 * m_QUANTIZATION_ONE;			      // unsigned integer	...
      m_QUANTIZATION_ONE_MASK = (1 <<  m_WordLength-1);		  	      // unsigned integer ...
      
#ifdef CONST_DEBUG_MSG       
      cout<<"m_QUANTIZATION_ONE"<<m_QUANTIZATION_ONE<<endl;
      cout<<"m_TWO_QUANTIZATION_ONE"<<m_TWO_QUANTIZATION_ONE<<endl; 	
#endif 		

      set_output_multiple(CONST_OUTPUT_MULTIPLE); 			          // TEST this setting - e.g. for 8192, 16384, 32768, 65536, ...
        
      m_exit_requested = false;
      m_GPIO_RESET = 0;
      m_radioInitialized = false;
      
      // Init AT86RF215 device first ...
      if(at86rf215_rx_init() != CONST_AT86RF215_INIT_OK){
         cout << "Module init error - at86rf215 side" <<endl;
         m_exit_requested = true; 
      }
      
      // Init msgdma --> open Linux device driver ...
      if(!m_exit_requested){
         if(msgdmaInit() == CONST_ALTERA_MSGDMA_OK){
            m_fifo.fifo_changeSize(m_BufferLength);        		      // Default buffer size is 67108864
            m_exit_requested = false;
        }else{
            cout << "Module init error - Linux driver DMA side" <<endl;
            m_exit_requested = true;
        } 
      }
      
      /* Create read stream THREAD [http://antonym.org/2009/05/threading-with-boost---part-i-creating-threads.html]
         and [http://antonym.org/2010/01/threading-with-boost---part-ii-threading-challenges.html]
      */

      cout << "Threading - using up to CPUs/Cores: "<< boost::thread::hardware_concurrency() <<endl;
      _thread = gr::thread::thread(&altera_socfpga_sdr_source_complex_impl::altera_msgdma_sdr_source_wait,this); 

      
      // FPGA side control initialize ...
      // --------------------------------
      if(!m_exit_requested){ 
         // Add some short to latch data from FPGA FW FIFOS ...
         boost::this_thread::sleep(boost::posix_time::milliseconds(10*CONST_SLEEP_INTERVAL_MILI_SECONDS)); 
         if(socfpga_side_init() == CONST_FPGA_FW_SIDE_INIT_OK){
            m_exit_requested = false; 
         }else{
            cout << "Module init error - FPGA FW side" <<endl;
            m_exit_requested = true; 
         }
      }
    }

    /*
     * Our virtual destructor.
     */
    altera_socfpga_sdr_source_complex_impl::~altera_socfpga_sdr_source_complex_impl()
    {
        
      // Only for debug  ...
      // -------------------
      cout<< "Calling destructor - altera_socfpga_sdr_source_complex_impl ... "<< endl;
        
      // De-init msgdma --> close Linux device driver ...
      // ------------------------------------------------
      msgdmaDeInit();
      
      // AT86RF215 --> deinit AT86RF215 device ...
      // -----------------------------------------
      at86rf215_rx_deinit(true);                    // Show debug messages if requested ...
      
      // FPGA side control deinit ...
      // ----------------------------
      socfpga_side_deinit();
    
      // Close /dev/altera_msgdma_rd0  ...
      // ---------------------------------
      if(fd_0 != -1) close(fd_0); fd_0 = -1;
	
      cout<< "Calling destructor - altera_socfpga_sdr_source_complex_impl - EnDL... "<< endl;
    }
    
    bool altera_socfpga_sdr_source_complex_impl::stop(){
        
        // Only for debug  ...
        // -------------------
        cout<< "Calling STOP - altera_socfpga_sdr_source_complex_impl ... "<< endl;
        
        // De-init msgdma --> close Linux device driver ...
        // ------------------------------------------------
        msgdmaDeInit();
        
        // AT86RF215 --> deinit AT86RF215 device ...
        // -----------------------------------------
        at86rf215_rx_deinit(true);                  // Show debug messages if requested ...
        
        // FPGA side control deinit ...
        // ----------------------------
        socfpga_side_deinit();
        
        // Close /dev/altera_msgdma_rd0  ...
        // ---------------------------------
        if(fd_0 != -1) close(fd_0); fd_0 = -1;
        
        return true;
    }
    
    
    // ftdiInit function [private] ...
    // -------------------------------
    int altera_socfpga_sdr_source_complex_impl::msgdmaInit(){

      int blockSize = CONST_MIN_BLOCK_SIZE;
      // int bufferSize = CONST_KMALLOC_MULTIPLE*CONST_MIN_BLOCK_SIZE;

      // Open /dev/altera_msgdma_rd0 / O_RDONLY ...
      // ------------------------------------------
      if((fd_0 = open(m_DeviceName.c_str(),O_RDONLY)) == -1){ 						// "/dev/altera_msgdma_rd0"
        cerr << "ERROR: could not open \""<<m_DeviceName<<"\"..." <<endl;		    // \"/dev/altera_msgdma_rd0\"
        return CONST_ALTERA_MSGDMA_FAILED;
      }

      // Set Altera MSGDMA internal parameters ...
      // -----------------------------------------
      // ioctl(fd_0,IO_ALTERA_MSGDMA_SET_BUFFER_SIZE,&bufferSize);
      ioctl(fd_0,IO_ALTERA_MSGDMA_SET_MIN_BLOCK_SIZE,&blockSize);

      // Reset Altera MSGDMA - option ...
      // --------------------------------
      ioctl(fd_0,IO_ALTERA_MSGDMA_STOP_RESET);

      // Return success ...
      // ------------------
      return CONST_ALTERA_MSGDMA_OK;
    }
    
    // AT86RF215 device init in RX mode  [private] ...
    // -----------------------------------------------
    int altera_socfpga_sdr_source_complex_impl::at86rf215_rx_init(){
        
        at86rf215_rx_control_st rx_control;
        
        // Initialize AT86RF215 device ...
        if(at86rf215_init(&dev_rx) == -1){
           return CONST_AT86RF215_INIT_FAILED;   
        }
            
        // -- Parse analog bw --
        switch(m_AnalogBw){
            case 2000:
                 rx_control.radio_rx_bw = at86rf215_radio_rx_bw_BW2000KHZ_IF2000KHZ;
                 break;
            case 1600:
                 rx_control.radio_rx_bw = at86rf215_radio_rx_bw_BW1600KHZ_IF2000KHZ;
                 break;
            case 1250:
                 rx_control.radio_rx_bw = at86rf215_radio_rx_bw_BW1250KHZ_IF2000KHZ;
                 break;
            case 1000:
                 rx_control.radio_rx_bw = at86rf215_radio_rx_bw_BW1000KHZ_IF1000KHZ;
                 break;
            case 800:
                 rx_control.radio_rx_bw = at86rf215_radio_rx_bw_BW800KHZ_IF1000KHZ;
                 break;
            case 630:
                 rx_control.radio_rx_bw = at86rf215_radio_rx_bw_BW630KHZ_IF1000KHZ;
                 break;
            case 500:
                 rx_control.radio_rx_bw = at86rf215_radio_rx_bw_BW500KHZ_IF500KHZ;
                 break;
            case 400:
                 rx_control.radio_rx_bw = at86rf215_radio_rx_bw_BW400KHZ_IF500KHZ;
                 break;
            case 320:
                 rx_control.radio_rx_bw = at86rf215_radio_rx_bw_BW320KHZ_IF500KHZ;
                 break;
            case 250:
                 rx_control.radio_rx_bw = at86rf215_radio_rx_bw_BW250KHZ_IF250KHZ;
                 break;
            case 200:
                 rx_control.radio_rx_bw = at86rf215_radio_rx_bw_BW200KHZ_IF250KHZ;
                 break;
            case 160:
                 rx_control.radio_rx_bw = at86rf215_radio_rx_bw_BW160KHZ_IF250KHZ;
                 break;
            default:
                 rx_control.radio_rx_bw = at86rf215_radio_rx_bw_BW2000KHZ_IF2000KHZ;
                 break;
        }
        
        // -- Parse digital bw --
        if(m_DigitalBw.compare("0_25")){                                
           rx_control.digital_bw = at86rf215_radio_rx_f_cut_0_25_half_fs;
        }else if(m_DigitalBw.compare("0_375")){
           rx_control.digital_bw = at86rf215_radio_rx_f_cut_0_375_half_fs;
        }else if(m_DigitalBw.compare("0_5")){
           rx_control.digital_bw = at86rf215_radio_rx_f_cut_0_5_half_fs;
        }else if(m_DigitalBw.compare("0_75")){
           rx_control.digital_bw = at86rf215_radio_rx_f_cut_0_75_half_fs;
        }else if(m_DigitalBw.compare("1_00")){
           rx_control.digital_bw = at86rf215_radio_rx_f_cut_half_fs;
        }else{
           rx_control.digital_bw = at86rf215_radio_rx_f_cut_0_5_half_fs;
        }
        
        // -- Parse sample rate - e.g: at86rf215_radio_rx_sample_rate_4000khz --
        switch(m_SampleRate){
            case 4000000:
                rx_control.fs = at86rf215_radio_rx_sample_rate_4000khz;
                break;
            case 2000000:
                rx_control.fs = at86rf215_radio_rx_sample_rate_2000khz;
                break;
            case 1333333:
                rx_control.fs = at86rf215_radio_rx_sample_rate_1333khz;
                break;
            case 1000000:
                rx_control.fs = at86rf215_radio_rx_sample_rate_1000khz;
                break;
            case 800000:
                rx_control.fs = at86rf215_radio_rx_sample_rate_800khz;
                break;
            case 666667:
                rx_control.fs = at86rf215_radio_rx_sample_rate_666khz;
                break;
            case 500000:
                rx_control.fs = at86rf215_radio_rx_sample_rate_500khz;
                break;
            case 400000:
                rx_control.fs = at86rf215_radio_rx_sample_rate_400khz;
                break;
            default:
                rx_control.fs = at86rf215_radio_rx_sample_rate_4000khz;
                break;
        }
        
        rx_control.agc_enable = m_AgcEnable ? 1 : 0;
        rx_control.agc_gain = m_RxGain;
        rx_control.agc_relative_atten = at86rf215_radio_agc_relative_atten_21_db;
        rx_control.agc_averaging = at86rf215_radio_agc_averaging_8;
        
        // Check frequency range ...
        bool freqOK;
        
        if(m_Frequency >=389.5e6 and m_Frequency <=510e6){
           freqOK = true;
        }else if(m_Frequency >=779e6 and m_Frequency <=1020e6){
           freqOK = true; 
        }else if(m_Frequency >=2400e6 and m_Frequency <=2483.5e6){
           freqOK = true; 
        }else{
           cout << "Frequency for at86rf215 is out of range - defaulting to 2400e6 (2.4Ghz - S Band) or 433e6 (sub-Ghz) ..." <<endl;
           freqOK = false; 
        }
        
        // -- Set radio --
        if(m_Frequency < 2000e6){
           cout << "Applying configuration for sub-GHz ..." << endl;
           at86rf215_setup_iq_radio_receive (&dev_rx, at86rf215_rf_channel_900mhz, freqOK ? (uint64_t)m_Frequency : (uint64_t)433e6, &rx_control, 0, at86rf215_iq_clock_data_skew_1_906ns);   // e.g - 433e6
        }else{
           cout << "Applying configuration for 2.4GHz ..." << endl;
           at86rf215_setup_iq_radio_receive (&dev_rx, at86rf215_rf_channel_2400mhz, freqOK ? (uint64_t)m_Frequency : (uint64_t)2400e6, &rx_control, 0, at86rf215_iq_clock_data_skew_1_906ns); // e.g - 2400e6
        }
        
         m_radioInitialized = true;
        
        // Return success status ...
        return CONST_AT86RF215_INIT_OK; 
        
    }
    
    // AT86RF215 deinit - RX mode  [private] ...
    // ----------------------------------------
    void altera_socfpga_sdr_source_complex_impl::at86rf215_rx_deinit(bool showStatus){
        
        at86rf215_irq_st irq = {0};
        
        // Check initialization status ...
        if(m_radioInitialized){
          // Print status of the device at the end ...
          if(showStatus){
             at86rf215_get_iq_sync_status(&dev_rx);
             at86rf215_get_irqs(&dev_rx, &irq, 1);
          }
        
          // Close and reset device if requested ... 
          at86rf215_close(&dev_rx, 1);  // Reset dev 1, no reset 0 ...
          
          m_radioInitialized = false;
        }
    }
    
    
    // SocFPGA init - initialize FPGA part [private]
    // ---------------------------------------------
    int altera_socfpga_sdr_source_complex_impl::socfpga_side_init(){
        /*
        # RX frontend select ...
        # 0 ... 2.4GHz
        # 1 ... sub-Ghz
        RX_FRONTEND_SELECT=0

        # Register meaning:
        # ------------------------------------
        #  5   | 4 3 2 1     | 0
        #  RST | SAMPLE RATE | FRONTEND SELECT
        # ------------------------------------
        # 5 ... RESET bit
        # 4 - 1 ... SAMPLE RATE [default 1001 - 9 - sample rate 400k]
        # 0 ... Fronted select (2.4G - 0, sub-GHz - 1) [default - 0]
        # -----------------------------------------------------------
        */
        
        int GPIO_VAL = 0;
        int RX_FRONTEND_SELECT = 0; // Default frontend - 0 --> 2.4GHz
        
        // -- Parse sample rate --
        switch(m_SampleRate){
            case 4000000:
                GPIO_VAL=0;
                break;
            case 2000000:
                GPIO_VAL=1;
                break;
            case 1333333:
                GPIO_VAL=2;
                break;
            case 1000000:
                GPIO_VAL=3;
                break;
            case 800000:
                GPIO_VAL=4;
                break;
            case 666667:
                GPIO_VAL=5;
                break;
            case 500000:
                GPIO_VAL=7;
                break;
            case 400000:
                GPIO_VAL=9;
                break;
            default:
                GPIO_VAL=9;
                break;
        }
        
        // -- Set radio --
        if(m_Frequency < 2000e6){
           RX_FRONTEND_SELECT = 1; 
        }
        // GPIO CLEAR - clear RESET ... 
        int GPIO_CLR = ((GPIO_VAL << 1) | (RX_FRONTEND_SELECT & 0x1));
        int valuefd = open("/sys/bus/platform/drivers/altera_gpio/ff200000.gpio1/altera_gpio", O_RDWR);

        if (valuefd < 0){
            cout << "Cannot open GPIO to export it" <<endl;
            return CONST_FPGA_FW_SIDE_INIT_FAILED;
        }
        
        string c_int = to_string(GPIO_CLR); 
        
        write(valuefd, c_int.c_str(), c_int.size());
        close(valuefd);
        
        // GPIO RESET - assert RESET ...
        m_GPIO_RESET = (GPIO_CLR | 0x20);

        // Return success     
        return CONST_FPGA_FW_SIDE_INIT_OK;
    }
    
    // SocFPGA init - deinitialize FPGA part [private]
    // ---------------------------------------------
    void altera_socfpga_sdr_source_complex_impl::socfpga_side_deinit(){
        
        // Assert FPGA FW RESET ...
        if(m_GPIO_RESET!=0){
           int valuefd = open("/sys/bus/platform/drivers/altera_gpio/ff200000.gpio1/altera_gpio", O_RDWR);

           if (valuefd < 0){
               cout << "Cannot open GPIO to export it" <<endl;
               return;
           }
        
           string c_int = to_string(m_GPIO_RESET); 
           write(valuefd, c_int.c_str(), c_int.size());
           close(valuefd);
           
           m_GPIO_RESET = 0;
        }
    }
    
    // msgdmaDeInit function [private] ...
    // -----------------------------------
    void altera_socfpga_sdr_source_complex_impl::msgdmaDeInit(){
        
        gr::thread::scoped_lock lock(fp_mutex);   // shared resources ...
        m_exit_requested = true;
        lock.unlock();

        if(_thread.joinable()) _thread.join();
     
        // m_fifo.~fifo_buffer();
    }

    // altera_msgdma_sdr_wait function [provides new THREAD] - [private /* static */] ...
    // ----------------------------------------------------------------------------------
    void altera_socfpga_sdr_source_complex_impl::altera_msgdma_sdr_source_wait(/*altera_socfpga_sdr_source_complex *obj*/){

      int length = CONST_READ_MSGDMA_BUFFER_SIZE;
      int sync_count = 0, gr_count = 0;
      int rx_state = CONST_SEARCH_SYNC_WORD_0;

      char msb_0 =0x00,lsb_0 = 0x00, msb_1 =0x00,lsb_1 = 0x00; 
      
      unsigned char buffer_read[CONST_READ_MSGDMA_BUFFER_SIZE];       // MSGDMA read buffer - CONST_READ_MSGDMA_BUFFER_SIZE
      gr_storage buffer_read_storage[CONST_READ_STORAGE_BUFFER_SIZE];

      cout << "altera_msgdma_sdr_source_wait SDR processing THREAD was started (reading data from FPGA MGSDMA ...)" << endl;

      if(m_exit_requested){
         cout<< "altera_msgdma_sdr_source_wait processing THREAD ends - clearing resources ... "<< endl;   
         return; 
      }
      
      // Do some dummy reads - clearing DMA FIFO ...
      // -------------------------------------------
      for(int i=0;i<CONST_DUMMY_READS;i++){
          length = read(fd_0,buffer_read,CONST_READ_MSGDMA_BUFFER_SIZE);
        
          if(length!=CONST_READ_MSGDMA_BUFFER_SIZE){
             cerr << "ERROR: read MSGDMA buffer - buffer fill level ..." <<endl;
             return;
          }
      }
      
      // While TRUE loop ...
      // -------------------
      while(true){          
        // Read data from Altera MSGDMA Linux driver ...
        // ---------------------------------------------
        length = read(fd_0,buffer_read,CONST_READ_MSGDMA_BUFFER_SIZE);
        
        if(length!=CONST_READ_MSGDMA_BUFFER_SIZE){
           cerr << "ERROR: read MSGDMA buffer - buffer fill level ..." <<endl;
           break;
        }
          
#ifdef CONST_DEBUG_MSG
        cout<< "Got number of bytes: "<<length<<endl;       
#endif
         gr_count = 0;
        
	for(int n = 0;n < CONST_READ_MSGDMA_BUFFER_SIZE;n++){
            switch(rx_state){
                case CONST_SEARCH_SYNC_WORD_0:
                     //cout <<"State"<< rx_state<<n<<end;
                     if(buffer_read[n] == 0x5A) rx_state = CONST_SEARCH_SYNC_WORD_1; else rx_state = CONST_SEARCH_SYNC_WORD_0;   
                     break;   
                case CONST_SEARCH_SYNC_WORD_1:
                     // cout <<"State"<< rx_state <<endl;
                     if(buffer_read[n] == 0x0F) rx_state = CONST_SEARCH_SYNC_WORD_2; else rx_state = CONST_SEARCH_SYNC_WORD_0;   
                     break;
                case CONST_SEARCH_SYNC_WORD_2:
                     // cout <<"State"<< rx_state <<endl;
                     if(buffer_read[n] == 0xBE) rx_state = CONST_SEARCH_SYNC_WORD_3; else rx_state = CONST_SEARCH_SYNC_WORD_0;   
                     break;
                case CONST_SEARCH_SYNC_WORD_3:
                      //	cout <<"State"<< rx_state <<endl;
                     if(buffer_read[n] == 0x66) rx_state = CONST_SEARCH_READ_WORD_0; else rx_state = CONST_SEARCH_SYNC_WORD_0; 
                     sync_count = CONST_SYNC_MAX_COUNT;
#ifdef CONST_DEBUG_MSG
                    cout<< "Got synchronization word - n: "<<n<<endl;       
#endif
                     break;
                case CONST_SEARCH_READ_WORD_0:
                     // cout <<"State"<< rx_state <<endl;
                     msb_0 = buffer_read[n]; sync_count--; rx_state = CONST_SEARCH_READ_WORD_1;    
                     break;
                case CONST_SEARCH_READ_WORD_1:
                     // cout <<"State"<< rx_state <<endl;	
                     lsb_0 = buffer_read[n]; sync_count--; rx_state = CONST_SEARCH_READ_WORD_2;    
                     break;
                case CONST_SEARCH_READ_WORD_2:
                     // cout <<"State"<< rx_state <<endl;
                     msb_1 = buffer_read[n]; sync_count--; rx_state = CONST_SEARCH_READ_WORD_3;    
                     break;
                case CONST_SEARCH_READ_WORD_3:
                     // cout <<"State"<< rx_state <<endl;	
                     lsb_1 = buffer_read[n]; sync_count--; 
                     // Make gr_storage container  ...
                     buffer_read_storage[gr_count++] = gr_storage(msb_0, lsb_0, msb_1,lsb_1);
                     if(sync_count == 0) rx_state = CONST_SEARCH_SYNC_WORD_0; else rx_state = CONST_SEARCH_READ_WORD_0;
                     break;
                default:
                    break;
            }
        }

        // ------------------------------------------
        // Write to internal circular FIFO buffer ...
        // ------------------------------------------
        if(m_fifo.fifo_write_storage((const gr_storage *)buffer_read_storage,gr_count)!=gr_count){
           cerr << "WARN: User-space FIFO circular buffer overflow (increase size ? ) !!!" << endl;
        }

        // Optional sleep time ...
        // -----------------------
        // boost::this_thread::sleep(boost::posix_time::microseconds(CONST_SLEEP_INTERVAL_MICRO_SECONDS));
        boost::this_thread::sleep(boost::posix_time::milliseconds(5*CONST_SLEEP_INTERVAL_MILI_SECONDS));
        
        gr::thread::scoped_lock lock(fp_mutex);   // shared resources ...
        if(m_exit_requested) break;               // shared global variable - exit requested if m_exit_requested == TRUE --> break ...  
      }

      // End of THREAD event loop ...
      // ----------------------------
      cout<< "altera_msgdma_sdr_source_wait processing THREAD ends - clearing resources ... "<< endl;
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

    int altera_socfpga_sdr_source_complex_impl::work(int noutput_items,
			  gr_vector_const_void_star &input_items,
			  gr_vector_void_star &output_items)
    {
        // const <+ITYPE+> *in = (const <+ITYPE+> *) input_items[0];            // -- source block - no inputs - ...
        // gr_complex *out = (gr_complex *) output_items[0];	                // -- old casting style ...
        
        int n = 0, count = 0;
        int rem = 0;
        int noutput_items_real = 0;
      
        gr_complex *out_fc32 = 0;
        complex<int16_t> *out_sc16 = 0; 
      
        if(m_itemsize == sizeof(gr_complex)){
           out_fc32 = reinterpret_cast<gr_complex *>(output_items[0]);      	// -- <+OTYPE+> *out = (<+OTYPE+> *) output_items[0];
        }else{
           out_sc16 = reinterpret_cast<complex<int16_t> *>(output_items[0]);    // -- <+OTYPE+> *out = (<+OTYPE+> *) output_items[0];
        }
	
        gr_storage *buff_storage = new gr_storage[noutput_items];	 	        // gr_storage -> 4 bytes -> 2 shorts -> gr_complex(float conversion) -> 4 bytes are needed ...

        // ------------------------------
        // -- Do <+signal processing+> --
        // ------------------------------
        // gr::thread::scoped_lock lock(fp_mutex);          			        // shared resources ...

        // Read from internal circular FIFO buffer ...
        // -------------------------------------------
        noutput_items_real = m_fifo.fifo_read_storage(buff_storage,noutput_items); // We combine two CHARS to single SHORT [2*chars - > 1*short] and 2*short -> gr_complex(float conversion) -> 4 bytes are needed ...

        for(n=0;n<noutput_items_real;n++){
            // Combine bytes to single uint16_t (unsigned short) / 16 bit output (MSB first) ...
            // ---------------------------------------------------------------------------------
            unsigned char DATA_MSB_0 = (buff_storage[n].getMsbByte0() /* & m_MSB_MASK */);     // MSB part 0 ...
            unsigned char DATA_LSB_0 = buff_storage[n].getLsbByte0();     	   	               // LSB part 0 ...

            unsigned char DATA_MSB_1 = (buff_storage[n].getMsbByte1() /* & m_MSB_MASK */);     // MSB part 1 ...
            unsigned char DATA_LSB_1 = buff_storage[n].getLsbByte1();   		               // LSB part 1 ...

#ifdef CONST_DEBUG_MSG  
            cout<<hex<<int(DATA_MSB_0)<<" "<<int(DATA_LSB_0)<<endl;	
            cout<<hex<<int(DATA_MSB_1)<<" "<<int(DATA_LSB_1)<<endl;
#endif
            unsigned short data_out_0 = ((DATA_MSB_0<<8) | DATA_LSB_0);
            unsigned short data_out_1 = ((DATA_MSB_1<<8) | DATA_LSB_1);

            short i_out_0 = 0, i_out_1 = 0;
            float f_out_0 = 0.0, f_out_1 = 0.0;

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
	    
            if(m_itemsize == sizeof(gr_complex)){
                // Scale - only for gr_complex ...
                // -------------------------------
                if(m_ScaleFactor){
                    f_out_0 = (float(i_out_0) / float(m_ScaleConstant /* m_QUANTIZATION_ONE */));
                    f_out_1 = (float(i_out_1) / float(m_ScaleConstant /* m_QUANTIZATION_ONE */));
                }else{
                    f_out_0 = float(i_out_0);
                    f_out_1 = float(i_out_1);
                }

                // ------------------------------------
                // -- gr_complex - I/Q signal parts ...
                // ------------------------------------
                if(m_swap_iq)
                   out_fc32[count++] = gr_complex(f_out_1,f_out_0);
                else    
                   out_fc32[count++] = gr_complex(f_out_0,f_out_1);
            }else{
                out_sc16[count++] = complex<int16_t>(i_out_0,i_out_1);  
            }
          }

          // Delete temporary buffer ...
          // ---------------------------
          if(buff_storage!=0) delete[] buff_storage; buff_storage=0;

          // Tell runtime system how many output items we produced.
          // ------------------------------------------------------
          return noutput_items_real;
    }

  } /* namespace AlteraSocSDR */
} /* namespace gr */
