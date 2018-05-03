#include <iostream> 
#include <stdio.h> 
#include <string.h> 
#include <string> 
#include <sys/socket.h>  
#include <arpa/inet.h> 
#include <netdb.h> 
 
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/if.h>

#include <fcntl.h>		//Needed for SPI port
#include <linux/spi/spidev.h>	//Needed for SPI port

using namespace std;

// _id Utilities ---------------------------------------------------------------
const char* getSerialNumber()
{
	static char str[16 + 1];
	char line[256]; 
	FILE *fd = fopen("/proc/cpuinfo", "r");
	
	if (!fd) 
	{
		return str;
	}

	while (fgets(line, 256, fd)) 
	{
		if (strncmp(line, "Serial", 6) == 0) 
		{
			strcpy(str, strchr(line, ':') + 2);
			str[strlen (str) -1] = 0;
			break;
		}
	}

	fclose(fd);

	return 	str;
}

const char* getMacAddress()
{
	static char str[18];
	struct 		ifreq s;
	int 		fs = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	strcpy(s.ifr_name, "eth0");
	
	if (0 == ioctl(fs, SIOCGIFHWADDR, &s)) 
	{
		snprintf(	
					str, sizeof(str), 
					"%02x:%02x:%02x:%02x:%02x:%02x", 
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
    uint8_t mode 		= SPI_MODE_0;
    uint8_t	bitsPerWord = 8;
    int 	fd 			= open(device.c_str(), O_RDWR);

    if (fd < 0)
    {
        perror("Error - Could not open SPI device");
        exit(1);
    }

    status = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if(status < 0)
    {
        perror("Could not set SPIMode (WR)...ioctl fail");
        exit(1);
    }

    status = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bitsPerWord);
    if(status < 0)
    {
      perror("Could not set SPI bitsPerWord (WR)...ioctl fail");
      exit(1);
    }

    status = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if(status < 0)
    {
      perror("Could not set SPI speed (WR)...ioctl fail");
      exit(1);
    }

    return( fd );
}

int spiClose( int fd )
{
	return( close(fd) );
}

int spiWrite( int fd, uint32_t speed, uint8_t *data, int size )
{	
	struct spi_ioc_transfer tr;
	
	memset(&tr, 0, sizeof tr);

	tr.tx_buf 			= (unsigned long)(data);
	tr.len 				= (unsigned long)(size);
	tr.speed_hz 		= speed;
	tr.bits_per_word 	= 8;						
				
	return ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}

// TCP Utilities --------------------------------------------------------------
int tcpConnect(string address, int port)
{
    struct sockaddr_in server;
    int reuse 	= 1;
    int sock 	= socket(AF_INET , SOCK_STREAM , 0);
	
	if (sock == -1)
	{
		cerr << "Could not create socket" << endl;					 
		return -1;
	}
    else   
        cerr << "Socket created"  << endl;

/*	
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
*/

    if(inet_addr(address.c_str()) == -1)
    {
        struct hostent *he;
        struct in_addr **addr_list;

        if((he = gethostbyname(address.c_str())) == NULL)
        {
            cerr << "Gethostbyname : Failed to resolve hostname" << endl;
            return -1;
        }

        addr_list = (struct in_addr **) he->h_addr_list;
 
        for(int i = 0; addr_list[i] != NULL; i++)
        {
            server.sin_addr = *addr_list[i];
             
            cerr << address << " resolved to " << inet_ntoa(*addr_list[i]) << endl;
             
            break;
        }
    }
    else
    {
        server.sin_addr.s_addr = inet_addr( address.c_str() );
    }
     
    server.sin_family 	= AF_INET;
    server.sin_port 	= htons( port );

    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        cerr << "connect failed" << endl;
        return -1;
    }
     
    cerr << "Connected" << endl;
    return sock;
}

// Utilities ------------------------------------------------------------------
void printFrame( uint8_t *data, int size ) 
{
	for(int i=0; i<size; i++) 
	{
		fprintf( stderr, "%02X,", data[i] );
	}
	
	fprintf( stderr, "\n" );
}

void send2spi( int fd, uint32_t freq, char *data, int size, bool endFrame )
{
	int 		i	= 0;
	uint32_t  	lgB = (uint32_t) (size * 4 / 3) + 4;
	lgB += (endFrame) ? ((uint32_t)(size/6) + 4) : 0;
	
	uint8_t buffer[lgB];
	
	for(; i<4; i++) 
	{
		buffer[i] 	= (uint8_t) 255;
	}

	for(int j=0; j<size; i+=4, j+=3) 
	{
		buffer[i] 	= (uint8_t) 255;
		buffer[i+1] = (uint8_t) data[j];
		buffer[i+2] = (uint8_t) data[j+1];
		buffer[i+3] = (uint8_t) data[j+2];
	}		
	
	for(; i<lgB; i++)
	{
		buffer[i] 	= (uint8_t) 255;
	}
	
	spiWrite( fd, freq, buffer, lgB );
	cerr << "<send2spi> data size = " << size << ", spi size = " << lgB << endl; 
//printFrame( buffer, lgB );
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

// MAIN -----------------------------------------------------------------------
int main(int argc, char *argv[])
{
	char		_id[21];
	string 		_host		= "127.0.0.1";
	int 		_port		= 1234;
	string 		_spiDevice	= "id_0"; 								// for test "id_1234_0"; 
		
	char		_header[]	= { 65, 80, 65, 49, 48, 50, 95 };	// "APA102_"
	int		_headerSize	= sizeof(_header) / sizeof(char);
	
	char 		_freqBuf[10];

	// Parameters -----------------------------------------
	int		_maxSize	= 20000;
	int 		_delay 		= 1;
	bool		_endFrame	= false;
	
	// Arguments ------------------------------------------
    if ( argc == 1 )
    {	
		cerr << "_id : id_" << getSerialNumber() << "_0" << endl;
//		cerr << "_id : id_" << getMacAddress() 	 << "_0" << endl;
		exit(0);
	} 
	else if ( argc == 4 )
	{		
		_host 		= string(argv[1]);
		_port 		= atoi(argv[2]);
		_spiDevice 	= string(argv[3]);	// /dev/spidev0.0 /dev/spidev1.0
		sprintf(_id, "id_%s_%c", getSerialNumber(),_spiDevice.at(_spiDevice.find_first_of(".") - 1) );
//		sprintf(_id, "id_%s_%c", getMacAddress(),_spiDevice.at(_spiDevice.find_first_of(".") - 1) );
    } 
	else 
	{
        cerr << "Usage: " << argv[0] << " host port spiDevice" << endl;
		exit(-1);
    }

	cerr << "Client id : " << _id << ", on : " << _spiDevice << ", waiting for " << _host << ":" << _port << endl;

	// Main loop ------------------------------------------
	int bufferSize;
	
	while(1)
    {
		int sockFd	= tcpConnect( _host, _port );
		int dataSize 	= 0;
		
		if( sockFd >= 0 )
		{
			if( send(sockFd, _id, strlen(_id), 0) >= 0 )
			{
				if( recv(sockFd, _freqBuf, sizeof(_freqBuf), 0) >= 0 )
				{
					uint32_t	spiFreq	= atoi(_freqBuf) * 1000;					
					int 		spiFd 	= spiOpen( _spiDevice, spiFreq );			
					char 		buffer[_maxSize];	
					char 		data[_maxSize];					
					
					cerr << "<main> connected at " << spiFreq << " Hz" << endl;					
					
					while(1)
					{						
						bufferSize = recv( sockFd, buffer, sizeof(buffer), 0 );
						
						if(bufferSize <= 0)
						{
							cerr << "Receive failed" << endl;
							break;
						}
						else
						{
							if( isNewFrame(_header, _headerSize, buffer, bufferSize) ) 
							{
								if(dataSize != 0)
									send2spi( spiFd, spiFreq, data, dataSize, _endFrame );
								
								dataSize = bufferSize - _headerSize;
								
								memset( data, 0, sizeof(data) );
								memcpy( data, &buffer[_headerSize], dataSize );
							} 
							else
							{
								memcpy( &data[dataSize], buffer, bufferSize );
								dataSize += bufferSize;
							}
						}
					}
					
					spiClose( spiFd );
				}
			}
		}
		
		close( sockFd );		
		sleep( _delay );
	}
	
    exit( -1 );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
