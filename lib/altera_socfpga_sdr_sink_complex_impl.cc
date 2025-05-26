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

#include <at86rf215.h>
#include <at86rf215_radio.h>
#include <io_utils/io_utils.h>

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


// #define CONST_DEBUG_MSG
// #define CONST_DEBUG_DATA_FLOW

namespace gr {
  namespace AlteraSocSDR {

    using namespace std;
    
    // AT86RF215 device configuration ...
    // ----------------------------------
    at86rf215_st dev_tx = {
            .cs_pin = GPIO_CS_OFFSET,
            .reset_pin = GPIO_RESET_OFFSET,
            .irq_pin = GPIO_EXT_INT_OFFSET,
                
            .spi_mode = 0,        // SPI mode (0)
            .spi_bits = 0,        // SPI bits mode (0 ... 8 bits) 
            .spi_speed = 1000000, // SPI baud rate
    };
    
    // Define static variables ...
    // ---------------------------
    fifo_buffer altera_socfpga_sdr_sink_complex_impl::m_fifo;
    int altera_socfpga_sdr_sink_complex_impl::fd_1 = -1;
    bool altera_socfpga_sdr_sink_complex_impl::m_exit_requested;

    altera_socfpga_sdr_sink_complex::sptr
    altera_socfpga_sdr_sink_complex::make(const std::string &DeviceName, unsigned long Frequency, int SampleRate, int AnalogBw, const std::string DigitalBw, int TxGain, unsigned int WordLength, bool ScaleFactor, int ScaleConstant,unsigned int BufferLength,size_t itemsize,bool swap_iq, float gainCorrection)
    {
      return gnuradio::get_initial_sptr
        (new altera_socfpga_sdr_sink_complex_impl(DeviceName,Frequency, SampleRate, AnalogBw, DigitalBw, TxGain, WordLength, ScaleFactor,ScaleConstant, BufferLength,itemsize,swap_iq, gainCorrection));
    }  

    /*
     * The private constructor
     */
    altera_socfpga_sdr_sink_complex_impl::altera_socfpga_sdr_sink_complex_impl(const std::string &DeviceName, unsigned long  Frequency, int SampleRate, int AnalogBw, const std::string DigitalBw, int TxGain, unsigned int WordLength, bool ScaleFactor, int ScaleConstant, unsigned int BufferLength, size_t itemsize, bool swap_iq, float gainCorrection)
      : gr::sync_block("altera_socfpga_sdr_sink_complex",
        gr::io_signature::make(1,1,itemsize), 		// - sizeof(gr_complex) - gr::io_signature::make(<+MIN_IN+>, <+MAX_IN+>, sizeof(<+ITYPE+>)),
        gr::io_signature::make(0,0,0)),                 // - gr::io_signature::make(<+MIN_OUT+>, <+MAX_OUT+>, sizeof(<+OTYPE+>))),
        m_DeviceName(DeviceName), m_Frequency(Frequency), m_SampleRate(SampleRate), m_AnalogBw(AnalogBw), m_DigitalBw(DigitalBw), m_TxGain(TxGain), m_WordLength(WordLength), m_ScaleFactor(ScaleFactor), m_ScaleConstant(ScaleConstant), m_BufferLength(BufferLength), m_itemsize(itemsize),m_swap_iq(swap_iq),m_gainCorrection(gainCorrection)
    {

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

      set_output_multiple(CONST_OUTPUT_MULTIPLE); 			          // TEST this setting - e.g. for 8192, 16384, 32768, 65536, ...

      // Init AT86RF215 device first ...
      // -------------------------------
      if(at86rf215_tx_init() != CONST_AT86RF215_INIT_OK){
         cout << "Module init error - at86rf215 side" <<endl; 
         m_exit_requested = true; 
      }
      
      // FPGA side control initialize ...
      // --------------------------------
      if(!m_exit_requested){ 
          if(socfpga_side_init() == CONST_FPGA_FW_SIDE_INIT_OK){
             m_exit_requested = false; 
          }else{
             cout << "Module init error - FPGA FW side" <<endl; 
             m_exit_requested = true;
          }
      }
      
      // Init msgdma --> open Linux device driver ...
      // --------------------------------------------
      if(!m_exit_requested){
         if(msgdmaInit() == CONST_ALTERA_MSGDMA_OK){
            m_fifo.fifo_changeSize(m_BufferLength);        		      // Default buffer size is 67108864
            m_exit_requested = false;
        }else{
            cout << "Module init error - Linux driver DMA side" <<endl;
            m_exit_requested = true;
        } 
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
      
      // FPGA side control deinit ...
      // ----------------------------
      socfpga_side_deinit();
      
      // AT86RF215 --> deinit AT86RF215 device ...
      // -----------------------------------------
      at86rf215_tx_deinit(true);                  // Show debug messages if requested ...
      
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
        
        // FPGA side control deinit ...
        // ----------------------------
        socfpga_side_deinit();
      
        // AT86RF215 --> deinit AT86RF215 device ...
        // -----------------------------------------
        at86rf215_tx_deinit(true);                  // Show debug messages if requested ...
      
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
    
    // AT86RF215 device init in TX mode  [private] ...
    // -----------------------------------------------
    int altera_socfpga_sdr_sink_complex_impl::at86rf215_tx_init(){
        
        // https://ww1.microchip.com/downloads/aemDocuments/documents/OTH/ProductDocuments/DataSheets/Atmel-42415-WIRELESS-AT86RF215_Datasheet.pdf
        
        at86rf215_tx_control_st tx_control;
        
        // Initialize AT86RF215 device ...
        if(at86rf215_init(&dev_tx) == -1){
           return CONST_AT86RF215_INIT_FAILED;   
        }
            
        tx_control.tx_control_with_iq_if = 0;
        tx_control.pa_ramping_time = at86rf215_radio_tx_pa_ramp_4usec;
        tx_control.current_reduction = at86rf215_radio_pa_current_reduction_0ma;
        tx_control.tx_power = m_TxGain; // Max power: 0x1F, default: 0x10

        // -- Parse analog bw --
        switch(m_AnalogBw){
            case 1000:
                 tx_control.analog_bw = at86rf215_radio_tx_cut_off_1000khz;
                 break;
            case 800:
                 tx_control.analog_bw = at86rf215_radio_tx_cut_off_800khz;
                 break;
            case 625:
                 tx_control.analog_bw = at86rf215_radio_tx_cut_off_625khz;
                 break;
            case 500:
                 tx_control.analog_bw = at86rf215_radio_tx_cut_off_500khz;
                 break;
            case 400:
                 tx_control.analog_bw = at86rf215_radio_tx_cut_off_400khz;
                 break;
            case 315:
                 tx_control.analog_bw = at86rf215_radio_tx_cut_off_315khz;
                 break;
            case 250:
                 tx_control.analog_bw = at86rf215_radio_tx_cut_off_250khz;
                 break;
            case 200:
                 tx_control.analog_bw = at86rf215_radio_tx_cut_off_200khz;
                 break;
            case 160:
                 tx_control.analog_bw = at86rf215_radio_tx_cut_off_160khz;
                 break;
            case 125:
                 tx_control.analog_bw = at86rf215_radio_tx_cut_off_125khz;
                 break;
            case 100:
                 tx_control.analog_bw = at86rf215_radio_tx_cut_off_100khz;
                 break;
            case 80:
                 tx_control.analog_bw = at86rf215_radio_tx_cut_off_80khz;
                 break;
            default:
                 tx_control.analog_bw = at86rf215_radio_tx_cut_off_1000khz;
                 break;
        }

        // -- Parse digital bw --
        if(m_DigitalBw.compare("0_25")){
           tx_control.digital_bw = at86rf215_radio_rx_f_cut_0_25_half_fs;
        }else if(m_DigitalBw.compare("0_375")){
           tx_control.digital_bw = at86rf215_radio_rx_f_cut_0_375_half_fs;
        }else if(m_DigitalBw.compare("0_5")){
           tx_control.digital_bw = at86rf215_radio_rx_f_cut_0_5_half_fs;
        }else if(m_DigitalBw.compare("0_75")){
           tx_control.digital_bw = at86rf215_radio_rx_f_cut_0_75_half_fs;
        }else if(m_DigitalBw.compare("1_00")){
           tx_control.digital_bw = at86rf215_radio_rx_f_cut_half_fs;
        }else{
           tx_control.digital_bw = at86rf215_radio_rx_f_cut_0_5_half_fs;
        }

        // -- Parse sample rate - e.g: at86rf215_radio_rx_sample_rate_4000khz --
        switch(m_SampleRate){
            case 4000000:
                tx_control.fs = at86rf215_radio_rx_sample_rate_4000khz;
                break;
            case 2000000:
                tx_control.fs = at86rf215_radio_rx_sample_rate_2000khz;
                break;
            case 1333333:
                tx_control.fs = at86rf215_radio_rx_sample_rate_1333khz;
                break;
            case 1000000:
                tx_control.fs = at86rf215_radio_rx_sample_rate_1000khz;
                break;
            case 800000:
                tx_control.fs = at86rf215_radio_rx_sample_rate_800khz;
                break;
            case 666667:
                tx_control.fs = at86rf215_radio_rx_sample_rate_666khz;
                break;
            case 500000:
                tx_control.fs = at86rf215_radio_rx_sample_rate_500khz;
                break;
            case 400000:
                tx_control.fs = at86rf215_radio_rx_sample_rate_400khz;
                break;
            default:
                tx_control.fs = at86rf215_radio_rx_sample_rate_4000khz;
                break;
        }
        
        // Check frequency range 
        bool freqOK;
        
        if(m_Frequency >=389.5e6 and m_Frequency <=510e6){
           freqOK = true;
        }else if(m_Frequency >=779e6 and m_Frequency <=1020e6){
           freqOK = true; 
        }else if(m_Frequency >=2400e6 and m_Frequency <=2483.5e6){
           freqOK = true; 
        }else{
           cout << "Frequency for at86rf215 is out of range - defaulting to 2400e6 (2.4GHz - S Band ) or 433e6 (or sub-GHz)" <<endl;
           freqOK = false; 
        }
        
        // -- Set radio --
        if(m_Frequency < 2000e6){
           cout << "Applying configuration for sub-GHz ..." << endl;
           at86rf215_setup_iq_radio_transmit (&dev_tx, at86rf215_rf_channel_900mhz, freqOK ? m_Frequency : 433e6, &tx_control, 0, at86rf215_iq_clock_data_skew_1_906ns);  // e.g -  433e6
        }else{
           cout << "Applying configuration for 2.4GHz ..." << endl;
           at86rf215_setup_iq_radio_transmit (&dev_tx, at86rf215_rf_channel_2400mhz, freqOK ? m_Frequency : 2400e6, &tx_control, 0, at86rf215_iq_clock_data_skew_1_906ns); // e.g -  2400e6
        }
        
        // Return success status ...
        return CONST_AT86RF215_INIT_OK; 
        
    }
    
    // AT86RF215 deinit - RX mode  [private] ...
    // ----------------------------------------
    void altera_socfpga_sdr_sink_complex_impl::at86rf215_tx_deinit(bool showStatus){
        
        at86rf215_irq_st irq = {0};
        
        // Print status of the device at the end ...
        if(showStatus){
           at86rf215_get_iq_sync_status(&dev_tx);
           at86rf215_get_irqs(&dev_tx, &irq, 1);
        }
        
        // Close and reset device if requested ... 
        at86rf215_close(&dev_tx, 1);  // Reset dev 1, no reset 0 ...
    }
    
    // SocFPGA init - initialize FPGA part [private]
    // ---------------------------------------------
    int altera_socfpga_sdr_sink_complex_impl::socfpga_side_init(){
        
        /*
        # Enable TX control ...
        TX_CONTROL=0

        # Register meaning:
        # -----------------
        #  5  | 4 | 3 2 1 0
        #  TX |RST| SAMPLE RATE
        # ---------------------
        # 5 ... TX bit control
        # 6 ... RESET bit
        # 3 ... 0 - Sample rate control
        */

        int GPIO_VAL = 0;
        int TX_CONTROL = 0;
        
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
        
        int valuefd = open("/sys/bus/platform/drivers/altera_gpio/ff200000.gpio1/altera_gpio", O_RDWR);

        if (valuefd < 0){
            cout << "Cannot open GPIO to export it" <<endl;
            return CONST_FPGA_FW_SIDE_INIT_FAILED;
        }
        
        // GPIO_CLR ...
        int GPIO_CLR = ((TX_CONTROL << 5) | (GPIO_VAL & 0xF));
        string c_int_clr = to_string(GPIO_CLR); 
        write(valuefd, c_int_clr.c_str(), c_int_clr.size());
        
        // GPIO_WRITE / ASSERT RESET / ...
        int GPIO_WRITE = ((TX_CONTROL << 5) | 0x10 | (GPIO_VAL & 0xF)); 
        string c_int_wr = to_string(GPIO_WRITE); 
        write(valuefd, c_int_wr.c_str(), c_int_wr.size());
        
        // GPIO_CLR ...
        write(valuefd, c_int_clr.c_str(), c_int_clr.size());
        
        close(valuefd);
        
        // Return success     
        return CONST_FPGA_FW_SIDE_INIT_OK;
    }
    
    // SocFPGA init - deinitialize FPGA part [private]
    // ---------------------------------------------
    void altera_socfpga_sdr_sink_complex_impl::socfpga_side_deinit(){
        
        // Nothing to do on TX SIDE ...
        return;
        
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
