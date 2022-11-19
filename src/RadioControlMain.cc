/*

Read the Clock-Wise and Counter-Clock-Wise state 
changes of an incremental rotary encoder using 
the sysfs interface on a Raspberry Pi.


Jeff McLane <jkmclane68@yahoo.com>

*/

#include <stdio.h>
#include <unistd.h>

#include <iostream>

#include "RotaryEncoder.hh"
#include "LcdI2cHD44780.hh"

int main()
{
    // Initialize the list of FM radio center frequencies

    const int NUM_FM_FREQS = 103;
    double fm_center_freqs_MHz[NUM_FM_FREQS];
    double freq_MHz = 87.5;
    int    stn_idx = 0;
    for (int ii=0; ii<NUM_FM_FREQS; ++ii) {

        fm_center_freqs_MHz[ii] = freq_MHz;
        freq_MHz += 0.200; // 200 kHz spacing
    }

    // Initialize the radio frequencey dial

    RotaryEncoder* re = new RotaryEncoder();
    re->init();

    // Initialize the radio frequencey display

    LcdI2cHD44780 lcd;
    lcd.init();
    lcd.clrLcd();
    lcd.typeln("RF (MHz): ");
    lcd.typeFloat(fm_center_freqs_MHz[stn_idx]);

    // Do forever

    int loop_cnt = 1;
    while(loop_cnt < 1000000) {

        RotaryEncoder::RotaryStates_t rs = re->get_state();
        switch(rs) {
            case RotaryEncoder::ROT_INCREMENT:
            case RotaryEncoder::ROT_DECREMENT:
            {
                if ((stn_idx == 0) && (rs == RotaryEncoder::ROT_DECREMENT)) {
                    std::cout << "Cannot tune below : " << fm_center_freqs_MHz[stn_idx] << " MHz"<< std::endl;
                }
                else if ((stn_idx == NUM_FM_FREQS-1) && (rs == RotaryEncoder::ROT_INCREMENT)) {
                    std::cout << "Cannot tune above : " << fm_center_freqs_MHz[stn_idx] << " MHz"<< std::endl;
                } else {
                    stn_idx += rs ;
                    char center_freq_MHz_s[10];
                    sprintf(center_freq_MHz_s, "%5.1f", fm_center_freqs_MHz[stn_idx]);

                    std::cout << "FM Center Freq (MHz) : " <<  fm_center_freqs_MHz[stn_idx] << " / " << center_freq_MHz_s << std::endl;

                    lcd.lcdLoc(LINE1);
                    lcd.typeln("RF (MHz): ");
                    lcd.typeFloat(fm_center_freqs_MHz[stn_idx]);
                }
                break;
            }
            case RotaryEncoder::ROT_SW_PUSHED:
            {
                std::cout << "SW : PUSHED" << std::endl;
                break;
            }
            default:
            {
                break;
            }
        }

        loop_cnt++;

        // Sleep
        usleep(2000); // 2ms is less than 5% CPU

    } // End While

    delete re;
}
