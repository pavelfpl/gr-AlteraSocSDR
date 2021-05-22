/*
 * File:   fifo_buffer.h
 * Author: Pavel Fiala 2015 - 2018, v 2.0 version adds gr_complex (complex) type ...
 * Designed for GnuRadio FIFO/FTDI FT232h or Altera MSGDMA sink / source
 */

#ifndef FIFO_BUFFER_H
#define FIFO_BUFFER_H

#include <sys/types.h>
#include <iostream>

#include <gnuradio/gr_complex.h>      // gr_complex (complex) types ...
#include <gnuradio/thread/thread.h>
#include <AlteraSocSDR/gr_storage.h>  // gr_storage type ...

#define CONST_BUFFER_UINT_8 0         // unsigned char ...
#define CONST_BUFFER_COMPLEX 1        // gr_complex ...
#define CONST_BUFFER_GR_STORAGE 2     // gr_storage ...

// -----------------------------------------------------------------
// Already defined in gr_storage.h - <AlteraSocSDR/gr_storage.h> ...
// -----------------------------------------------------------------

/*
class gr_storage{
public:
  gr_storage();
  gr_storage(unsigned char msbData0_, unsigned char lsbData0_,
             unsigned char msbData1_, unsigned char lsbData1_){

                msbData0 = msbData0_; lsbData0 = lsbData0_;
                msbData1 = msbData1_; lsbData1 = lsbData1_;
             }

  // Copy constructor ...
  // --------------------
  gr_storage(const gr_storage &gr) {
              msbData0 = gr.msbData0; lsbData0 = gr.lsbData0;
              msbData1 = gr.msbData1; lsbData1 = gr.lsbData1;
            }

  unsigned char getMsbByte0() const {return msbData0;}
  unsigned char getLsbByte0() const {return lsbData0;}
  unsigned char getMsbByte1() const {return msbData1;}
  unsigned char getLsbByte1() const {return lsbData1;}
private:
  unsigned char msbData0;
  unsigned char lsbData0;
  unsigned char msbData1;
  unsigned char lsbData1;
};
*/

struct fifo_t{
    // Using only one of these three buffers ...
    // -----------------------------------------
    unsigned char *fifoBuffer;
    gr_complex *fifoBufferComplex;
    gr_storage *fifoBufferStorage;

    uint fifoHead;
    uint fifoTail;
    uint fifoMask;
    size_t fifoSize;
};

class fifo_buffer{

public:
    fifo_buffer(size_t bufferSize_ = 1048576,int buffer_type_ = CONST_BUFFER_GR_STORAGE);
    ~fifo_buffer();
    void fifo_changeSize(size_t bufferSize_);
    uint fifo_getFifoHead() const {return m_fifo.fifoHead;}
    uint fifo_getFifoTail() const {return m_fifo.fifoTail;}
    uint fifo_getFifoSize() const {return m_fifo.fifoSize;}
    uint fifo_write(const unsigned char *buff,const uint nBytes);
    uint fifo_read(unsigned char *buff,const uint nBytes);
    uint fifo_read_2(unsigned char *buff,const uint nBytes);
    uint fifo_read_3(unsigned char *buff,const uint nBytes);

    // ------------------------
    // gr_complex functions ...
    // ------------------------
    uint fifo_write_complex(const gr_complex *buff_complex, const uint nComplex);
    uint fifo_read_complex(gr_complex *buff_complex,const uint nComplex);
    
    // ------------------------
    // gr_storage functions ...
    // ------------------------
    uint fifo_write_storage(const gr_storage *buff_storage, const uint nStorage);
    uint fifo_read_storage(gr_storage *buff_storage, const uint nStorage);
private:
    boost::mutex fp_mutex;
    
    int m_buffer_type;
    int m_div_constant;
    struct fifo_t m_fifo;
    unsigned char *m_fifoBuffer;
    gr_complex *m_fifoBufferComplex;
    gr_storage *m_fifoBufferStorage;

    unsigned isPowerOfTwo(unsigned int x);
    void fifo_init(unsigned char *buff, gr_complex *buff_complex, gr_storage *buff_storage, size_t bufferSize_);
};

#endif /* FIFO_BUFFER_H */
