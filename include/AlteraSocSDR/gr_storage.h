#ifndef GR_STORAGE_H
#define GR_STORAGE_H

class gr_storage{
public:
  gr_storage(){};
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

#endif