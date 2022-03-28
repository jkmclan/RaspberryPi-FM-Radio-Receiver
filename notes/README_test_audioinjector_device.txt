hw:CARD=vc4hdmi,DEV=0
    vc4-hdmi, MAI PCM i2s-hifi-0
    Direct hardware device without any conversions
plughw:CARD=vc4hdmi,DEV=0
    vc4-hdmi, MAI PCM i2s-hifi-0
    Hardware device with all software conversions
default:CARD=vc4hdmi
    vc4-hdmi, MAI PCM i2s-hifi-0
    Default Audio Device
sysdefault:CARD=vc4hdmi
    vc4-hdmi, MAI PCM i2s-hifi-0
    Default Audio Device
hdmi:CARD=vc4hdmi,DEV=0
    vc4-hdmi, MAI PCM i2s-hifi-0
    HDMI Audio Output
dmix:CARD=vc4hdmi,DEV=0
    vc4-hdmi, MAI PCM i2s-hifi-0
    Direct sample mixing device
--------------------------------------------
hw:CARD=audioinjectorpi,DEV=0
    audioinjector-pi-soundcard, AudioInjector audio wm8731-hifi-0
    Direct hardware device without any conversions
plughw:CARD=audioinjectorpi,DEV=0
    audioinjector-pi-soundcard, AudioInjector audio wm8731-hifi-0
    Hardware device with all software conversions
default:CARD=audioinjectorpi
    audioinjector-pi-soundcard, AudioInjector audio wm8731-hifi-0
    Default Audio Device
sysdefault:CARD=audioinjectorpi
    audioinjector-pi-soundcard, AudioInjector audio wm8731-hifi-0
    Default Audio Device
dmix:CARD=audioinjectorpi,DEV=0
    audioinjector-pi-soundcard, AudioInjector audio wm8731-hifi-0
    Direct sample mixing device
--------------------------------------------
speaker-test -Dhw:audioinjectorpi -c2 -twav -wSineWaveMinus16.wav -W ./  // Fails due to stereo wav file
speaker-test -Dhw:audioinjectorpi -c2 -twav -wNoise.wav      // Successfully alternates mono wav file between left and right

speaker-test -Dhw:audioinjectorpi -c2 -tsine -f75   // Successfully alternates tone between left and right

aplay -Dhw:audioinjectorpi -c2  -twav /usr/share/sounds/alsa/Noise.wav   // Fails with mono wav file: Channels count non available
aplay -Dhw:audioinjectorpi -c2  -twav /home/pi/SineWaveMinus16.wav       // Apparently Failes: plays stereo wav file as mono into left and right
aplay -Dhw:audioinjectorpi -c2  -twav /home/pi/piano2.wav      // Apparently Fails: plays stereo wav file as mono into left and right
aplay -D[plug]hw:audioinjectorpi -c2  -twav /home/pi/M1F1-int16WE-AFsp.wav -Vstereo // Fails with stereo wav file: Unable to install hw params:
aplay -Ddmix:audioinjectorpi -c2  -twav /home/pi/M1F1-int32WE-AFsp.wav -Vstereo // Successfull plays stereo wav file as stereo into left and right

----------------------------------------------------------
rtl_fm -p 22 -f 90.1e6 -M fm -s 200000  -A std -r 32000 -l 0 -E deemp  - | aplay -Dplughw:audioinjectorpi  -f S16_LE -c 2 -t raw --verbose -r 16000 --mmap --buffer-size=16000  --dump-hw-params

Playing raw data 'stdin' : Signed 16 bit Little Endian, Rate 16000 Hz, Stereo
HW Params of device "plughw:audioinjectorpi":
--------------------
ACCESS:  MMAP_INTERLEAVED MMAP_NONINTERLEAVED MMAP_COMPLEX RW_INTERLEAVED RW_NONINTERLEAVED
FORMAT:  S8 U8 S16_LE S16_BE U16_LE U16_BE S24_LE S24_BE U24_LE U24_BE S32_LE S32_BE U32_LE U32_BE FLOAT_LE FLOAT_BE FLOAT64_LE FLOAT64_BE MU_LAW A_LAW IMA_ADPCM S20_LE S20_BE U20_LE U20_BE S24_3LE S24_3BE U24_3LE U24_3BE S20_3LE S20_3BE U20_3LE U20_3BE S18_3LE S18_3BE U18_3LE U18_3BE
SUBFORMAT:  STD
SAMPLE_BITS: [4 64]
FRAME_BITS: [4 640000]
CHANNELS: [1 10000]
RATE: [4000 4294967295)
PERIOD_TIME: (333 8192000]
PERIOD_SIZE: (1 4294967295)
PERIOD_BYTES: (0 4294967295)
PERIODS: (0 4294967294)
BUFFER_TIME: [1 4294967295]
BUFFER_SIZE: [2 4294967294]
BUFFER_BYTES: [1 4294967295]
TICK_TIME: ALL
--------------------
Found 1 device(s):
Plug PCM: Rate conversion PCM (32000, sformat=S16_LE)
Converter: linear-interpolation
Protocol version: 10002
Its setup is:
  stream       : PLAYBACK
  access       : MMAP_INTERLEAVED
  format       : S16_LE
  subformat    : STD
  channels     : 2
  rate         : 16000
  exact rate   : 16000 (16000/1)
  msbits       : 16
  buffer_size  : 16000
  period_size  : 4000
  period_time  : 250000
  tstamp_mode  : NONE
  tstamp_type  : MONOTONIC
  period_step  : 1
  avail_min    : 4000
  period_event : 0
  start_threshold  : 16000
  stop_threshold   : 16000
  silence_threshold: 0
  silence_size : 0
  boundary     : 1048576000
Slave: Hardware PCM card 1 'audioinjector-pi-soundcard' device 0 subdevice 0
Its setup is:
  stream       : PLAYBACK
  access       : MMAP_INTERLEAVED
  format       : S16_LE
  subformat    : STD
  channels     : 2
  rate         : 32000
  exact rate   : 32000 (32000/1)
  msbits       : 16
  buffer_size  : 32000
  period_size  : 8000
  period_time  : 250000
  tstamp_mode  : NONE
  tstamp_type  : MONOTONIC
  period_step  : 1
  avail_min    : 8000
  period_event : 0
  start_threshold  : 32000
  stop_threshold   : 32000
  silence_threshold: 0
  silence_size : 0
  boundary     : 2097152000
  appl_ptr     : 0
  hw_ptr       : 0
mmap_area[0] = 0x133d38,0,32 (16)
mmap_area[1] = 0x133d38,16,32 (16)
  0:  Generic, RTL2832U, SN: 77771111153705700

Using device 0: Generic RTL2832U
Found Elonics E4000 tuner
Tuner gain set to automatic.
Tuner error set to 22 ppm.
Tuned to 90400000 Hz.
Oversampling input by: 6x.
Oversampling output by: 1x.
Buffer size: 6.83ms
Allocating 15 zero-copy buffers
Sampling at 1200000 S/s.
Output at 200000 Hz.

