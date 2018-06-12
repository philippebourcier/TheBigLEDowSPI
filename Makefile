all:
	                g++ -Wall -DHAS_SPI -O3 -static -o TheBigLEDowSPI TheBigLEDowSPI.cpp
debug:
	                g++ -Wall -DDEBUG -O3 -static -o TheBigLEDowSPI.debug TheBigLEDowSPI.cpp
	                g++ -Wall -DDEBUG -DHAS_SPI -O3 -static -o TheBigLEDowSPI.debug.spi TheBigLEDowSPI.cpp
