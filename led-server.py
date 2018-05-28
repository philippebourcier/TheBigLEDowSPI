#!/usr/bin/pypy

# DO NOT USE Python to run this script... use pypy !
# Your CPU will thank you.

import	time
import 	random
import 	socket
import	binascii
import threading
import sys
sys.stdout = sys.stderr

# Utilities -------------------------------------------------------------------
def setPattern( id, brightness ):
	ptrn = _patterns[id]
	
	for i in xrange(len(ptrn)):
		if ptrn[i] != 0:
			ptrn[i] = int( round(ptrn[i] * brightness) )
	
	return ptrn

# APA102 utilities ----------------------------------------
def createApa102( start, header, pattern, nbLeds ):
	lgPtn	= len(pattern)
	lgLeds	= nbLeds * 3
	j		= start
	'''
	# Fake test frames 
	if random.random() > 0.9:		
		if random.random() > 0.5:
			data	= [33, 24, 87, 55, 83, 27, 33, 22, 44] + [0] * lgLeds
			print "fake packet(+2) :"
		else:
			data	= [33, 24, 87] + [0] * lgLeds
			print "fake packet(-4) :"	
	else:
		data	= header + [0] * lgLeds
	'''
	data	= header + [0] * lgLeds
	
	for i in xrange(len(header), len(data)):
		if j >= lgPtn:
			j = 0
		
		data[i] = pattern[j]
		j += 1

	return 	"".join( chr( byte ) for byte in data )

# TCP Client ----------------------------------------------
class TcpClient( threading.Thread ):
    def __init__( self, cnx, params ):
		threading.Thread.__init__(self)
		self.cnx 	= cnx
		self.params = params
		self.size	= len(params["header"]) + (params["nbLeds"] * 3)

    def run(self):
		params		= self.params
		start 		= 0
		self.cnx.send( str(params["kHz"]) + "," + str(self.size) )

		try:
			while True:
				data = createApa102( start, params["header"], params["pattern"], params["nbLeds"] )
				
				start 	+= params["move"] 
				if start >= params["lg"]:
					start -= params["lg"]
				elif start < 0:
					start += params["lg"]

				self.cnx.send( data )
				#print "packet(" + str(len(data)) + ")=[" + binascii.hexlify(data) + "] in " + str(params["delay"])				
				#print "packet(" + str(len(data)) + ") in " + str(params["delay"])
				time.sleep( params["delay"] )

				'''
				# Random network delays 
				delay = 0.05 + (random.random() * 0.2)
				print "packet(" + str(len(data)) + ") in " + str(delay)
				time.sleep( delay )
				'''

		except socket.error as msg:
			print 'Connection failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
			self.cnx.close()
			
# MAIN ------------------------------------------------------------------------
if __name__ == '__main__':
	
	_patterns	= {				# pattern length must be a multiple of 3 (RGB)		
					"red"	: [255,0,0],
					"green"	: [0,255,0],
					"blue"	: [0,0,255],
                                        "france": [0,0,255, 0,0,255, 255,255,255, 255,255,255, 255,0,0, 255,0,0],
					"om"	: [10,109,170, 10,109,170, 255,255,255, 255,255,255],
					"orange": [255,128,0],
					"purple": [0,128,255]
				}
			
	# Parameters ----------------------
	nbLeds 		= 3000
	kHz		= 2400
	patternID	= "france"
	brightness 	= 1			#  0 <-> 1
	move		= 2			# -m < 0 > m, led movement per frame refresh	
	IPport		= 4200		
	fps		= 30		# nb frames per second
	
	# Initialisations -----------------
	_params		= {
					"header" 	: [65, 80, 65, 49, 48, 50, 95],
					"nbLeds"	: nbLeds,
					"kHz"		: kHz,					
					"pattern"	: setPattern(patternID, brightness),
					"move"		: (move * 3),
					"delay"		: (1.0 / fps)
				}
	_params["lg"] = int( round(len(_params["pattern"])) )

	_sock 		= socket.socket( socket.AF_INET, socket.SOCK_STREAM )
	_sock.setsockopt( socket.SOL_SOCKET, socket.SO_REUSEADDR, 1 )
	_sock.bind( (socket.gethostname(), IPport) )
	print 'starting up on ' + socket.gethostname()  + ' port ' + str(IPport)
	_sock.listen( 5 )

	# Main loop -----------------------
	while True:
		print 'waiting for a connection'
		connection, client_address = _sock.accept()		
		print 'connection from %s:%s' % client_address 
		id 		= connection.recv( 30 )
		print 'client id = <' + str(id) + '> at ' + str(kHz) + ' kHz\n' 
		
		client 	= TcpClient( connection, _params )
		client.start()

#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
