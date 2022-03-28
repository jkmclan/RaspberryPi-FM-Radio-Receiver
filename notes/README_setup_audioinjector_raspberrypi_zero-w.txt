I have managed to get this working on an RPi 4 running Raspbian Buster. Hopefully this will help someone else.

Don't download the dpkg and run the auto setup. These steps are to get it running manually!

1) Edit /boot/config.txt

Code: Select all

sudo nano /boot/config.txt

2) Uncomment these lines (remove the #):

Code: Select all

dtparam=i2c_arm=on
dtparam=i2s=on
dtparam=spi=on

3) Comment out this line (add a # at the start):

Code: Select all

#dtparam=audio=on

4) Add the following line just after the one you commented out in the previous step:

Code: Select all

dtoverlay=audioinjector-wm8731-audio

5) Quit and save (ctrl-x, y, enter)

6) Reboot the pi:

Code: Select all

sudo reboot

7) Run alsa mixer:

Code: Select all

alsamixer

8) Scroll across to the one titled 'Output Mixer HiFi' and press 'm' to unmute it. It should now say '00' above it with a green background. Press 'esc' to save and exit.

That should be everything. You can now test that it works with:

Code: Select all

aplay test.wav

Note you will need your own test wav file, you can Google for one and use wget to download it directly to the Pi.
