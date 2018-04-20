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

#include <fcntl.h>				//Needed for SPI port
#include <linux/spi/spidev.h>	//Needed for SPI port

using namespace std;

// ID Utilities ---------------------------------------------------------------
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
    //SPI_MODE_0 (0,0) 	CPOL = 0, CPHA = 0, Clock idle low, data is clocked in on rising edge, output data (change) on falling edge
    //SPI_MODE_1 (0,1) 	CPOL = 0, CPHA = 1, Clock idle low, data is clocked in on falling edge, output data (change) on rising edge
    //SPI_MODE_2 (1,0) 	CPOL = 1, CPHA = 0, Clock idle high, data is clocked in on falling edge, output data (change) on rising edge
    //SPI_MODE_3 (1,1) 	CPOL = 1, CPHA = 1, Clock idle high, data is clocked in on rising, edge output data (change) on falling edge
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
/*	
    status = ioctl(fd, SPI_IOC_RD_MODE, &mode);
    if(status < 0)
    {
      perror("Could not set SPIMode (RD)...ioctl fail");
      exit(1);
    }
	
    status = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bitsPerWord);
    if(status < 0)
    {
      perror("Could not set SPI bitsPerWord(RD)...ioctl fail");
      exit(1);
    }

    status = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if(status < 0)
    {
      perror("Could not set SPI speed (RD)...ioctl fail");
      exit(1);
    }
*/	
    return( fd );
}

int spiClose( int fd )
{
	return( close(fd) );
}

int spiWrite( int fd, uint8_t *data, int size, uint32_t speed )
{	
	struct spi_ioc_transfer tr;
	
	memset(&tr, 0, sizeof tr);

	tr.tx_buf 		= (unsigned long)(data);
	tr.len 			= (unsigned long)(size);
	tr.speed_hz 		= speed;
	tr.bits_per_word 	= 8;						
				
	return ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}

// TCP Utilities --------------------------------------------------------------
int tcpCconnect(string address, int port)
{
    struct sockaddr_in server;
    int sock = socket(AF_INET , SOCK_STREAM , 0);
		
	if (sock == -1)
	{
		cout << "Could not create socket" << endl;					 
		return -1;
	}
    else   
        cout << "Socket created"  << endl;

    if(inet_addr(address.c_str()) == -1)
    {
        struct hostent *he;
        struct in_addr **addr_list;

        if ( (he = gethostbyname( address.c_str() ) ) == NULL)
        {
            cout << "Gethostbyname : Failed to resolve hostname" << endl;
            return -1;
        }

        addr_list = (struct in_addr **) he->h_addr_list;
 
        for(int i = 0; addr_list[i] != NULL; i++)
        {
            server.sin_addr = *addr_list[i];
             
            cout << address << " resolved to " << inet_ntoa(*addr_list[i]) << endl;
             
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
        cout << "connect failed" << endl;
        return -1;
    }
     
    cout << "Connected" << endl;
    return sock;
}

// Utilities ------------------------------------------------------------------
int endProg( int fd, int value ) 
{
	spiClose( fd );
	exit( value );
}

uint8_t* createData( int size, int nbLeds ) 
{
	uint8_t *data 	= new uint8_t[size]();
	int lg 			= (nbLeds+1) * 4;

	for(int i=4; i<lg; i+=4) 
	{
		data[i] = 255;
	}
	
	return data;
}

void setFrame( char *buf, int size, uint8_t *data ) 
{
	for(int i=0, j=4; i<size; i+=3, j+=4) 
	{
		data[j+1] = (uint8_t) buf[i+0];
		data[j+2] = (uint8_t) buf[i+1];
		data[j+3] = (uint8_t) buf[i+2];
	}	
}

void printFrame( uint8_t *data, int size, char *pBuffer ) 
{
	for(int i=0; i<size; i++) 
	{
		sprintf( &pBuffer[3 * i], "%02X,", (char) data[i] );
	}
}

// MAIN -----------------------------------------------------------------------
int main(int argc, char *argv[])
{
	char		id[30];
	memset(id, 0, 30);   
	
	string 		host		= "";
	int 		port		= 4444;
	int			nbLeds		= 0;
	
	int			spifd		= 0;
	string 		spiDevice	= ""; // /dev/spidev0.0 /dev/spidev1.0
	uint32_t 	spiSpeed	= 0;
	
	char 		*buffer;
	int 		bufferSize 	= 0;
	int			bufferPos 	= 0;
	
	uint8_t 	*data;
	int 		dataSize 	= 0;
	
	char *		pBuffer;
	
    if ( argc == 1 )
    {	
		cout << "ID : " << getSerialNumber() << "_0" << endl;
		//cout << "ID : " << getMacAddress() << "_0" << endl;
		exit(0);
	} 
	else if ( argc == 6 )
	{		
		host 		= string(argv[1]);
		port 		= atoi(argv[2]);
		nbLeds 		= atoi(argv[5]);
		
		spiDevice 	= string(argv[3]);

		sprintf(id, "id_%s_%c", getSerialNumber(), spiDevice.at(spiDevice.find_first_of(".") - 1) );
//		sprintf(id, "id_%s_%c", getMacAddress(), );			
		
		spiSpeed			= atoi(argv[4]) * 1000000;
		uint32_t maxSpeed 	= uint32_t((19200 - (2.95 * nbLeds)) * 1000000);
		
		if(spiSpeed > maxSpeed)
		{
			spiSpeed = maxSpeed;
			cout << "Warning : MHz param " << argv[3] << " > calc max MHz = "<< (maxSpeed/1000000) << endl;
			cout << "Calc speed used instead..." << endl;
		}
		spifd 		= spiOpen( spiDevice, spiSpeed );
		
		if(spifd < 0)
			endProg( spifd, -2 );		
		
		bufferSize 	= nbLeds * 3;
		buffer		= new char[bufferSize];
		
		dataSize 	= 4 + (nbLeds * 4);// + 8; // + (8 * (nbLeds >> 4));
		data		= createData( dataSize, nbLeds );
		pBuffer		= new char[dataSize * 3];
    } 
	else 
	{
        cout << "Usage: " << argv[0] << " host port spiDevice MHz nbLeds" << endl;
		exit(-1);
    }

	cout << "Client ID = " << id << " waiting for " << host << ":" << port << " to send " << nbLeds << " leds at " << (spiSpeed/1000000) << " MHz..." << endl;
	cout << "bufferSize  = " << bufferSize << ", dataSize = " << dataSize << endl;

	// Main loop --------------------------------------------------------------
    while(1)
    {
		int sockfd	= tcpCconnect(host, port);
		 
		if(sockfd >= 0)
		{
			if(send(sockfd, id, strlen(id), 0) >= 0)
			{
				while(1)
				{
					int lg = recv(sockfd, &buffer[bufferPos], (bufferSize - bufferPos), 0);
					
					if(lg < 0)
					{
						cout << "Receive failed" << endl;
						break;
					}
					else
					{
						if( bufferPos == 0 )
						{				
							//cout << "First frame found for " << lg << " bytes" << endl;						
							bufferPos = lg;
						}
						else			
						{
							//cout << "Other frame for " << lg << " bytes" << endl;
							bufferPos += lg;
						}
						
						//cout << "----------------bytes received = " << bufferPos << "/" << bufferSize  << endl;
						if( bufferPos == bufferSize )
						{
							setFrame( buffer, bufferSize, data );
							//printFrame( data, dataSize, pBuffer );
							//cout << "Frame = [" <<  pBuffer << "]" << endl;
							spiWrite( spifd, data, dataSize, spiSpeed );
							bufferPos = 0;
						}
						else if(bufferPos > bufferSize)
						{
							cout << "Frame rejected = " << bufferPos << endl;
							bufferPos = 0;
						}
					}
				}
			}
		}
		
		close( sockfd );		
		sleep( 1 );
	}
	
    endProg( spifd, 0 );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------