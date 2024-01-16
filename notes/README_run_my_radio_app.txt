
./build/a.out -p 22 -M fm -s 200000  -A std -r 32000 -l 0 -E deemp  /dev/null

./build/a.out -p 22 -M fm -s 200000  -A std -r 32000 -l 0 -E deemp -  | aplay -Dplughw:audioinjectorpi  -f S16_LE -c 2 -t raw --verbose -r 16000 --mmap --buffer-size=16000  --dump-hw-params
