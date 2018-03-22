#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <string.h>
#include <time.h>

#include "errlib.h"
#include "sockwrap.h"
#include "ftp.h"

#define MAXBUFL 255
#define MAX_STR 1023
#define CHUNK 1024


char *prog_name;
int sigpipe;

void sigpipeHndlr(int signal) {
 	 printf("SIGPIPE captured!\n");
  	sigpipe = 1;
  	return;
}


int client_protocol(int mysocket, char* progname);

int client_protocol(int mysocket, char* progname){//TODO: AGGIUNGI PARAMETRI PER IL PROTOCOLLO

	printf("(%s) --- Start Application ---\n", progname);
	
	return 0;
}


int main (int argc, char *argv[]) {

	//signal(SIGINT, SIG_IGN);//ignore ctrl-c quit only with commands		
	
	int sockfd, control,err=0;
	char dest_h[MAXBUFL], dest_p[MAXBUFL];

	/* for errlib to know the program name */
	prog_name = argv[0];

	/* check arguments */
	if (argc!=3)
		err_quit ("usage: %s <dest_host> <dest_port>", prog_name);
	strcpy(dest_h,argv[1]);
	strcpy(dest_p,argv[2]);
	
//----------------------------------------------------------------------------------------------------
	 /* Obtain address(es) matching host/port */

	   struct addrinfo hints;
           struct addrinfo *result, *rp;		
		
           memset(&hints, 0, sizeof(struct addrinfo));
           hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
           hints.ai_socktype = SOCK_STREAM; /* TCP socket */
           hints.ai_flags = 0;
           hints.ai_protocol = 0;          /* Any protocol */

	   //getaddrinfo(host, port, &hints, &result);
           err = getaddrinfo(dest_h, dest_p, &hints, &result);
           if (err != 0) {
               fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
               exit(EXIT_FAILURE);
           }

           /* getaddrinfo() returns a list of address structures.
              Try each address until we successfully connect(2).
              If socket(2) (or connect(2)) fails, we (close the socket
              and) try the next address. */

           for (rp = result; rp != NULL; rp = rp->ai_next) {
               sockfd = socket(rp->ai_family, rp->ai_socktype,
                            rp->ai_protocol);
               if (sockfd == -1)
                   continue;

               if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
                   break;                  /* Success */

               close(sockfd);
           }

           if (rp == NULL) {               /* No address succeeded */
               fprintf(stderr, "Could not connect\n");
               exit(EXIT_FAILURE);
           }

  
           /* VERSION IP */

	   struct sockaddr* sa= rp->ai_addr;		
	   struct sockaddr_in6* sa_ipv6;
	   struct sockaddr_in* sa_ipv4;
	   int flag_ip=-1;  //this flag if equal to 0 is ipv4, otherwise is ipv6
	   char server[MAXBUFL];
	   
	   if( rp->ai_family==AF_INET6){// IPV6
	   	flag_ip=1;
		fprintf(stdout,"IPV6 Connection !!!\n");
		sa_ipv6=(struct sockaddr_in6 *) sa;
		char straddr6[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &(sa_ipv6->sin6_addr), straddr6,sizeof(straddr6));
		sprintf(server,"Server - %s : %u",  straddr6, ntohs(sa_ipv6->sin6_port));
	        err_msg("(%s) connected to %s\n", prog_name,server);
	   }
 	   else{ // IPV4
 	        flag_ip=0;
		fprintf(stdout,"IPV4 Connection !!!\n");
		sa_ipv4=(struct sockaddr_in *) sa;
		sprintf(server,"Server - %s : %u",inet_ntoa(sa_ipv4->sin_addr), ntohs(sa_ipv4->sin_port));
		err_msg("(%s) connected to %s", prog_name, server);
	   }
	
	   freeaddrinfo(result);           /* No longer needed */

//----------------------------------------------------------------------------------------------------
	  sigpipe = 0;
  	  signal(SIGPIPE, sigpipeHndlr);
  	  
// TODO:START PROTOCOL 
	
	control=ftp_receiver_multiplexing(sockfd,prog_name);
	if(control==-1){
		printf("(%s) --- error: protocol\n", prog_name);
	}

// END PROTOCOL 	
	
	

	return 0;
}

