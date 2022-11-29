

$ cc -c -g convenience.c 
$ cc -c -g rtl_fm.c
$ cc -g rtl_fm.c ./convenience.o  -lrtlsdr -lpthread -lm

==============
$ g++ -c -g convenience.c
$ g++ -c -g rtl_fm.c
$ g++ -g rtl_fm.c ./convenience.o -lrtlsdr -lpthread -lm

==============
$ g++ -c -g rtl_convenience.c
$ g++ -c -g rtl_fm_lib.c
$ g++ -c -g RadioControlMain.cc
$ g++ -g RadioControlMain.cc ./rtl_fm_lib.o ./rtl_convenience.o ../build/RotaryEncoder.o ../build/LcdI2cHD44780.o -lrtlsdr -lwiringPi -lpthread -lm
$ sudo ./a.out -p 22 -f 90.1e6 -M fm -s 200000  -A std -r 32000 -l 0 -E deemp - > /dev/null
$ sudo ./a.out -p 22 -f 90.1e6 -M fm -s 200000  -A std -r 32000 -l 0 -E deemp - | aplay -Dplughw:audioinjectorpi  -f S16_LE -c 2 -t raw --verbose -r 16000 --mmap --buffer-size=16000  --dump-hw-params
$ sudo 
