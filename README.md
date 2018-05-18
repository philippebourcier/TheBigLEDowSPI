# TheBigLEDowSPI

TheBigLEDowSPI - Simple APA102/SK9822 SPI LED controller for SBC (ie: RasPi, Asus Tinker, etc)
Included in this repo, the static binary (for DietPi), so you can directly use this program on your favorite RasPi 3+...

To use this program, simply type :
pypy ./led-server.py

led-server.py is a sample program configured for 3000 LEDs per default.
It can run up to 5000 LEDs per output (2 outputs on the RasPi3) if you want to keep the FPS of above 20.

Then launch the client :
./TheBigLEDowSPI IP-of-master-server TCP-Port spidev-device

ie: ./TheBigLEDowSPI 127.0.0.1 4200 /dev/spidev0.0

The server (Python script) is the code which calculates the frames to be sent to the LEDs.
It will also calculate the ideal speed in KHz based on the number of LEDs you asked for.

A RasPi has 2 SPI outputs, so you can handle 10000 LEDs at 30 FPS with one client running on each output (spidev0.0 and spidev1.0).

Interesting details regarding maximum speed...

In order to calculate the max speed for a given number of LEDs, use this :
MaxFreq = -2.95 * NbLEDs + 19200

So, for 3000 LEDs, the max speed is -2.95 * 3000 + 19200 = 10.350 Mhz (so let's use 10).
What will be the max FPS with that speed ?
Speed in Hz / NbLEDs / 32 (bits) = 
10,000,000 / 3000 / 32 = 104 FPS

