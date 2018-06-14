all:
	                g++ -std=c++17 -Wall -DHAS_SPI -O3 -static -o TheBigLEDowSPI TheBigLEDowSPI.cpp -lstdc++fs
debug:
	                g++ -std=c++17 -Wall -DDEBUG -O3 -static -o TheBigLEDowSPI.debug TheBigLEDowSPI.cpp -lstdc++fs
	                g++ -std=c++17 -Wall -DDEBUG -DHAS_SPI -O3 -static -o TheBigLEDowSPI.debug.spi TheBigLEDowSPI.cpp -lstdc++fs
