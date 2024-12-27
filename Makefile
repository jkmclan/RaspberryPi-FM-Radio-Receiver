# -*- MakeFile -*-
# dont forget to use tabs, not spaces for indents
# to use simple copy this file in the same directory and type 'make'

./build/a.out: ./build/RadioControlMain.o \
               ./build/RotaryEncoderEvent.o \
               ./build/OledI2cSH1106.o \
               ./build/rtl_fm_lib.o \
               ./build/rtl_convenience.o
	g++ -o ./build/a.out \
               ./build/RadioControlMain.o \
               ./build/RotaryEncoderEvent.o \
               ./build/OledI2cSH1106.o \
               ./build/rtl_fm_lib.o \
               ./build/rtl_convenience.o \
               -lrtlsdr \
               -L /opt/lcdgfx/bld/ -llcdgfx \
               -lgpiod \
               -lasound \
               -lkissfft-int16_t \
               -lpthread \
               -lm

# 
# g++ -std=c++11 -g -Os -w -ffreestanding -MD -g -Os -w -ffreestanding 
# -I./demos/sh1106_demo -I../src -Wall -Werror -ffunction-sections -fdata-sections  
# -Wl,--gc-sections -fno-rtti -x c++ 
# -c demos/sh1106_demo/sh1106_demo.ino -o ../bld/demos/sh1106_demo/sh1106_demo.o

./build/%.o: ./src/%.cc ./src/%.hh
	@mkdir -p ./build
	g++ -c $< -o $@ -I /opt/lcdgfx/src/

./build/%.o: ./src/%.c ./src/%.h
	@mkdir -p ./build
	g++ -c $< -o $@

#foo.o: foo.c
#	gcc -c -o foo.o foo.c

