# TheBigLEDowSPI
TheBigLEDowSPI - Simple APA102/SK9822 SPI LED control for SBC (ie: RasPi, Asus Tinker, etc)

Included in this repo, the binary, so you can directly use this program on your favorite RasPi 3+...

To use this program, simply type :
./TheBigLEDowSPI IP-of-master-server TCP-Port spidev-device Speed-in-Mhz Number-of-LEDs

ie: ./TheBigLEDowSPI 192.168.2.1 4200 /dev/spidev0.0 10 3000

In order to calculate the max speed for a given number of LEDs, use this :
MaxFreq = -2.95 * NbLEDs + 19200

So, for 3000 LEDs, the max speed is -2.95 * 3000 + 19200 = 10.350 Mhz (so let's use 10).
What will be the max FPS with that speed ?
Speed in Hz / NbLEDs / 32 (bits) = 
10,000,000 / 3000 / 32 = 104 FPS

