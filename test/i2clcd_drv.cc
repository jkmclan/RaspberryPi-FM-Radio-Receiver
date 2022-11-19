

#include "LcdI2cHD44780.hh"
 
int main()   {

/*
  if (wiringPiSetup () == -1) exit (1);
  lcd_fd = wiringPiI2CSetup(I2C_ADDR);
  lcd_init(); // setup LCD
*/

  LcdI2cHD44780 lcd;
  lcd.init();  

  char array1[] = "Hello world!";

  while (1)   {

    lcd.lcdLoc(LINE1);
    lcd.typeln("Using wiringPi");
    lcd.lcdLoc(LINE2);
    lcd.typeln("Geany editor.");

    delay(2000);
    lcd.clrLcd();
    lcd.lcdLoc(LINE1);
    lcd.typeln("I2c  Programmed");
    lcd.lcdLoc(LINE2);
    lcd.typeln("in C not Python.");

    delay(2000);
    lcd.clrLcd();
    lcd.lcdLoc(LINE1);
    lcd.typeln("Arduino like");
    lcd.lcdLoc(LINE2);
    lcd.typeln("fast and easy.");

    delay(2000);
    lcd.clrLcd();
    lcd.lcdLoc(LINE1);
    lcd.typeln(array1);

    delay(2000);
    lcd.clrLcd(); // defaults LINE1
    lcd.typeln("Int  ");
    int value = 20125;
    lcd.typeInt(value);

    delay(2000);
    lcd.lcdLoc(LINE2);
    lcd.typeln("Float ");
    float FloatVal = 10045.25989;
    lcd.typeFloat(FloatVal);
    delay(2000);
  }

  return 0;

}

