/*

*/

#include <stdio.h>
#include <unistd.h>

#include <iostream>

#include "RotaryEncoder.hh"

int main()
{
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
                    std::cout << "Cannot tune below stn_idx : " << stn_idx << std::endl;
                } else {
                    stn_idx += rs ;
                    std::cout << "stn_idx : " << stn_idx << std::endl;
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
