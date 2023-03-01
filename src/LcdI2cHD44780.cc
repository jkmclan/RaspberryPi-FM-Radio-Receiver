
#include "LcdI2cHD44780.hh"

LcdI2cHD44780::LcdI2cHD44780() {
    m_fd = -1;
};

LcdI2cHD44780::~LcdI2cHD44780() {
    clrLcd();
}

bool LcdI2cHD44780::init() {

  // Initialize the WiringPi library
  if (wiringPiSetup () == -1) return false;
  if ((m_fd = wiringPiI2CSetup(I2C_ADDR)) < 0) return false;

  // Initialise display
  lcd_byte(0x33, LCD_CMD); // Initialise
  lcd_byte(0x32, LCD_CMD); // Initialise
  lcd_byte(0x06, LCD_CMD); // Cursor move direction
  lcd_byte(0x0C, LCD_CMD); // 0x0F On, Blink Off
  lcd_byte(0x28, LCD_CMD); // Data length, number of lines, font size
  lcd_byte(0x01, LCD_CMD); // Clear display
  delayMicroseconds(500);

  return true;
}

// float to string
void LcdI2cHD44780::typeFloat(float myFloat) {
  char buffer[20];
  sprintf(buffer, "%4.2f",  myFloat);
  typeln(buffer);
}

// int to string
void LcdI2cHD44780::typeInt(int i) {
  char array1[20];
  sprintf(array1, "%d",  i);
  typeln(array1);
}

// clr lcd go home loc 0x80
void LcdI2cHD44780::clrLcd(void) {
  lcd_byte(0x01, LCD_CMD);
  lcd_byte(0x02, LCD_CMD);
}

// go to location on LCD
void LcdI2cHD44780::lcdLoc(int line) {
  lcd_byte(line, LCD_CMD);
}

// out char to LCD at current position
void LcdI2cHD44780::typeChar(char val) {

  lcd_byte(val, LCD_CHR);
}


// this allows use of any size string
void LcdI2cHD44780::typeln(const char *s) {

  while ( *s ) lcd_byte(*(s++), LCD_CHR);

}

void LcdI2cHD44780::lcd_byte(int bits, int mode) {

  //Send byte to data pins
  // bits = the data
  // mode = 1 for data, 0 for command
  int bits_high;
  int bits_low;
  // uses the two half byte writes to LCD
  bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT ;
  bits_low = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT ;

  // High bits
  wiringPiI2CReadReg8(m_fd, bits_high);
  lcd_toggle_enable(bits_high);

  // Low bits
  wiringPiI2CReadReg8(m_fd, bits_low);
  lcd_toggle_enable(bits_low);
}

void LcdI2cHD44780::lcd_toggle_enable(int bits) {
  // Toggle enable pin on LCD display
  delayMicroseconds(500);
  wiringPiI2CReadReg8(m_fd, (bits | ENABLE));
  delayMicroseconds(500);
  wiringPiI2CReadReg8(m_fd, (bits & ~ENABLE));
  delayMicroseconds(500);
}

