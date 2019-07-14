/**
 *  @filename   :   epd1in54.cpp
 *  @brief      :   Implements for e-paper library
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

#include <stdlib.h>
#include "epd1in54.h"

const unsigned char lut_full_update[] =
{
    0x02, 0x02, 0x01, 0x11, 0x12, 0x12, 0x22, 0x22, 
    0x66, 0x69, 0x69, 0x59, 0x58, 0x99, 0x99, 0x88, 
    0x00, 0x00, 0x00, 0x00, 0xF8, 0xB4, 0x13, 0x51, 
    0x35, 0x51, 0x51, 0x19, 0x01, 0x00
};

const unsigned char lut_partial_update[] =
{
    0x10, 0x18, 0x18, 0x08, 0x18, 0x18, 0x08, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x13, 0x14, 0x44, 0x12, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};



Epd::~Epd() {
};


Epd::Epd(PinName mosi,
         PinName miso,
         PinName sclk, 
         PinName cs, 
         PinName dc, 
         PinName rst, 
         PinName busy
         ):EpdIf(mosi, miso, sclk, cs, dc, rst, busy){
             
             width = EPD_WIDTH;
             height= EPD_HEIGHT;
             rotate = ROTATE_270;
             
}

int Epd::Init(const unsigned char* lut) {

    if(IfInit() != 0){
        return -1;
    }

    /* EPD hardware init start */
    Reset();
    SendCommand(DRIVER_OUTPUT_CONTROL);
    SendData((EPD_HEIGHT - 1) & 0xFF);
    SendData(((EPD_HEIGHT - 1) >> 8) & 0xFF);
    SendData(0x00);                     // GD = 0; SM = 0; TB = 0;
    SendCommand(BOOSTER_SOFT_START_CONTROL);
    SendData(0xD7);
    SendData(0xD6);
    SendData(0x9D);
    SendCommand(WRITE_VCOM_REGISTER);
    SendData(0xA8);                     // VCOM 7C
    SendCommand(SET_DUMMY_LINE_PERIOD);
    SendData(0x1A);                     // 4 dummy lines per gate
    SendCommand(SET_GATE_TIME);
    SendData(0x08);                     // 2us per line
    SendCommand(DATA_ENTRY_MODE_SETTING);
    SendData(0x03);                     // X increment; Y increment
    SetLut(lut);
    /* EPD hardware init end */
    return 0;
}

/**
 *  @brief: basic function for sending commands
 */
void Epd::SendCommand(unsigned char command) {
    DigitalWrite(m_dc, LOW);
    SpiTransfer(command);
}

/**
 *  @brief: basic function for sending data
 */
void Epd::SendData(unsigned char data) {
    DigitalWrite(m_dc, HIGH);
    SpiTransfer(data);
}

/**
 *  @brief: Wait until the m_busy goes HIGH
 */
void Epd::WaitUntilIdle(void) {
    while(DigitalRead(m_busy) == 1) {      //0: busy, 1: idle
        DelayMs(100);
    }      
}

/**
 *  @brief: module reset.
 *          often used to awaken the module in deep sleep,
 *          see Epd::Sleep();
 */
void Epd::Reset(void) {
    DigitalWrite(m_rst, LOW);                //module reset    
    DelayMs(200);
    DigitalWrite(m_rst, HIGH);
    DelayMs(200);    
}


/**
 *  @brief: set the look-up table register
 */
void Epd::SetLut(const unsigned char* lut) {
    SendCommand(WRITE_LUT_REGISTER);
    /* the length of look-up table is 30 bytes */
    for (int i = 0; i < 30; i++) {
        SendData(lut[i]);
    }
}


/**
 *  @brief: put an image buffer to the frame memory.
 *          this won't update the display.
 */
void Epd::SetFrameMemory(
    const unsigned char* image_buffer,
    int x,
    int y,
    int image_width,
    int image_height
) {
    int x_end;
    int y_end;

    if (
        image_buffer == NULL ||
        x < 0 || image_width < 0 ||
        y < 0 || image_height < 0
    ) {
        return;
    }
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    x &= 0xF8;
    image_width &= 0xF8;
    if (x + image_width >= this->width) {
        x_end = this->width - 1;
    } else {
        x_end = x + image_width - 1;
    }
    if (y + image_height >= this->height) {
        y_end = this->height - 1;
    } else {
        y_end = y + image_height - 1;
    }
    SetMemoryArea(x, y, x_end, y_end);
    SetMemoryPointer(x, y);
    SendCommand(WRITE_RAM);
    /* send the image data */
    for (int j = 0; j < y_end - y + 1; j++) {
        for (int i = 0; i < (x_end - x + 1) / 8; i++) {
            SendData(image_buffer[i + j * (image_width / 8)]);
        }
    }
}

/**
 *  @brief: clear the frame memory with the specified color.
 *          this won't update the display.
 */
void Epd::ClearFrameMemory(unsigned char color) {
    SetMemoryArea(0, 0, this->width - 1, this->height - 1);
    SetMemoryPointer(0, 0);
    SendCommand(WRITE_RAM);
    /* send the color data */
    for (int i = 0; i < this->width / 8 * this->height; i++) {
        SendData(color);
    }
}

/**
 *  @brief: update the display
 *          there are 2 memory areas embedded in the e-paper display
 *          but once this function is called,
 *          the the next action of SetFrameMemory or ClearFrame will 
 *          set the other memory area.
 */
void Epd::DisplayFrame(void) {
    SendCommand(DISPLAY_UPDATE_CONTROL_2);
    SendData(0xC4);
    SendCommand(MASTER_ACTIVATION);
    SendCommand(TERMINATE_FRAME_READ_WRITE);
    WaitUntilIdle();
}

/**
 *  @brief: private function to specify the memory area for data R/W
 */
void Epd::SetMemoryArea(int x_start, int y_start, int x_end, int y_end) {
    SendCommand(SET_RAM_X_ADDRESS_START_END_POSITION);
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    SendData((x_start >> 3) & 0xFF);
    SendData((x_end >> 3) & 0xFF);
    SendCommand(SET_RAM_Y_ADDRESS_START_END_POSITION);
    SendData(y_start & 0xFF);
    SendData((y_start >> 8) & 0xFF);
    SendData(y_end & 0xFF);
    SendData((y_end >> 8) & 0xFF);
}

/**
 *  @brief: private function to specify the start point for data R/W
 */
void Epd::SetMemoryPointer(int x, int y) {
    SendCommand(SET_RAM_X_ADDRESS_COUNTER);
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    SendData((x >> 3) & 0xFF);
    SendCommand(SET_RAM_Y_ADDRESS_COUNTER);
    SendData(y & 0xFF);
    SendData((y >> 8) & 0xFF);
    WaitUntilIdle();
}


/**
 *  @brief: After this command is transmitted, the chip would enter the 
 *          deep-sleep mode to save power. 
 *          The deep sleep mode would return to standby by hardware reset. 
 *          You can use Epd::Init() to awaken
 */
void Epd::Sleep() {
    SendCommand(DEEP_SLEEP_MODE);
    WaitUntilIdle();
}


void Epd::SetRotate(int rotate){
    if (rotate == ROTATE_0){
        rotate = ROTATE_0;
        width = EPD_WIDTH;
        height = EPD_HEIGHT;
    }
    else if (rotate == ROTATE_90){
        rotate = ROTATE_90;
        width = EPD_HEIGHT;
        height = EPD_WIDTH;
    }
    else if (rotate == ROTATE_180){
        rotate = ROTATE_180;
        width = EPD_WIDTH;
        height = EPD_HEIGHT;
    }
    else if (rotate == ROTATE_270){ 
        rotate = ROTATE_270;
        width = EPD_HEIGHT;
        height = EPD_WIDTH;
    }
}


void Epd::SetPixel(unsigned char* frame_buffer, int x, int y, int colored){
    if (x < 0 || x >= width || y < 0 || y >= height){
        return;
    }
    if (rotate == ROTATE_0){
        SetAbsolutePixel(frame_buffer, x, y, colored);
    }
    else if (rotate == ROTATE_90){
        int point_temp = x;
        x = EPD_WIDTH - y;
        y = point_temp;
        SetAbsolutePixel(frame_buffer, x, y, colored);
    }
    else if (rotate == ROTATE_180){
        x = EPD_WIDTH - x;
        y = EPD_HEIGHT- y;
        SetAbsolutePixel(frame_buffer, x, y, colored);
    }
    else if (rotate == ROTATE_270){
        int point_temp = x;
        x = y;
        y = EPD_HEIGHT - point_temp;
        SetAbsolutePixel(frame_buffer, x, y, colored);
    }
}

void Epd::SetAbsolutePixel(unsigned char *frame_buffer, int x, int y, int colored){
    // To avoid display orientation effects
    // use EPD_WIDTH instead of self.width
    // use EPD_HEIGHT instead of self.height
    if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT){
        return;
    }
    if (colored){
        frame_buffer[(x + y * EPD_WIDTH) / 8] &= ~(0x80 >> (x % 8));
    }
    else{
        frame_buffer[(x + y * EPD_WIDTH) / 8] |= 0x80 >> (x % 8);
    }
}

void Epd::DrawLine(unsigned char*frame_buffer, int x0, int y0, int x1, int y1, int colored){
    // Bresenham algorithm
    int dx = x1 - x0 >= 0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 - y0 <= 0 ? y1 - y0 : y0 - y1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while((x0 != x1) && (y0 != y1)){
        SetPixel(frame_buffer, x0, y0 , colored);
        if (2 * err >= dy){
            err += dy;
            x0 += sx;
        }
        if (2 * err <= dx){
            err += dx;
            y0 += sy;
        }
    }
}

 void Epd::DrawHorizontalLine(unsigned char *frame_buffer, int x, int y, int width, int colored){
    for (int i=x; i<x + width; i++){
        SetPixel(frame_buffer, i, y, colored);
    }
}

 void Epd::DrawVerticalLine(unsigned char *frame_buffer, int x, int y, int height, int colored){
    for (int i=y; i<y + height; i++){
        SetPixel(frame_buffer, x, i, colored);
    }
}

 void Epd::DrawRectangle(unsigned char *frame_buffer, int x0, int y0, int x1, int y1, int colored){
    int min_x = x1 > x0 ? x0 : x1;
    int max_x = x1 > x0 ? x1 : x0;
    int min_y = y1 > y0 ? y0 : y1;
    int max_y = y1 > y0 ? y1 : y0;
    DrawHorizontalLine(frame_buffer, min_x, min_y, max_x - min_x + 1, colored);
    DrawHorizontalLine(frame_buffer, min_x, max_y, max_x - min_x + 1, colored);
    DrawVerticalLine(frame_buffer, min_x, min_y, max_y - min_y + 1, colored);
    DrawVerticalLine(frame_buffer, max_x, min_y, max_y - min_y + 1, colored);
}


void Epd::DrawFilledRectangle(unsigned char *frame_buffer, int x0, int y0, int x1, int y1, int colored){
    int min_x = x1 > x0 ? x0 : x1;
    int max_x = x1 > x0 ? x1 : x0;
    int min_y = y1 > y0 ? y0 : y1;
    int max_y = y1 > y0 ? y1 : y0;

    for (int i=min_x; i < max_x+1; i++){
        DrawVerticalLine(frame_buffer, i, min_y, max_y - min_y + 1, colored);
    }
}

void Epd::DrawCircle(unsigned char *frame_buffer, int x, int y, int radius, int colored){
    // Bresenham algorithm
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    if (x >= width || y >= height){
        return;
    }
    while ( 1 ){
        SetPixel(frame_buffer, x - x_pos, y + y_pos, colored);
        SetPixel(frame_buffer, x + x_pos, y + y_pos, colored);
        SetPixel(frame_buffer, x + x_pos, y - y_pos, colored);
        SetPixel(frame_buffer, x - x_pos, y - y_pos, colored);
        int e2 = err;
        if (e2 <= y_pos){
            y_pos += 1;
            err += y_pos * 2 + 1;
            if(-x_pos == y_pos && e2 <= x_pos){
                e2 = 0;
            }
        }
        if (e2 > x_pos){
            x_pos += 1;
            err += x_pos * 2 + 1;
        }
        if (x_pos > 0){
            break;
        }
    }
}

void Epd::DrawFilledCircle(unsigned char* frame_buffer, int x, int y, int radius, int colored){
    // Bresenham algorithm
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    if (x >= width || y >= height){
        return;
    }
    while ( 1 ){
        SetPixel(frame_buffer, x - x_pos, y + y_pos, colored);
        SetPixel(frame_buffer, x + x_pos, y + y_pos, colored);
        SetPixel(frame_buffer, x + x_pos, y - y_pos, colored);
        SetPixel(frame_buffer, x - x_pos, y - y_pos, colored);
        DrawHorizontalLine(frame_buffer, x + x_pos, y + y_pos, 2 * (-x_pos) + 1, colored);
        DrawHorizontalLine(frame_buffer, x + x_pos, y - y_pos, 2 * (-x_pos) + 1, colored);
        int e2 = err;
        if (e2 <= y_pos){
            y_pos += 1;
            err += y_pos * 2 + 1;
            if(-x_pos == y_pos && e2 <= x_pos){
                e2 = 0;
            }
        }
        if (e2 > x_pos){
            x_pos  += 1;
            err += x_pos * 2 + 1;
        }
        if (x_pos > 0){
            break;
        }
    }
}



/**
 *  @brief: this draws a charactor on the frame buffer but not refresh
 */
void Epd::DrawCharAt(unsigned char *frame_buffer, int x, int y, char ascii_char, sFONT* font, int colored) {
    int i, j;
    unsigned int char_offset = (ascii_char - ' ') * font->Height * (font->Width / 8 + (font->Width % 8 ? 1 : 0));
    const unsigned char* ptr = &font->table[char_offset];

    for (j = 0; j < font->Height; j++) {
        for (i = 0; i < font->Width; i++) {
            if (*ptr & (0x80 >> (i % 8))) {
                SetPixel(frame_buffer, x + i, y + j, colored);
            }
            if (i % 8 == 7) {
                ptr++;
            }
        }
        if (font->Width % 8 != 0) {
            ptr++;
        }
    }
}

/**
*  @brief: this displays a string on the frame buffer but not refresh
*/
void Epd::DrawStringAt(unsigned char *frame_buffer, int x, int y, const char* text, sFONT* font, int colored) {
    const char* p_text = text;
    unsigned int counter = 0;
    int refcolumn = x;
    
    /* Send the string character by character on EPD */
    while (*p_text != 0) {
        /* Display one character on EPD */
        DrawCharAt(frame_buffer, refcolumn, y, *p_text, font, colored);
        /* Decrement the column position by 16 */
        refcolumn += font->Width;
        /* Point on the next character */
        p_text++;
        counter++;
    }
}