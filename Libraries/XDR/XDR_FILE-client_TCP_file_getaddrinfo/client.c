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
#include "types.h"

#define MAXBUFL 255
#define MAX_STR 1023
#define CHUNK 1024


#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif

char *prog_name;
int xdr_enable=0;
int sigpipe;

void sigpipeHndlr(int signal) {
 	 printf("SIGPIPE captured!\n");
  	sigpipe = 1;
  	return;
}

int ftp_receiver_xdr(int mysocket, char* progname){

	int j;
	char filename[MAXBUFL];
	char command[MAXBUFL];
	char garbage[MAXBUFL];
	FILE *fp;
	//defining the xdr variables
    
    	struct message message;
	    
	// Streams comunicazione
	XDR xdrs_in;
	XDR xdrs_out;
	FILE *stream_socket_r;
	FILE *stream_socket_w;
	//char  obuf[MAXBUFL];	   /* transmission buffer *
	//char  ibuf[MAXBUFL];	   /* reception buffer */

	printf("(%s) -  Client Application FTP-XDR_MODE start:\n",progname);
	
	while(1){

		printf("(%s) -  Insert the files to retrive from server (GET filename | QUIT): \n",progname);

		fgets(command,MAXBUFL,stdin);
		
		
		if(strncmp(command, "QUIT", 4) == 0){

			message.tag = QUIT;

			stream_socket_w = fdopen(mysocket, "w");
			
			xdrstdio_create(&xdrs_out, stream_socket_w, XDR_ENCODE);

			if(!xdr_message(&xdrs_out, &message)){
				printf("(%s) - Error in transmitting data QUIT \n",progname);
				exit(1);
			}

			xdr_destroy(&xdrs_out);
			Close(mysocket);
			return 1;
		}else if(strncmp(command,"GET", 3) == 0 ){
		
			
				sscanf(command,"%s %s", garbage, filename);
				
			
				//RICHIESTA
				
				message.tag = GET;
				message.message_u.filename = filename;

				stream_socket_w = fdopen(mysocket, "w");

				xdrstdio_create(&xdrs_out, stream_socket_w, XDR_ENCODE);

				if(!xdr_message(&xdrs_out, &message)){
					printf("(%s) - Error in transmitting data GET \n", progname);
					exit(1);
				}
				fflush(stream_socket_w);

				xdr_destroy(&xdrs_out);

				//RICEZIONE 
				stream_socket_r = fdopen(mysocket, "r");

				if (stream_socket_r == NULL){
					err_sys("(%s) error - fdopen() failed", progname);
				}

				xdrstdio_create(&xdrs_in, stream_socket_r, XDR_DECODE);
			
				// PULIZIA STRUTTURA RICEZIONE
				memset(&message, 0, sizeof(message));
				message.tag = 0;
				message.message_u.filename = NULL;
				message.message_u.fdata.contents.contents_len = 0;
				message.message_u.fdata.contents.contents_val = NULL;
				message.message_u.fdata.last_mod_time = 0;
			
				if(!xdr_message(&xdrs_in, &message)){
					printf("(%s) - ERROR (cannot read message with XDR)", progname) ;
					xdr_destroy(&xdrs_in);
				}else{

					if(message.tag == OK){

						if((fp = fopen(filename, "w")) == NULL){
							printf("(%s) - Error in opening file\n",progname);
							exit(1);
						}
						char* c_time_string;
						c_time_string = ctime((const time_t* )&message.message_u.fdata.last_mod_time);
						printf("(%s) - File %s received; Last modification time: %s; File size: %u byte\n",progname, filename,c_time_string, message.message_u.fdata.contents.contents_len);

						for(j = 0; j < message.message_u.fdata.contents.contents_len; j++){
							fprintf(fp, "%c", message.message_u.fdata.contents.contents_val[j]);
						}
						fclose(fp);

						printf("(%s) -File written in the local file system\n",progname);
					}

					if(message.tag == ERR){

						printf("(%s) - The server have sent an error message; file can not be downloaded\n", progname);

					}
					xdr_destroy(&xdrs_in);
				}
			}else{ printf("(%s) - Invalid command...(GET filename | QUIT)\n", progname); continue;}
		
	}//end while
}

int main (int argc, char *argv[]) {

	//signal(SIGINT, SIG_IGN);//ignore ctrl-c quit only with commands		
	
	int sockfd, control,err=0;
	char dest_h[MAXBUFL], dest_p[MAXBUFL];

	/* for errlib to know the program name */
	prog_name = argv[0];

	/* check arguments */
	if (argc==3){
		xdr_enable=0;
		strcpy(dest_h,argv[1]);
		strcpy(dest_p,argv[2]);
	}else if(argc==4){
		
		if(strcmp("-x",argv[1])==0){
			xdr_enable=1;
			strcpy(dest_h,argv[2]);
			strcpy(dest_p,argv[3]);
		}
		else{
			err_quit ("usage: %s <port> or %s <-x> <port>\n", prog_name, prog_name);
		}
	
	}else{
		err_quit ("usage: %s <dest_host> <dest_port> or  %s <-x> <dest_host> <dest_port>", prog_name, prog_name);
	}	
	
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
	if(xdr_enable==1){
		control=ftp_receiver_xdr(sockfd,prog_name);
	}else{
		control=ftp_receiver(sockfd,prog_name);
	}
	
	if(control==-1){
		printf("(%s) --- error: protocol\n", prog_name);
	}

// END PROTOCOL 	
	
	

	return 0;
}

