#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <inttypes.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/if.h>

#include <fcntl.h>		// Needed for SPI port
#include <linux/spi/spidev.h>	// Needed for SPI port

using namespace std;

//-----------------------------------------------------------------------------
// tcp2apa102spi.cpp V2.3
//-----------------------------------------------------------------------------
#define VERSION "V2.3"

// ID Utilities ---------------------------------------------------------------
const char* getMacAddress()
{
    static char str[13];
    struct      ifreq s;
    int         fs = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    strcpy(s.ifr_name, "eth0");

    if (0 == ioctl(fs, SIOCGIFHWADDR, &s))
    {
        snprintf(
            str, sizeof(str),
            "%02x%02x%02x%02x%02x%02x",
            s.ifr_addr.sa_data[0], s.ifr_addr.sa_data[1],
            s.ifr_addr.sa_data[2], s.ifr_addr.sa_data[3],
            s.ifr_addr.sa_data[4], s.ifr_addr.sa_data[5]
        );
    }

    return str;
}

// SPI Utilities --------------------------------------------------------------
int spiOpen( string device, uint32_t speed )
{
    int 	status 		= -1;
    //----- SET SPI MODE -----
    //SPI_MODE_0 (0,0) 	CPOL = 0, CPHA = 0, Clock _idle low, data is clocked in on rising edge, output data (change) on falling edge
    //SPI_MODE_1 (0,1) 	CPOL = 0, CPHA = 1, Clock _idle low, data is clocked in on falling edge, output data (change) on rising edge
    //SPI_MODE_2 (1,0) 	CPOL = 1, CPHA = 0, Clock _idle high, data is clocked in on falling edge, output data (change) on rising edge
    //SPI_MODE_3 (1,1) 	CPOL = 1, CPHA = 1, Clock _idle high, data is clocked in on rising, edge output data (change) on falling edge
    uint8_t 	mode 		= SPI_MODE_0;
    uint8_t	bitsPerWord	= 8;
    int 	fd 		= open(device.c_str(), O_RDWR);

    if (fd < 0)
    {
        cerr << "<spiOpen> ERROR : Could not open SPI device." << endl;
        exit(1);
    }

    status = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if(status < 0)
    {
        cerr << "<spiOpen> ERROR : Could not set SPIMode (WR)...ioctl fail" << endl;
        exit(1);
    }

    status = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bitsPerWord);
    if(status < 0)
    {
        cerr << "<spiOpen> ERROR : Could not set SPI bitsPerWord (WR)...ioctl fail" << endl;
        exit(1);
    }

    status = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if(status < 0)
    {
        cerr << "<spiOpen> ERROR : Could not set SPI speed (WR)...ioctl fail" << endl;
        exit(1);
    }

    return( fd );
}

int spiWrite( int fd, uint32_t speed, uint8_t *data, int size )
{
    struct spi_ioc_transfer tr;

    memset(&tr, 0, sizeof tr);

    tr.tx_buf 			= (unsigned long) data;
    tr.rx_buf 			= (unsigned long) NULL;
    tr.len 			= (unsigned long) size;
    tr.speed_hz 		= speed;
    tr.bits_per_word		= 8;

    try
    {
        ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    }
    catch (const std::exception& e)
    {
        cerr << "<spiWrite> ERROR : " << e.what() << endl;
    }

    return 0;
}

// TCP Utilities --------------------------------------------------------------
int tcpConnect(string address, int port)
{
    struct sockaddr_in server;
    int reuse 	= 1;
    int sock 	= socket(AF_INET, SOCK_STREAM, 0);

    if (sock == -1)
    {
        cerr << "tcpConnect> ERROR : Could not create socket" << endl;
        return -1;
    }
    else
        cerr << "tcpConnect> Socket created"  << endl;

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        cerr << "Could not set SO_REUSEADDR" << endl;
        return -1;
    }

#ifdef SO_REUSEPORT
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
    {
        cerr << "Could not set SO_REUSEPORT" << endl;
        return -1;
    }
#endif

    if(inet_addr(address.c_str()) < 0)
    {
        struct hostent *he;
        struct in_addr **addr_list;

        if((he = gethostbyname(address.c_str())) == NULL)
        {
            cerr << "tcpConnect> ERROR Gethostbyname : Failed to resolve hostname" << endl;
            return -1;
        }

        addr_list = (struct in_addr **) he->h_addr_list;

        for(int i = 0; addr_list[i] != NULL; i++)
        {
            server.sin_addr = *addr_list[i];

            cerr << address << "tcpConnect> resolved to " << inet_ntoa(*addr_list[i]) << endl;

            break;
        }
    }
    else
    {
        server.sin_addr.s_addr = inet_addr( address.c_str() );
    }

    server.sin_family 	= AF_INET;
    server.sin_port 	= htons( port );

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        cerr << "tcpConnect> ERROR : connect failed." << endl;
        return -1;
    }

    cerr << "tcpConnect> OK: Connected." << endl;
    return sock;
}

// CLIENT ---------------------------------------------------------------------
void send2spi( int fd, uint32_t freq, uint8_t brightness, char *data, int size, bool endFrame )
{
    uint8_t	light 	= 224 + (uint8_t) (brightness * 31.0 / 100.0);
    uint32_t  	lgB 	= (uint32_t) (size * 4 / 3) + 4;
    lgB += (endFrame) ? ((uint32_t)((size/3)-1)/16) : 0;

#ifdef HAS_SPI
    uint32_t 	i = 0;
    uint8_t 	buffer[lgB];

    for(; i<4; i++)
    {
        buffer[i] 	= (uint8_t) 0;
    }

    for(int j=0; j<size; i+=4, j+=3)
    {
        buffer[i]   = (uint8_t) light;
        buffer[i+1] = (uint8_t) data[j];
        buffer[i+2] = (uint8_t) data[j+1];
        buffer[i+3] = (uint8_t) data[j+2];
    }

    for(; i<lgB; i++)
    {
        buffer[i] 	= (uint8_t) 0;
    }

    spiWrite( fd, freq, buffer, lgB );
#endif

#ifdef DEBUG
    cerr << "<send2spi> data size = " << size << ", spi size = " << lgB << ", light = " << (int)light << endl;
#endif

}

bool isNewFrame( char *header, int lgH, char *buffer, int lgB )
{
    if(lgH > lgB)
        return false;

    for(int i=0; i<lgH; i++)
        if(header[i] != buffer[i])
            return false;

    return true;
}

void tcpClient( int sockFd, int spiFd, uint32_t spiFreq, uint32_t maxSize, uint8_t brightness, char *header, int headerSize, bool endFrame )
{
    unsigned int	bufferSize 		= 0;
    unsigned int	dataSize 		= maxSize-headerSize;
    unsigned int 	size2receive	= maxSize;
    int 			dataLength 		= 0;

    char			buffer[maxSize];
    char 			data[maxSize];

    while(1)
    {
        try
        {
            bufferSize = recv( sockFd, buffer, size2receive, 0 );

            if(bufferSize <= 0)
            {
                cerr << "<tcpClient> ERROR : receive failed." << endl;
                break;
            }
            else
            {
                if( isNewFrame(header, headerSize, buffer, bufferSize) )
                {
                    if(bufferSize == maxSize)
                    {
                        if(dataLength != 0)
                        {
#ifdef DEBUG
                            cerr << "<tcpClient> ERROR (buffer == max) : previous frame of " << dataLength << endl;
#endif
                        }
                        else
                        {
                            memcpy( data, &buffer[headerSize], dataSize );	// copy buffer -> data minus header
                            send2spi( spiFd, spiFreq, brightness, data, dataSize, endFrame );
#ifdef DEBUG
                            cerr << "<tcpClient> OK : header frame received in one piece : " << dataSize << " bytes." << endl;
#endif
                        }

                        size2receive 	= maxSize;
                        dataLength	= 0;
                    }
                    else
                    {
                        if(bufferSize < maxSize)
                        {
                            size2receive = maxSize 		- bufferSize;
                            dataLength	 = bufferSize 	- headerSize;
                            memcpy( data, &buffer[headerSize], dataLength ); // copy buffer -> data minus header
#ifdef DEBUG
                            cerr << "<tcpClient> partial header frame received : " << bufferSize << "waiting for " << size2receive << " bytes." << endl;
#endif
                        }
                        else
                        {
                            size2receive = maxSize;
                            dataLength	 = 0;
                            memset( data, 0, dataSize );			// cleanup data
#ifdef DEBUG
                            cerr << "<tcpClient> ERROR : header frame too long : " << bufferSize << endl;
#endif
                        }
                    }
                }
                else
                {
                    size2receive 	-= bufferSize;

                    if(size2receive == 0)
                    {
                        if((dataLength + bufferSize) != dataSize)
                        {
#ifdef DEBUG
                            cerr << "<tcpClient> ERROR (size2 == 0) : frame of " << (dataLength + bufferSize) << endl;
#endif
                        }
                        else
                        {
                            memcpy( &data[dataLength], buffer, bufferSize ); // add buffer to data
                            send2spi( spiFd, spiFreq, brightness, data, dataSize, endFrame );
#ifdef DEBUG
                            cerr << "<tcpClient> OK : frame completed from several parts, total : " << dataSize << " bytes." << endl;
#endif
                        }

                        size2receive 	= maxSize;
                        dataLength	= 0;
                    }
                    else
                    {
                        if(size2receive > 0)
                        {
                            if(dataLength == 0)
                            {
                                memset( data, 0, dataSize );		// cleanup data
                                size2receive 	= maxSize;
                                dataLength	= 0;
#ifdef DEBUG
                                cerr << "<tcpClient> ERROR : partial frame without Header : " << bufferSize << " bytes." << endl;
#endif
                            }
                            else
                            {
                                memcpy( &data[dataLength], buffer, bufferSize );	// add buffer to data
                                dataLength		+= bufferSize;
#ifdef DEBUG
                                cerr << "<tcpClient> frame part : " << bufferSize << " bytes received, waiting : " << size2receive << " bytes." << endl;
#endif
                            }
                        }
                        else
                        {
                            memset( data, 0, dataSize );			// cleanup data
                            size2receive 	= maxSize;
                            dataLength		= 0;
#ifdef DEBUG
                            cerr << "<tcpClient> ERROR : frame part too long : " << bufferSize << " bytes." << endl;
#endif
                        }
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            cerr << "<tcpClient> ERROR : " << e.what() << endl;
            break;
        }
    }
}

// MAIN -----------------------------------------------------------------------
int main(int argc, char *argv[])
{
    char		_id[20];
    string 		_host		= "127.0.0.1";
    int 		_port		= 4200;
    string 		_spiDevice	= "";

    char		_header[]	= { 65, 80, 65, 49, 48, 50, 95 };	// "APA102_"
    int			_headerSize	= sizeof(_header) / sizeof(char);

    char 		_initBuf[80];

    // Parameters -----------------------------------------
    int 		_delay 		= 6;
    bool		_endFrame	= true;

#ifdef HAS_SPI
    sprintf( _id, "id_%s_", getMacAddress() );
#else
    sprintf( _id, "id_1234_" );
#endif

    // Arguments ------------------------------------------
    if ( argc == 1 )
    {
	cout << "id : " << _id << "0" << endl;
        exit(0);
    }
    else if ( argc == 4 )
    {
        _host 		= string( 	argv[1] );
        _port 		= atoi(		argv[2] );
        _spiDevice 	= string( 	argv[3] );	// /dev/spidev0.0 /dev/spidev1.0
#ifdef HAS_SPI
	string idtmp	= string(_id) + _spiDevice.substr(_spiDevice.find_first_of(".")-1,1);
	strcpy(_id,idtmp.c_str());
#endif
    }
    else
    {
        cout << "Usage: " << argv[0] << " host port spiDevice" << endl;
        exit(-1);
    }

    cerr << "TCP to APA1O2 Client " << VERSION << ", id : " << _id << ", on : " << _spiDevice << ", waiting for " << _host << ":" << _port << endl;

    // Main loop ------------------------------------------
    while(1)
    {
        int sockFd	= tcpConnect( _host, _port );

        if( sockFd >= 0 )
        {
            if( send(sockFd, _id, strlen(_id), 0) >= 0 )
            {
                if( recv(sockFd, _initBuf, sizeof(_initBuf), 0) >= 0 )
                {
                    uint32_t	spiFreq 	= 0;
                    uint32_t	receiveSize	= 0;
                    uint8_t		brightness	= 0;
                    int			spiFd		= 0;

                    sscanf( _initBuf, "%d,%d,%2" SCNu8, &spiFreq, &receiveSize, &brightness );
                    spiFreq		*= 1000;

                    if(spiFreq != 0 || receiveSize != 0)
                    {
#ifdef HAS_SPI
                        spiFd = spiOpen( _spiDevice, spiFreq );
#endif
                        tcpClient( sockFd, spiFd, spiFreq, receiveSize, brightness, _header, _headerSize, _endFrame );
                    }
                    else
                    {
                        cerr << "<Main> Initialization failed." << endl;
                    }

#ifdef HAS_SPI
                    close( spiFd );
#endif
                    close( sockFd );
                    sleep( _delay );
                }
            }
        }
        else
        {
            cerr << "<Main> TCP Connect failed." << endl;
        }

        sleep( _delay );
    }

    exit( -1 );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
