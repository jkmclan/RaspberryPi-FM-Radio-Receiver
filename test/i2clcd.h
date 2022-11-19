/*
*
* by Lewis Loflin www.bristolwatch.com lewis@bvu.net
* http://www.bristolwatch.com/rpi/i2clcd.htm
* Using wiringPi by Gordon Henderson
*
*
* Port over lcd_i2c.py to C and added improvements.
* Supports 16x2 and 20x4 screens.
* This was to learn now the I2C lcd displays operate.
* There is no warrenty of any kind use at your own risk.
*
*/

#ifndef __I2CLCD_H__
#define __I2CLCD_H__


#include <wiringPiI2C.h>
#include <wiringPi.h>
#include <stdlib.h>
#include <stdio.h>

// Define some device parameters
#define I2C_ADDR   0x27 // I2C device address

// Define some device constants
#define LCD_CHR  1 // Mode - Sending data
#define LCD_CMD  0 // Mode - Sending command

#define LINE1  0x80 // 1st line
#define LINE2  0xC0 // 2nd line

#define LCD_BACKLIGHT   0x08  // On
// LCD_BACKLIGHT = 0x00  # Off

#define ENABLE  0b00000100 // Enable bit

// added by Lewis
#ifdef __cplusplus
extern "C" {
#endif

extern void lcd_init(void);
extern void lcd_byte(int bits, int mode);
extern void lcd_toggle_enable(int bits);

extern void typeInt(int i);
extern void typeFloat(float myFloat);
extern void lcdLoc(int line); //move cursor
extern void ClrLcd(void); // clr LCD return home
extern void typeln(const char *s);
extern void typeChar(char val);

static int lcd_fd;  // seen by all subroutines

#ifdef __cplusplus
}
#endif

#endif
