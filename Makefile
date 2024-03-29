# -*- MakeFile -*-
# dont forget to use tabs, not spaces for indents
# to use simple copy this file in the same directory and type 'make'

./build/a.out: ./build/RadioControlMain.o \
               ./build/RotaryEncoderEvent.o \
               ./build/LcdI2cHD44780.o \
               ./build/rtl_fm_lib.o \
               ./build/rtl_convenience.o
	g++ -o ./build/a.out \
               ./build/RadioControlMain.o \
               ./build/RotaryEncoderEvent.o \
               ./build/LcdI2cHD44780.o \
               ./build/rtl_fm_lib.o \
               ./build/rtl_convenience.o \
               -lrtlsdr \
               -lwiringPi \
               -lgpiod \
               -lasound \
               -lkissfft-int16_t \
               -lpthread \
               -lm

./build/%.o: ./src/%.cc ./src/%.hh
	@mkdir -p ./build
	g++ -c $< -o $@

./build/%.o: ./src/%.c ./src/%.h
	@mkdir -p ./build
	g++ -c $< -o $@

#foo.o: foo.c
#	gcc -c -o foo.o foo.c

