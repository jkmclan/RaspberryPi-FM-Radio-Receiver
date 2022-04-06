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

int main()
{
    // Initialize the list of FM radio center frequencies

    const int NUM_FM_FREQS = 103;
    double fm_center_freqs_MHz[NUM_FM_FREQS];
    double freq_MHz = 87.5;
    for (int ii=0; ii<NUM_FM_FREQS; ++ii) {

        fm_center_freqs_MHz[ii] = freq_MHz;
        freq_MHz += 0.200; // 200 kHz spacing
    }

    RotaryEncoder* re = new RotaryEncoder();

    re->init();

    // Do forever

    int stn_idx = 0;
    int loop_cnt = 1;
    while(loop_cnt < 1000000) {

        RotaryEncoder::RotaryStates_t rs = re->get_state();
        switch(rs) {
            case RotaryEncoder::ROT_INCREMENT:
            case RotaryEncoder::ROT_DECREMENT:
            {
                if ((stn_idx == 0) && (rs == RotaryEncoder::ROT_DECREMENT)) {
                    std::cout << "Cannot tune below : " << fm_center_freqs_MHz[stn_idx] << " MHz"<< std::endl;
                } else {
                    stn_idx += rs ;
                    std::cout << "FM Center Freq (MHz) : " <<  fm_center_freqs_MHz[stn_idx] << std::endl;
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
