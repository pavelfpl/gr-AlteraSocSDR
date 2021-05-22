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


#include "fifo_buffer.h"

#define CONST_POWER_OF_TWO_ARRAY_L 12 // Define powersOfTwo length [12]
#define CONST_POWER_OF_TWO_ARRAY_D 1  // Define powersOfTwo default element [67108864]

const unsigned int powersOfTwo[CONST_POWER_OF_TWO_ARRAY_L] = {1048576,2097152,4194304,8388608,
                                     16777216,33554432,67108864,134217728,268435456,536870912,
                                     1073741824,2147483648};

// fifo_buffer constructor [public]
// --------------------------------
fifo_buffer::fifo_buffer(size_t bufferSize_, int buffer_type_){
    m_buffer_type = buffer_type_;

    switch(m_buffer_type){
      case CONST_BUFFER_UINT_8:
        m_div_constant = 1; m_fifoBuffer = new unsigned char[bufferSize_]; m_fifoBufferComplex = 0; m_fifoBufferStorage = 0; break;
      case CONST_BUFFER_COMPLEX:
        m_div_constant = 8; m_fifoBufferComplex = new gr_complex[bufferSize_/m_div_constant]; m_fifoBuffer = 0; m_fifoBufferStorage = 0; break;
      case CONST_BUFFER_GR_STORAGE:
        m_div_constant = 4; m_fifoBufferStorage = new gr_storage[bufferSize_/m_div_constant]; m_fifoBuffer = 0; m_fifoBufferComplex = 0; break;
      default:
        m_div_constant = 4; m_fifoBufferStorage = new gr_storage[bufferSize_/m_div_constant]; m_fifoBuffer = 0; m_fifoBufferComplex = 0; break;
    }

    fifo_init(m_fifoBuffer,m_fifoBufferComplex,m_fifoBufferStorage, bufferSize_);
}

// fifo_buffer destructor [public]
// -------------------------------
fifo_buffer::~fifo_buffer(){

  switch(m_buffer_type){
    case CONST_BUFFER_UINT_8:
      if(m_fifoBuffer != 0){ delete[] m_fifoBuffer; m_fifoBuffer = 0; m_fifoBufferComplex = 0; m_fifoBufferStorage = 0;} break;
    case CONST_BUFFER_COMPLEX:
      if(m_fifoBufferComplex != 0){ delete[] m_fifoBufferComplex; m_fifoBufferComplex = 0; m_fifoBuffer = 0; m_fifoBufferStorage = 0; } break;
    case CONST_BUFFER_GR_STORAGE:
      if(m_fifoBufferStorage != 0){ delete[] m_fifoBufferStorage; m_fifoBufferStorage = 0; m_fifoBuffer = 0; m_fifoBufferComplex = 0; } break;
    default:
      if(m_fifoBufferStorage != 0){ delete[]m_fifoBufferStorage; m_fifoBufferStorage = 0; m_fifoBuffer = 0; m_fifoBufferComplex = 0; } break;
  }
}

// isPowerOfTwo function [private]
// -------------------------------
unsigned int fifo_buffer::isPowerOfTwo(unsigned int x){

    int j=0;

    for (j=0;j<CONST_POWER_OF_TWO_ARRAY_L;j++){
         if(powersOfTwo[j] == x){
            return x;
         }
    }

    return powersOfTwo[CONST_POWER_OF_TWO_ARRAY_D];
}

// fifo_init function [private]
// ----------------------------
void fifo_buffer::fifo_init(unsigned char *buff, gr_complex *buff_complex, gr_storage *buff_storage, size_t bufferSize_){
    m_fifo.fifoBuffer = buff;
    m_fifo.fifoBufferComplex = buff_complex;
    m_fifo.fifoBufferStorage = buff_storage;
    m_fifo.fifoHead = 0;
    m_fifo.fifoTail = 0;
    m_fifo.fifoMask = (bufferSize_/m_div_constant)-1;
    m_fifo.fifoSize = (bufferSize_/m_div_constant);
}

// fifo_changeSize function [public]
// ---------------------------------
void fifo_buffer::fifo_changeSize(size_t bufferSize_){

    size_t bufferSize;

    switch(m_buffer_type){
      case CONST_BUFFER_UINT_8:
        if(m_fifoBuffer != 0){ delete[] m_fifoBuffer; m_fifoBuffer = 0; m_fifoBufferComplex = 0; m_fifoBufferStorage = 0;} break;
      case CONST_BUFFER_COMPLEX:
        if(m_fifoBufferComplex != 0){ delete[]m_fifoBufferComplex; m_fifoBufferComplex = 0; m_fifoBuffer = 0; m_fifoBufferStorage = 0;}  break;
      case CONST_BUFFER_GR_STORAGE:
        if(m_fifoBufferStorage != 0){ delete[] m_fifoBufferStorage; m_fifoBufferStorage = 0; m_fifoBuffer = 0; m_fifoBufferComplex = 0; } break;
      default:
        if(m_fifoBufferStorage != 0){ delete[] m_fifoBufferStorage; m_fifoBufferStorage = 0; m_fifoBuffer = 0; m_fifoBufferComplex = 0; } break;
    }

    bufferSize = isPowerOfTwo(bufferSize_);

    switch(m_buffer_type){
      case CONST_BUFFER_UINT_8:
        m_div_constant = 1; m_fifoBuffer = new unsigned char[bufferSize_]; m_fifoBufferComplex = 0; m_fifoBufferStorage = 0; break;
      case CONST_BUFFER_COMPLEX:
        m_div_constant = 8; m_fifoBufferComplex = new gr_complex[bufferSize_/m_div_constant]; m_fifoBuffer = 0; m_fifoBufferStorage = 0; break;
      case CONST_BUFFER_GR_STORAGE:
        m_div_constant = 4; m_fifoBufferStorage = new gr_storage[bufferSize_/m_div_constant]; m_fifoBuffer = 0; m_fifoBufferComplex = 0; break;
      default:
        m_div_constant = 4; m_fifoBufferStorage = new gr_storage[bufferSize_/m_div_constant]; m_fifoBuffer = 0; m_fifoBufferComplex = 0; break;
    }

    fifo_init(m_fifoBuffer, m_fifoBufferComplex, m_fifoBufferStorage, bufferSize_);

}

// fifo_write function [public]
// ----------------------------
uint fifo_buffer::fifo_write(const unsigned char *buff,const uint nBytes){

    int j = 0;
    const unsigned char *p = buff;

    gr::thread::scoped_lock lock(fp_mutex);   // shared resources ...
    
    for(j=0;j<nBytes;j++){
        if(m_fifo.fifoHead+1 == m_fifo.fifoTail || ((m_fifo.fifoHead+1 == m_fifo.fifoSize) && (m_fifo.fifoTail==0))){
           return j;        // fifo buffer overflow - no more space to write ...
        }else{
           uint fifoHeadTmp = m_fifo.fifoHead;
           m_fifo.fifoBuffer[fifoHeadTmp] =*p++;
           fifoHeadTmp++;
           m_fifo.fifoHead = (fifoHeadTmp & m_fifo.fifoMask);  // if(m_fifo.fifoHead == m_fifo.fifoSize) {m_fifo.fifoHead = 0;}
        }
    }

    return nBytes;  // fifo write OK ...
}

// fifo_read function [public]
// ---------------------------
uint fifo_buffer::fifo_read(unsigned char *buff,const uint nBytes){

    int j = 0;

    gr::thread::scoped_lock lock(fp_mutex);   // shared resources ...
    
    for(j=0;j<nBytes;j++){
        
        if(m_fifo.fifoHead == m_fifo.fifoTail ){
           return j; // no or more data to read ...
        }else{
           uint fifoTailTmp = m_fifo.fifoTail;
           *buff++= m_fifo.fifoBuffer[fifoTailTmp];
           fifoTailTmp++;
           m_fifo.fifoTail = (fifoTailTmp & m_fifo.fifoMask); // if(m_fifo.fifoTail == m_fifo.fifoSize) {m_fifo.fifoTail = 0;}
        }
    }

    return nBytes;  // fifo read OK ...

}

// fifo_read_2 function - special function for uint16 read - two bytes [public]
// ----------------------------------------------------------------------------
uint fifo_buffer::fifo_read_2(unsigned char *buff,const uint nBytes){

    int j = 0;
    
    gr::thread::scoped_lock lock(fp_mutex);   // shared resources ...

    for(j=0;j<nBytes;j++){
        if(m_fifo.fifoHead == m_fifo.fifoTail ){ // no or more data to read ...
	      // Returns only even number of items ...
	      // -------------------------------------
	      if((j % 2) != 0){
	         if(m_fifo.fifoTail == 0){
	            m_fifo.fifoTail = m_fifo.fifoMask;
	         }else {
	            m_fifo.fifoTail = m_fifo.fifoTail-1; // m_fifo.fifoTail correction
	         }
	         return (j-1);	  		                   // return item count (here j-1)
	      }else{
	         return j;
	      }
      }else{
           uint fifoTailTmp = m_fifo.fifoTail;
           *buff++= m_fifo.fifoBuffer[fifoTailTmp];
           fifoTailTmp++;
           m_fifo.fifoTail = (fifoTailTmp & m_fifo.fifoMask); // if(m_fifo.fifoTail == m_fifo.fifoSize) {m_fifo.fifoTail = 0;}
        }
    }

    return nBytes;  // fifo read OK ...
}

// fifo_read_2 function - special function for 2*uint16 [complex] read - four bytes [public]
// -----------------------------------------------------------------------------------------
uint fifo_buffer::fifo_read_3(unsigned char *buff,const uint nBytes){

    int j = 0;
    
    gr::thread::scoped_lock lock(fp_mutex);   // shared resources ...
    
    for(j=0;j<nBytes;j++){
      if(m_fifo.fifoHead == m_fifo.fifoTail ){ // no or more data to read ...
	       // Returns only mod(x,4) = 0 items ...
	       // -----------------------------------
	       int rem =  (j % 4);
	       // if rem != 0
	       // -----------
	       if(rem != 0){
	         if((m_fifo.fifoTail - rem) <= 0){
	            m_fifo.fifoTail = m_fifo.fifoMask + m_fifo.fifoTail - rem;
	         }else {
	            m_fifo.fifoTail = m_fifo.fifoTail-rem; // m_fifo.fifoTail correction
	         }
	         return (j-rem);	  		                   // return item count (here: j - rem)
	       }else{
	         return j;
	       }
       }else{
           uint fifoTailTmp = m_fifo.fifoTail;
           *buff++= m_fifo.fifoBuffer[fifoTailTmp];
           fifoTailTmp++;
           m_fifo.fifoTail = (fifoTailTmp & m_fifo.fifoMask); // if(m_fifo.fifoTail == m_fifo.fifoSize) {m_fifo.fifoTail = 0;}
       }
    }

    return nBytes;  // fifo read OK ...
}

// gr_complex functions ...
// ------------------------
uint fifo_buffer::fifo_write_complex(const gr_complex *buff_complex, const uint nComplex){

  int j = 0;
  
  const gr_complex *p_c = buff_complex;
  
  gr::thread::scoped_lock lock(fp_mutex);   // shared resources ...

  for(j=0;j<nComplex;j++){
      if(m_fifo.fifoHead+1 == m_fifo.fifoTail || ((m_fifo.fifoHead+1 == m_fifo.fifoSize) && (m_fifo.fifoTail==0))){
         return j;        // fifo buffer overflow - no more space to write ...
      }else{
         uint fifoHeadTmp = m_fifo.fifoHead;
         m_fifo.fifoBufferComplex[fifoHeadTmp] =*p_c++;
         fifoHeadTmp++;
         m_fifo.fifoHead = (fifoHeadTmp & m_fifo.fifoMask);  // if(m_fifo.fifoHead == m_fifo.fifoSize) {m_fifo.fifoHead = 0;}
      }
  }

  return nComplex;  // fifo write OK ...
}

uint fifo_buffer::fifo_read_complex(gr_complex *buff_complex,const uint nComplex){

  int j = 0;

  gr::thread::scoped_lock lock(fp_mutex);   // shared resources ...
  
  for(j=0;j<nComplex;j++){

      if(m_fifo.fifoHead == m_fifo.fifoTail ){
         return j; // no or more data to read ...
      }else{
         uint fifoTailTmp = m_fifo.fifoTail;
         *buff_complex++= m_fifo.fifoBufferComplex[fifoTailTmp];
         fifoTailTmp++;
         m_fifo.fifoTail = (fifoTailTmp & m_fifo.fifoMask); // if(m_fifo.fifoTail == m_fifo.fifoSize) {m_fifo.fifoTail = 0;}
      }
  }

  return nComplex;  // fifo read OK ..
}

// gr_storage functions ...
// ------------------------
uint fifo_buffer::fifo_write_storage(const gr_storage *buff_storage, const uint nStorage){

  int j = 0;
  const gr_storage *p_gr = buff_storage;

  gr::thread::scoped_lock lock(fp_mutex);   // shared resources ...
  
  for(j=0;j<nStorage;j++){
      if(m_fifo.fifoHead+1 == m_fifo.fifoTail || ((m_fifo.fifoHead+1 == m_fifo.fifoSize) && (m_fifo.fifoTail==0))){
         return j;                                          // fifo buffer overflow - no more space to write ...
      }else{
         // uint fifoHeadTmp = m_fifo.fifoHead;
         m_fifo.fifoBufferStorage[m_fifo.fifoHead] =*p_gr++;
         m_fifo.fifoHead++;
         // m_fifo.fifoHead = (fifoHeadTmp & m_fifo.fifoMask);  // 
	if(m_fifo.fifoHead == m_fifo.fifoSize) {m_fifo.fifoHead = 0;}
      }
  }

  return nStorage;  // fifo write OK ...
}

uint fifo_buffer::fifo_read_storage(gr_storage *buff_storage,const uint nStorage){

  int j = 0;
  
  gr::thread::scoped_lock lock(fp_mutex);   // shared resources ...

  for(j=0;j<nStorage;j++){

      if(m_fifo.fifoHead == m_fifo.fifoTail ){
         return j; // no or more data to read ...
      }else{
         // uint fifoTailTmp = m_fifo.fifoTail;
         *buff_storage++ = m_fifo.fifoBufferStorage[m_fifo.fifoTail];
         m_fifo.fifoTail++;
         //m_fifo.fifoTail = (fifoTailTmp & m_fifo.fifoMask); // 
	if(m_fifo.fifoTail == m_fifo.fifoSize) {m_fifo.fifoTail = 0;}
      }
  }

  return nStorage;  // fifo read OK ..
}
