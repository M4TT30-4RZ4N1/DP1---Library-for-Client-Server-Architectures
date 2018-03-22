
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
#define MAXBUFL 255

#define MSG_ERR "wrong operands\r\n"
#define MSG_OVF "overflow\r\n"

#define MAX_UINT16T 0xffff
//#define STATE_OK 0x00
//#define STATE_V  0x01


char *prog_name;

//try_recv is a function that try to receive data from server with N tries defined in the udp_protocol
//servaddr è un puntatore all struttura generica contenente l'indirizzo del server
void try_recv(int mysocket,char* progname, struct sockaddr* servaddr, fd_set socketlist,struct timeval mytimer,int timeout,int* flag){
		
		int n;
		socklen_t l_server;
		l_server = sizeof(servaddr);
		ssize_t byte_recv=0;
		char buf[MAXBUFL+1]; 

		//block on read
		if((n=select(FD_SETSIZE,&socketlist,NULL,NULL,&mytimer))==-1){
			printf("(%s) - select() - failed\n", progname);
		}
		else if(n>0){

    			//recfrom datagram
    			l_server = sizeof(servaddr);
    			byte_recv = recvfrom(mysocket, buf, MAXBUFL , 0, servaddr,&l_server);
    
    			if (byte_recv  < 0) {
      				printf("(%s) - ERROR in recvfrom\n", progname);
       				exit(-2);
      			}
      			else{
      			
      			/*TODO: MAYBE SOME OPERATIONS ON RECEIVED DATA  */
    				printf("(%s) - Echo from server: %s\n", progname, buf);
				*flag=1;

    			}
		}
		else{ /*TODO: TIMEOUT EXPIRED */
			printf("(%s) - No response - timeout #%d  has expired \n",progname, timeout);
		}

	
}

//servaddr è un puntatore all struttura generica contenente l'indirizzo del server
int udp_client_protocol(int mysocket, char* progname,struct sockaddr* servaddr){

		char buf[MAXBUFL];
		int result=0; 
		ssize_t byte_sent=0;
		size_t buf_len=0;
		socklen_t servaddr_len;
		servaddr_len = sizeof(struct sockaddr_storage);
				
		
		 /*TODO: USER INTERFACE, POPULATE BUF AND SEND DATA */
    		bzero(buf, MAXBUFL);
    		printf("(%s) - Please enter msg: ", progname);
    		fgets(buf, MAXBUFL , stdin);
    		buf_len=strlen(buf);
		//sendto datagram udp
//ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,const struct sockaddr *dest_addr, socklen_t addrlen);
    		byte_sent = sendto(mysocket, buf, buf_len, 0, servaddr, servaddr_len);
    		
    		if(byte_sent!= buf_len){
    			printf("(%s) - ERROR in sendto, not all data sent\n", progname);
      			exit(-1);
    		} 
    		if (byte_sent  < 0){
      			printf("(%s) - ERROR in sendto, no data sent\n", progname);
      			exit(-1);
      		}
      		
	
		//try recvfrom
		//a) rcvfrom with no problems
		//b) rcvfrom in one of the tries
		//c) never perform recvfrom

		//set TIMER and socket list(only one), i can change time in base the #request
		//it's better to manage this types here to have more flexibility in the protocol
		struct timeval mytimer;//creo timer
		fd_set socketlist; // socket list
		FD_ZERO(&socketlist);//clear
		FD_SET(mysocket,&socketlist);//insert socke in the list
		mytimer.tv_sec=3; // 3 sec
		mytimer.tv_usec=0;	

		int retry=5;  //tries
		int flag=0;   //FAIL recvfrom, become 1 when it perform one recvfrom

		while(retry!=0 && flag==0){
		//recvfrom with timer
		/*TODO: recvfrom with timer*/
			try_recv(mysocket,progname,(struct sockaddr*) &servaddr, socketlist, mytimer, retry, &flag); 
			retry--;
		}


 
		result=close(mysocket);

		//control
		if(result<0 ){
			printf("(%s) - errore close %d\n",progname, result);
			return -1;
		}
		else{
			printf("(%s) - close client application \n",progname);
			return 0;

		}
		
		
			
}

int main (int argc, char *argv[]) {

		//short port_number;
		int sockfd=0, err=0;
		
		struct addrinfo hints;
		struct addrinfo *result, *rp;
		struct sockaddr_storage* servaddr;  //identify server both ipv4/ipv6

//start

		if( argc!= 3){
			printf("(%s) - Error number of parameters", prog_name);
			return -1;
		}

		prog_name=argv[0];

		//port_number=atoi(argv[2]);

		if(sockfd<0){
			err_msg("(%s) - socket() failed",prog_name);
		}

		// getaddrinfo paradigm
		 memset(&hints, 0, sizeof(struct addrinfo));
		 hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
		 hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
		 hints.ai_flags = 0;    	 /* NO wildcard IP address */
		 hints.ai_protocol = 0;          /* Any protocol */
		 hints.ai_canonname = NULL;
		 hints.ai_addr = NULL;
		 hints.ai_next = NULL;

		//argv[1] is the address, argv[2] is the port 
		 err = getaddrinfo(argv[1],argv[2], &hints, &result);
		   if (err != 0) {
		       printf("(%s) - getaddrinfo: %s\n", prog_name, gai_strerror(err));
		       exit(EXIT_FAILURE);
		   }
		   
		   for (rp = result; rp != NULL; rp = rp->ai_next) {
		   
		       sockfd= socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
		               
		       if (sockfd != -1)
		           break;  		/* Success */

		       close(sockfd);
		   }
		   
		   if (rp == NULL) {               /* No address succeeded */
		       printf("(%s) - Could not create socket\n", prog_name);
		       exit(EXIT_FAILURE);
		   }
			
		   //servaddr è un puntatore all struttura generica contenente l'indirizzo del server
		   servaddr= (struct sockaddr_storage*) rp->ai_addr;
           

//PROTOCOL UDP
		   udp_client_protocol(sockfd,prog_name,(struct sockaddr*) servaddr);
		   
//END PROTOCOL, SOCKET CLOSE IN THE PROTOCOL

	          freeaddrinfo(result);		   


		return 0;
}
