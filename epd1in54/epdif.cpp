/**
 *  @filename   :   epdif.cpp
 *  @brief      :   Implements EPD interface functions
 *                  Users have to implement all the functions in epdif.cpp
 *  @author     :   Yehui from Waveshare
 *
 *  Copyright (C) Waveshare     August 10 2017
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "epdif.h"
EpdIf::EpdIf(){
    }
EpdIf::EpdIf(PinName mosi, 
             PinName miso, 
             PinName sclk, 
             PinName cs, 
             PinName dc, 
             PinName rst, 
             PinName busy) {
    m_spi = new SPI(mosi, miso, sclk);
    m_cs = new DigitalOut(cs);
    m_dc = new DigitalOut(dc);
    m_rst = new DigitalOut(rst);
    m_busy = new DigitalIn(busy);    
}

EpdIf::~EpdIf() {
}

void EpdIf::DigitalWrite(DigitalOut* pout, int value) {
    *pout = value;
}

int EpdIf::DigitalRead(DigitalIn* pin) {
    int ret = *pin;
    return ret;
}

void EpdIf::DelayMs(unsigned int delaytime) {
    wait_ms(delaytime);
}

void EpdIf::SpiTransfer(unsigned char data) {
    *m_cs = 0;
    m_spi->write(data);
    *m_cs = 1;
}

int EpdIf::IfInit(void){
    m_spi->format(8,0); 
    m_spi->frequency(2000000); 
    return 0;
}
    

