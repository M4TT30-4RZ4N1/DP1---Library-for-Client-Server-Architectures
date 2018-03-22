
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <rpc/xdr.h>

#include <string.h>
#include <time.h>
#include <unistd.h>

#include "errlib.h"
#include "sockwrap.h"

//#define SRVPORT 1500

#define LISTENQ 15
#define MAXBUFL 10

#define MSG_ERR "wrong operands\r\n"
#define MSG_OVF "overflow\r\n"

#define MAX_UINT16T 0xffff
//#define STATE_OK 0x00
//#define STATE_V  0x01

#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif

char *prog_name;



int main (int argc, char *argv[]) {

	int listenfd, err=0;
	short port;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t cliaddrlen = sizeof(cliaddr);
	
	char buf_r[MAXBUFL+1]; 
        char buf_s[MAXBUFL+1]; 
        ssize_t byte_sent=0;
	ssize_t byte_recv=0;

	/* for errlib to know the program name */
	prog_name = argv[0];

	/* check arguments */
	if (argc!=2)
		err_quit ("usage: %s <protocol> <port>\n where <protocol> can be -a -x", prog_name);
		
		
	port=atoi(argv[1]);
	
	/* create socket */
	listenfd = Socket(AF_INET, SOCK_DGRAM, 0);

	/* specify address to bind to */
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	Bind(listenfd, (SA*) &servaddr, sizeof(servaddr));

	trace ( err_msg("(%s) socket created",prog_name) );
	trace ( err_msg("(%s) listening on %s:%u", prog_name, inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port)) );

	

while (1) { //il server non termina mai!!! quindi rimane sempre in ascolto
	
	
	trace( err_msg ("\n(%s) waiting for DATAGRAM ...", prog_name) );
		
		
	//ricezione datagram dal client
    	byte_recv = recvfrom(listenfd, buf_r, MAXBUFL , 0, (struct sockaddr*) &cliaddr, &cliaddrlen);
    
    	if (byte_recv  < 0) {
    	
      		printf("ERROR in recvfrom");
       		exit(-2);
      	}
      	
      else{
      
    		printf("Echo from client: %s\n", buf_r);
    		
      }
      
        //ricopio lo stesso messaggio e lo invio indietro
      	strcpy(buf_s,buf_r);
      	
    
    
	//invio datagram udp
	byte_sent = sendto(listenfd, buf_s, strlen(buf_s), 0,  (struct sockaddr*) &cliaddr, cliaddrlen);
    
	if (byte_sent  < 0){
      		printf("ERROR in sendto");
     		 exit(-1);
      	}
    
	
		
	}
	
	
	//chiudo il socket
		Close (listenfd);
	
	
	return 0;
}

