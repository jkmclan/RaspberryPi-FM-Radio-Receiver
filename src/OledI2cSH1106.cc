
#include "OledI2cSH1106.hh"

OledI2cSH1106::OledI2cSH1106() :
    m_display(-1) {
}

OledI2cSH1106::~OledI2cSH1106() {
  clrLcd();
}

bool OledI2cSH1106::init() {

  /* Select the font to use with menu and all font functions */
  m_display.begin();
  m_display.setFixedFont(comic_sans_font24x32_123);

  //display.fill( 0x00 );

  return true;
}

void OledI2cSH1106::typeInt(int val) {

  char buffer[20];
  sprintf(buffer, "%d", val);
  typeln(buffer);
}

void OledI2cSH1106::typeFloat(float val) {

  char buffer[20];
  sprintf(buffer, "%4.1f", val);
  typeln(buffer);
}

void OledI2cSH1106::lcdLoc(int line) { 
  //move cursor
}

void OledI2cSH1106::clrLcd(void) {

  m_display.clear();
}

// this allows use of any size string
void OledI2cSH1106::typeln(const char *s) {

  m_display.clear();
  m_display.printFixed(0, 16, s, STYLE_NORMAL);
}

void OledI2cSH1106::typeChar(char val) {

  char buffer[2];
  memset(buffer, 0, sizeof(buffer));
  buffer[0] = val;
  typeln(buffer);
}
