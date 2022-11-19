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
#include <string>

#include "RotaryEncoder.hh"

bool RotaryEncoder::init() {

    char err_msg[128];
    std::string m_PinNames[3];

    // Initialize the array of pin numbers

    m_PinNames[ROT_CLK_I] = ROT_CLK_S;
    m_PinNames[ROT_DT_I]  = ROT_DT_S;
    m_PinNames[ROT_SW_I]  = ROT_SW_S;

    int fd = -1;

    // Unexport the pin by writing to /sys/class/gpio/unexport

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1) {
        perror("Error opening /sys/class/gpio/unexport");
        return false;
    }

    for (int ii=ROT_CLK_I; ii<=ROT_SW_I; ++ii) {
        if (write(fd, m_PinNames[ii].c_str(), m_PinNames[ii].size()) != m_PinNames[ii].size()) {
            perror("Error writing to /sys/class/gpio/unexport");
            // return false;  Try not returning if the initial failure occurs after a fresh boot up
        }
    }

    close(fd);

    // Export the desired pin by writing to /sys/class/gpio/export

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1) {
        perror("Unable to open /sys/class/gpio/export");
        return false;
    }

    for (int ii=ROT_CLK_I; ii<=ROT_SW_I; ++ii) {
        if (write(fd, m_PinNames[ii].c_str(), m_PinNames[ii].size()) != m_PinNames[ii].size()) {
            perror("Error writing to /sys/class/gpio/export");
            return false;
        }
    }

    close(fd);

    // For each pin

    for (int ii=ROT_CLK_I; ii<=ROT_SW_I; ++ii) {

        // Set the pin to be an input by writing "in" to /sys/class/gpio/gpio<X>/direction

        std::string gpio_dir = std::string("/sys/class/gpio/gpio") + m_PinNames[ii] + std::string("/direction");
        fd = open(gpio_dir.c_str(), O_WRONLY);
        if (fd == -1) {
            sprintf(err_msg, "Unable to open %s", gpio_dir);
            perror(err_msg);
            return false;
        }

        std::string dir = "in";
        if (write(fd, dir.c_str(), dir.size()) != dir.size()) {
            sprintf(err_msg, "Error writing %s to %s", dir.c_str(), gpio_dir);
            perror(err_msg);
            return false;
        }

        close(fd);

        // Open the pin for reading 

        std::string gpio_val = std::string("/sys/class/gpio/gpio") + m_PinNames[ii] + std::string("/value");
        m_PinFds[ii] = open(gpio_val.c_str(), O_RDONLY);
        if (fd == -1) {
            sprintf(err_msg, "Unable to open %s", gpio_val.c_str());
            perror(err_msg);
            return false;
        } else {
            printf("Opened fd=%d, %s\n", m_PinFds[ii], gpio_val.c_str());
        }

    } // End For

    return true;
}

RotaryEncoder::RotaryStates_t RotaryEncoder::get_state() {

    char err_msg[128];
    char cur_state_CLK_c;
    char cur_state_DT_c;
    char cur_state_SW_c;

    lseek(m_PinFds[ROT_CLK_I], 0, SEEK_SET);
    lseek(m_PinFds[ROT_DT_I], 0, SEEK_SET);
    lseek(m_PinFds[ROT_SW_I], 0, SEEK_SET);

    if (read(m_PinFds[ROT_CLK_I], &cur_state_CLK_c, 1) != 1) {
        sprintf(err_msg, "Error reading from fd=%d", m_PinFds[ROT_CLK_I]);
        perror(err_msg);
        return ROT_NC;
    }

    if (read(m_PinFds[ROT_DT_I], &cur_state_DT_c, 1) != 1) {
        sprintf(err_msg, "Error reading from fd=%d", m_PinFds[ROT_DT_I]);
        perror(err_msg);
        return ROT_NC;
    }

    //printf("CLK = %c\n", cur_state_CLK_c);
    //printf("DT  = %c\n", cur_state_DT_c);

    char cur_state_c[2];
    strncpy(&cur_state_c[0], &cur_state_CLK_c, 1);
    strncpy(&cur_state_c[1], &cur_state_DT_c, 1);

    RotaryStates_t rotary_state = eval_rotary_state(cur_state_c);
    if (rotary_state != ROT_NC) {
        return rotary_state;
    }

    if (read(m_PinFds[ROT_SW_I], &cur_state_SW_c, 1) != 1) {
        sprintf(err_msg, "Error reading from fd=%d", m_PinFds[ROT_SW_I]);
        perror(err_msg);
        return ROT_NC;
    } 

    RotaryStates_t switch_state = eval_switch_state(&cur_state_SW_c);
    if (switch_state != ROT_NC) {
        return switch_state;
    }

    return ROT_NC;
}

RotaryEncoder::~RotaryEncoder() {

    for (int ii=0; ii<3; ++ii) {
        close(m_PinFds[ii]);
    }

    // Unexport the pin by writing to /sys/class/gpio/unexport

    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1) {
        perror("Error opening /sys/class/gpio/unexport");
        return;
    }

    for (int ii=0; ii<3; ++ii) {
        if (write(fd, m_PinNames[ii].c_str(), m_PinNames[ii].size()) != m_PinNames[ii].size()) {
            perror("Error writing to /sys/class/gpio/unexport");
            return;
        }
    }

    close(fd);

    return;
}

RotaryEncoder::RotaryStates_t RotaryEncoder::eval_rotary_state(char* cur_state_c) {

    RotaryStates_t state = ROT_NC;

    // The Rotary pins are Active-Low

    switch(m_RotaryQuadratureState) {
    case ROT_IDLE:
    {
        if (strncmp(cur_state_c, "01", 2) == 0) {
            m_RotaryQuadratureState = ROT_CW_1;
        }
        if (strncmp(cur_state_c, "10", 2) == 0) {
            m_RotaryQuadratureState = ROT_CCW_1;
        }
        break;
    }
    case ROT_CW_1:
    {
        if (strncmp(cur_state_c, "00", 2) == 0) {
            m_RotaryQuadratureState = ROT_CW_2;
        }
        break;
    }
    case ROT_CW_2:
    {
        if (strncmp(cur_state_c, "10", 2) == 0) {
            m_RotaryQuadratureState = ROT_CW_3;
        }
        break;
    }
    case ROT_CW_3:
    {
        if (strncmp(cur_state_c, "11", 2) == 0) {
            m_RotaryQuadratureState = ROT_IDLE;
            state = ROT_INCREMENT;
        }
        break;
    }
    case ROT_CCW_1:
    {
        if (strncmp(cur_state_c, "00", 2) == 0) {
            m_RotaryQuadratureState = ROT_CCW_2;
        }
        break;
    }
    case ROT_CCW_2:
    {
        if (strncmp(cur_state_c, "01", 2) == 0) {
            m_RotaryQuadratureState = ROT_CCW_3;
        }
        break;
    }
    case ROT_CCW_3:
    {
        if (strncmp(cur_state_c, "11", 2) == 0) {
            m_RotaryQuadratureState = ROT_IDLE;
            state = ROT_DECREMENT;
        }
        break;
    }
    default:
    {
        printf("Invalid rotary state: %d\n", m_RotaryQuadratureState);
        m_RotaryQuadratureState = ROT_IDLE;
    }
    }

    return state;
}

RotaryEncoder::RotaryStates_t RotaryEncoder::eval_switch_state(char* cur_state_c) {

    RotaryStates_t state = ROT_NC;

    // The Rotary switch pin is Active-Low

    switch (m_RotarySwitchState) {
    case ROT_SW_1: {
        if (strncmp(cur_state_c, "0", 1) == 0) {
            m_RotarySwitchState = ROT_SW_2;
        }
        break;
    }
    case ROT_SW_2: {
        if (strncmp(cur_state_c, "1", 1) == 0) {
            m_RotarySwitchState = ROT_SW_1;
            state = ROT_SW_PUSHED;
        }
        break;
    }
    default:
    {
        printf("Invalid rotary switch state: %d\n", m_RotarySwitchState);
        m_RotaryQuadratureState = ROT_IDLE;
    }
    }

    return state;
}
