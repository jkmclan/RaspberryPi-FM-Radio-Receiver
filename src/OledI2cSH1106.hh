/*
*
* Adapted from LCDGFX library example
* by Alexey Dynda https://github.com/lexus2k/lcdgfx
*
*/

#ifndef __I2COLED_H__
#define __I2COLED_H__

#include "lcdgfx.h"

#define LINE1  0x80 // 1st line - copied from LcdI2cHD44780.hh for now

class OledI2cSH1106 {

public:

    OledI2cSH1106();

    ~OledI2cSH1106();

    bool init(void);

    void typeInt(int val);
    void typeFloat(float val);
    void lcdLoc(int line); //move cursor
    void clrLcd(void); // clr LCD return home
    void typeln(const char *s);
    void typeChar(char val);

protected:

private:

    // Ctor arg (-1) is suitable for most platforms by default or (-1,{busId, addr, scl, sda, frequency})
    // By default, the I2C bus address and SCL/SDA pins are correctly identifed by the LCDGFX library
    DisplaySH1106_128x64_I2C m_display;

};

#endif
