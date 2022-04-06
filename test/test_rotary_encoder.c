/*

Read the Clock-Wise and Counter-Clock-Wise state 
changes of an incremental rotary encoder using 
the sysfs interface on a Raspberry Pi.


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


typedef enum RotaryStates {ROT_IDLE=0,
                       ROT_CW_1= 1,
                       ROT_CW_2=2,
                       ROT_CW_3=3,
                       ROT_CW_4=4,
                       ROT_CCW_1=5,
                       ROT_CCW_2=6,
                       ROT_CCW_3=7,
                       ROT_CCW_4=8
                      } RotaryStates_t;

static RotaryStates_t RotaryState_s = ROT_IDLE;

int eval_rotary_state(char* cur_state_c);

int main()
{
    enum PinNames {ROT_CLK_I=0, ROT_DT_I= 1, ROT_SW_I=2};

    int  fd;
    char err_msg[128];
    char pin_array[3][3];
    int  pin_fds[3];

    int counter = 0;
    int cur_state_SW;
    char enc_dir[4];

    // Initialize the array of pin numbers

    strcpy(pin_array[ROT_CLK_I], ROT_CLK_S);
    strcpy(pin_array[ROT_DT_I], ROT_DT_S);
    strcpy(pin_array[ROT_SW_I], ROT_SW_S);

    // Unexport the pin by writing to /sys/class/gpio/unexport

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1) {
        perror("Error opening /sys/class/gpio/unexport");
    }

    for (int ii=ROT_CLK_I; ii<=ROT_SW_I; ++ii) {
        if (write(fd, pin_array[ii], strlen(pin_array[ii])) != strlen(pin_array[ii])) {
            perror("Error writing to /sys/class/gpio/unexport");
        }
    }

    close(fd);

    // Export the desired pin by writing to /sys/class/gpio/export

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1) {
        perror("Unable to open /sys/class/gpio/export");
        exit(1);
    }

    for (int ii=ROT_CLK_I; ii<=ROT_SW_I; ++ii) {
        if (write(fd, pin_array[ii], strlen(pin_array[ii])) != strlen(pin_array[ii])) {
            perror("Error writing to /sys/class/gpio/export");
            exit(1);
        }
    }

    close(fd);

    // For each pin

    for (int ii=ROT_CLK_I; ii<=ROT_SW_I; ++ii) {

        // Set the pin to be an input by writing "in" to /sys/class/gpio/gpio<X>/direction

        char*  gpio_dir = malloc(strlen("/sys/class/gpio/gpio") + strlen(pin_array[ii]) + strlen("/direction") + 1);
        gpio_dir = strcpy(gpio_dir, "/sys/class/gpio/gpio");
        gpio_dir = strcat(gpio_dir, pin_array[ii]);
        gpio_dir = strcat(gpio_dir, "/direction");
        fd = open(gpio_dir, O_WRONLY);
        if (fd == -1) {
            sprintf(err_msg, "Unable to open %s", gpio_dir);
            perror(err_msg);
            exit(1);
        }

        char* dir = "in";
        if (write(fd, dir, strlen(dir)) != strlen(dir)) {
            perror(strcat("Error writing to ", gpio_dir));
            exit(1);
        }

        free(gpio_dir);
        close(fd);

        // Open the pin for reading 

        char*  gpio_val = malloc(strlen("/sys/class/gpio/gpio") + strlen(pin_array[ii]) + strlen("/value") + 1);
        gpio_val = strcpy(gpio_val, "/sys/class/gpio/gpio");
        gpio_val = strcat(gpio_val, pin_array[ii]);
        gpio_val = strcat(gpio_val, "/value");
        pin_fds[ii] = open(gpio_val, O_RDONLY);
        if (fd == -1) {
            sprintf(err_msg, "Unable to open %s", gpio_val);
            perror(err_msg);
            exit(1);
        } else {
            printf("Opened fd=%d, %s\n", pin_fds[ii], gpio_val);
        }

        free(gpio_val);

    } // End For

    char cur_state_CLK_c;
    char cur_state_DT_c;
    char cur_state_SW_c;

    // Do forever

    int loop_cnt = 1;
    while(loop_cnt < 1000000) {

        lseek(pin_fds[ROT_CLK_I], 0, SEEK_SET);
        lseek(pin_fds[ROT_DT_I], 0, SEEK_SET);
        lseek(pin_fds[ROT_SW_I], 0, SEEK_SET);

        if (read(pin_fds[ROT_CLK_I], &cur_state_CLK_c, 1) != 1) {
            sprintf(err_msg, "Error reading from fd=%d", pin_fds[ROT_CLK_I]);
            perror(err_msg);
            exit(1);
        }

        if (read(pin_fds[ROT_DT_I], &cur_state_DT_c, 1) != 1) {
            sprintf(err_msg, "Error reading from fd=%d", pin_fds[ROT_DT_I]);
            perror(err_msg);
            exit(1);
        }

        //printf("CLK = %c\n", cur_state_CLK_c);
        //printf("DT  = %c\n", cur_state_DT_c);

        char cur_state_c[2];
        strncpy(&cur_state_c[0], &cur_state_CLK_c, 1);
        strncpy(&cur_state_c[1], &cur_state_DT_c, 1);

        int incr = eval_rotary_state(cur_state_c);
        counter += incr;
        if (incr > 0) {
            strcpy(enc_dir, "CW");
            printf("%s : %d\n", enc_dir, counter);
        } else if (incr < 0) {
            strcpy(enc_dir, "CCW");
            printf("%s : %d\n", enc_dir, counter);
        }

        if (read(pin_fds[ROT_SW_I], &cur_state_SW_c, 1) != 1) {
            sprintf(err_msg, "Error reading from fd=%d", pin_fds[ROT_SW_I]);
            perror(err_msg);
            exit(1);
        } 
        cur_state_SW = cur_state_SW_c & 0x0F;

        // The SW pin is Active-Low
        if (cur_state_SW == 0) printf("SW: ON\n");

        loop_cnt++;

        // Sleep
        usleep(1000); // 10ms is less than 3% CPU

    } // End While

    for (int ii=0; ii<3; ++ii) {
        close(pin_fds[ii]);
    }

    // Unexport the pin by writing to /sys/class/gpio/unexport

    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1) {
        perror("Error opening /sys/class/gpio/unexport");
    }

    for (int ii=0; ii<3; ++ii) {
        if (write(fd, pin_array[ii], strlen(pin_array[ii])) != strlen(pin_array[ii])) {
            perror("Error writing to /sys/class/gpio/unexport");
        }
    }

    close(fd);

    // And exit
    return 0;
}

int eval_rotary_state(char* cur_state_c) {

    int incr = 0;

    switch(RotaryState_s) {
    case ROT_IDLE:
    {
        if (strncmp(cur_state_c, "01", 2) == 0) {
            RotaryState_s = ROT_CW_1;
        }
        if (strncmp(cur_state_c, "10", 2) == 0) {
            RotaryState_s = ROT_CCW_1;
        }
        break;
    }
    case ROT_CW_1:
    {
        if (strncmp(cur_state_c, "00", 2) == 0) {
            RotaryState_s = ROT_CW_2;
        }
        break;
    }
    case ROT_CW_2:
    {
        if (strncmp(cur_state_c, "10", 2) == 0) {
            RotaryState_s = ROT_CW_3;
        }
        break;
    }
    case ROT_CW_3:
    {
        if (strncmp(cur_state_c, "11", 2) == 0) {
            RotaryState_s = ROT_IDLE;
            incr = 1;
        }
        break;
    }
    case ROT_CCW_1:
    {
        if (strncmp(cur_state_c, "00", 2) == 0) {
            RotaryState_s = ROT_CCW_2;
        }
        break;
    }
    case ROT_CCW_2:
    {
        if (strncmp(cur_state_c, "01", 2) == 0) {
            RotaryState_s = ROT_CCW_3;
        }
        break;
    }
    case ROT_CCW_3:
    {
        if (strncmp(cur_state_c, "11", 2) == 0) {
            RotaryState_s = ROT_IDLE;
            incr = -1;
        }
        break;
    }
    default:
    {
        printf("Invalid rotary state: %d\n", RotaryState_s);
        RotaryState_s = ROT_IDLE;
    }
    }

    return incr;

}
