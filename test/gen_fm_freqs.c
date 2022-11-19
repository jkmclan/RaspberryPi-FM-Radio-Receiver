/*

*/

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

int main()
{

    int  fd;

    int ii = 1;
    for (double freq=87.5; freq<108.0; freq += 0.200) {

        printf("Center Freq: Index=%d, (MHz)=%f\n", ii, freq);
        ii++;
    }

}

