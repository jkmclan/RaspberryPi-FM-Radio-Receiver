
#include <i2clcd.h>

int main()   {

  printf("main: Here 1\n");
  if (wiringPiSetup () == -1) exit (1);

  printf("main: Here 2\n");
  lcd_fd = wiringPiI2CSetup(I2C_ADDR);
  printf("main: Here 3\n");

  printf("lcd_fd = %d\n", lcd_fd);

  lcd_init(); // setup LCD
  printf("main: Here 4\n");

  char array1[] = "Hello world!";

  while (1)   {

    lcdLoc(LINE1);
    printf("main: Here 5\n");
    typeln("Using wiringPi");
    printf("main: Here 6\n");
    lcdLoc(LINE2);
    typeln("Geany editor.");

    delay(2000);
    ClrLcd();
    lcdLoc(LINE1);
    typeln("I2c  Programmed");
    lcdLoc(LINE2);
    typeln("in C not Python.");

    delay(2000);
    ClrLcd();
    lcdLoc(LINE1);
    typeln("Arduino like");
    lcdLoc(LINE2);
    typeln("fast and easy.");

    delay(2000);
    ClrLcd();
    lcdLoc(LINE1);
    typeln(array1);

    delay(2000);
    ClrLcd(); // defaults LINE1
    typeln("Int  ");
    int value = 20125;
    typeInt(value);

    delay(2000);
    lcdLoc(LINE2);
    typeln("Float ");
    float FloatVal = 10045.25989;
    typeFloat(FloatVal);
    delay(2000);
  }

  return 0;

}

// float to string
void typeFloat(float myFloat)   {
  char buffer[20];
  sprintf(buffer, "%4.2f",  myFloat);
  typeln(buffer);
}

// int to string
void typeInt(int i)   {
  char array1[20];
  sprintf(array1, "%d",  i);
  typeln(array1);
}

// clr lcd go home loc 0x80
void ClrLcd(void)   {
  lcd_byte(0x01, LCD_CMD);
  lcd_byte(0x02, LCD_CMD);
}

// go to location on LCD
void lcdLoc(int line)   {
  lcd_byte(line, LCD_CMD);
}

// out char to LCD at current position
void typeChar(char val)   {

  lcd_byte(val, LCD_CHR);
}


// this allows use of any size string
void typeln(const char *s)   {

  while ( *s ) lcd_byte(*(s++), LCD_CHR);

}

void lcd_byte(int bits, int mode)   {

  //Send byte to data pins
  // bits = the data
  // mode = 1 for data, 0 for command
  int bits_high;
  int bits_low;
  // uses the two half byte writes to LCD
  bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT ;
  bits_low = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT ;

  // High bits
  wiringPiI2CReadReg8(lcd_fd, bits_high);
  lcd_toggle_enable(bits_high);

  // Low bits
  wiringPiI2CReadReg8(lcd_fd, bits_low);
  lcd_toggle_enable(bits_low);
}

void lcd_toggle_enable(int bits)   {
  // Toggle enable pin on LCD display
  delayMicroseconds(500);
  wiringPiI2CReadReg8(lcd_fd, (bits | ENABLE));
  delayMicroseconds(500);
  wiringPiI2CReadReg8(lcd_fd, (bits & ~ENABLE));
  delayMicroseconds(500);
}


void lcd_init()   {
  // Initialise display
  lcd_byte(0x33, LCD_CMD); // Initialise
  lcd_byte(0x32, LCD_CMD); // Initialise
  lcd_byte(0x06, LCD_CMD); // Cursor move direction
  lcd_byte(0x0C, LCD_CMD); // 0x0F On, Blink Off
  lcd_byte(0x28, LCD_CMD); // Data length, number of lines, font size
  lcd_byte(0x01, LCD_CMD); // Clear display
  delayMicroseconds(500);
}
