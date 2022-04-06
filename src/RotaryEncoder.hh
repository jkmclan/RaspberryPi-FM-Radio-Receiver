/*

A class which reads the Clock-Wise, 
Counter-Clock-Wise, and momentary 
switch state changes of an incremental 
rotary encoder using the sysfs interface 
on a Raspberry Pi.


Jeff McLane <jkmclane68@yahoo.com>

*/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#define ROT_CLK_S "23"
#define ROT_DT_S  "24"
#define ROT_SW_S  "25"


class RotaryEncoder {

public:

    typedef enum RotaryStates {
                       ROT_DECREMENT=-1,
                       ROT_NC=0,
                       ROT_INCREMENT=1,
                       ROT_SW_PUSHED=2
                      } RotaryStates_t;
 
    RotaryEncoder() {
        m_RotaryQuadratureState = ROT_IDLE;
        m_RotarySwitchState = ROT_SW_1;
    };

    ~RotaryEncoder();

    bool init();

    RotaryStates_t get_state();

protected:

private:

    typedef enum PinNames {
                       ROT_CLK_I=0,
                       ROT_DT_I= 1,
                       ROT_SW_I=2
                      } PinNames_t;
 
    typedef enum RotaryQuadratureStates {
                           ROT_IDLE=0,
                           ROT_CW_1= 1,
                           ROT_CW_2=2,
                           ROT_CW_3=3,
                           ROT_CW_4=4,
                           ROT_CCW_1=5,
                           ROT_CCW_2=6,
                           ROT_CCW_3=7,
                           ROT_CCW_4=8
                          } RotaryQuadratureStates_t;

    typedef enum RotarySwitchStates {
                           ROT_SW_1=0,
                           ROT_SW_2=1
                          } RotarySwitchStates_t;

    std::string m_PinNames[3];

    int  m_PinFds[3];

    RotaryQuadratureStates_t m_RotaryQuadratureState;

    RotarySwitchStates_t m_RotarySwitchState;

    RotaryStates_t eval_rotary_state(char* cur_state_c);

    RotaryStates_t eval_switch_state(char* cur_state_c);

};

