# TheBigLEDowSPI

TheBigLEDowSPI - Simple APA102/SK9822 SPI LED controller for SBC (ie: RasPi, Asus Tinker, etc)
Included in this repo, the static binary (for DietPi), so you can directly use this program on your favorite RasPi 3+...

To use this program, simply type :
pypy ./led-server.py &

led-server.py is a sample program configured for displaying a simple animation on 2000 LEDs at 30 FPS with a (very conservative) 4 MHz bus clock.
TheBigLEDowSPI can run up to 5000 LEDs per output (2 outputs on the RasPi3) if you want to keep the FPS above 20.

Then you can launch the client :
./TheBigLEDowSPI IP-of-master-server TCP-Port spidev-device &

ie: ./TheBigLEDowSPI 127.0.0.1 4200 /dev/spidev0.0 &

The server (Python script) is the code which calculates the frames to be sent to the LEDs.
It will not calculate the ideal speed in KHz based on the number of LEDs you asked for.
In order to calculate the max KHz for a given number of LEDs, use this formula :
MaxFreq = -2.95 * NbLEDs + 19200

So, for 3000 LEDs, the max speed is -2.95 * 3000 + 19200 = 10350 KHz (so let's use 10000).  
What will be the max FPS with that speed ?  
Speed in Hz / NbLEDs / 32 (bits) = 10,000,000 / 3000 / 32 = 104 FPS

******

There are 3 binary releases in this repo (statically built on DietPi) :
 - TheBigLEDowSPI : production release
 - TheBigLEDowSPI.debug : debug release, that doesn't send data to SPI
 - TheBigLEDowSPI.debug.spi : debug release, which also send the data on the SPI bus

******

Finally, in order to configure your RasPi for sending data on the SPI bus, you should check this repo :
https://github.com/philippebourcier/DietPi-scripts
... which contains setup_spi_pi.sh, where you can find all information needed to configure the SPI ports on a RasPi3 running DietPi.

******

If you want to get an idea of what can be done with this code, you can [check our corporate website](http://www.whilezero.fr/) [//]: # {:target="_blank"}.

