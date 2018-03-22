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

#define MSG_OK "+OK"
#define MSG_GET "GET"
#define MSG_QUIT "QUIT"
#define MSG_ERR "-ERR"

#include "errlib.h"
#include "sockwrap.h"

#define BUF_SIZE 500
#define MAXBUFL 255
#define MAX_STR 1023

#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif

char *prog_name;
int ftp(int sockfd);
     
int main(int argc, char *argv[])
       {
          
           int sfd, err, j;
           size_t len;
           ssize_t nread;
           char buf[BUF_SIZE];
	   int check;
		
	   /* for errlib to know the program name */
	   prog_name = argv[0];

           if (argc < 3) {
               fprintf(stderr, "Usage: %s host port msg...\n", argv[0]);
               exit(EXIT_FAILURE);
           }

           /* Obtain address(es) matching host/port */

	   struct addrinfo hints;
           struct addrinfo *result, *rp;		
		
           memset(&hints, 0, sizeof(struct addrinfo));
           hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
           hints.ai_socktype = SOCK_STREAM; /* TCP socket */
           hints.ai_flags = 0;
           hints.ai_protocol = 0;          /* Any protocol */

	   //getaddrinfo(host, port, &hints, &result);
           err = getaddrinfo(argv[1], argv[2], &hints, &result);
           if (err != 0) {
               fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
               exit(EXIT_FAILURE);
           }

           /* getaddrinfo() returns a list of address structures.
              Try each address until we successfully connect(2).
              If socket(2) (or connect(2)) fails, we (close the socket
              and) try the next address. */

           for (rp = result; rp != NULL; rp = rp->ai_next) {
               sfd = socket(rp->ai_family, rp->ai_socktype,
                            rp->ai_protocol);
               if (sfd == -1)
                   continue;

               if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
                   break;                  /* Success */

               close(sfd);
           }

           if (rp == NULL) {               /* No address succeeded */
               fprintf(stderr, "Could not connect\n");
               exit(EXIT_FAILURE);
           }

           

           /* VERSION IP */

	   struct sockaddr* sa= rp->ai_addr;		
	   struct sockaddr_in6* sa_ipv6;
	   struct sockaddr_in* sa_ipv4;
	   
	   if( rp->ai_family==AF_INET6){// IPV6
		fprintf(stdout,"IPV6 Connection !!!\n");
		sa_ipv6=(struct sockaddr_in6 *) sa;
		char straddr6[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &(sa_ipv6->sin6_addr), straddr6,sizeof(straddr6));
	        err_msg("(%s) connected to %s:%u", prog_name, straddr6, ntohs(sa_ipv6->sin6_port));
	   }
 	   else{ // IPV4
		fprintf(stdout,"IPV4 Connection !!!\n");
		struct sockaddr_in* sa_ipv4=(struct sockaddr_in *) sa;
		err_msg("(%s) connected to %s:%u", prog_name, inet_ntoa(sa_ipv4->sin_addr), ntohs(sa_ipv4->sin_port));
	   }
	
	   freeaddrinfo(result);           /* No longer needed */
		
	   //PROTOCOL
	   check=ftp(sfd);

	   if(check!=0){
		fprintf(stderr,"Error checked in the ftp protocol !!!\n");		
	   }
	   else{
		fprintf(stdout,"End of ftp protocol !!!\n");
	   }

	   //close the socket and release resources
           close(sfd);
     	   return 0;
       }

int ftp(int sockfd){

	char buf[MAXBUFL];
	char fname[MAX_STR];
	char garbage[MAX_STR];

	fd_set mysockets;
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	int nread = 0; char c;
	int n,i;

	uint32_t file_bytes;
	uint32_t val_time;

	char fnamestr[MAX_STR+1];

	FILE *fp;

	fprintf(stdout,"STARTING FTP PROTOCOL ...\n");

	while(1){
	
	printf("Insert command GET FILENAME/QUIT:\n");
	n=0;
	do{ //read command line
		c=getchar();
		buf[n]=c;
		n++;
	} while(c!='\n'&& n< MAXBUFL);

	n--;
	buf[n]='\0'; //remove \n and use \0

        if(strncmp(buf,MSG_QUIT,strlen(MSG_QUIT))==0){ break; } //EXIT IF I RECEIVE QUIT COMMAND

	else if(strncmp(buf,MSG_GET,strlen(MSG_GET))==0){ 
	
	sscanf(buf,"%s %s",garbage,fname);// retrieve the name of the file

	// protocol format
	buf[n]='\r';
	buf[n++]='\n';
	buf[n++]='\0';
	
	write(sockfd, buf, strlen(buf));  //send command without terminator

	trace ( err_msg("(%s) - data has been sent", prog_name) );

	FD_ZERO(&mysockets);
	FD_SET(sockfd, &mysockets);

	if ((n=select(FD_SETSIZE, &mysockets, NULL, NULL, &tv))==-1) {
		fprintf(stderr," select() - failed\n");
	}
	else if(n>0){
		nread=0;
		do {//TCP READ

			// NB: do NOT use Readline since it buffers the input
			read(sockfd, &c, sizeof(char));
			buf[nread++]=c;
		} while (c != '\n' && nread < MAXBUFL-1);
		buf[nread]='\0';  //i have read until \n --> i need to read again if i want to know size and timestamp

		while (nread > 0 && (buf[nread-1]=='\r' || buf[nread-1]=='\n')) {
			//remove \r \n
			buf[nread-1]='\0';
			nread--;
		}
		trace( err_msg("(%s) --- received string '%s'",prog_name, buf) );
		
		//MSG_OK defined as +OK

		if (nread >= strlen(MSG_OK) && strncmp(buf,MSG_OK,strlen(MSG_OK))==0) {
			//the server has sent a line formatted as the protocol

			
			sprintf(fnamestr, "copy_of_%s", fname);
			
		
			//read size_of_file that is still in the socket		
		
			n= read(sockfd, buf, 4);
			file_bytes = ntohl((*(uint32_t *)buf));
			trace( err_msg("(%s) --- received file size '%u'",prog_name, file_bytes) );

			//read time_stamp
			struct tm * timeinfo;
			int time;		
			n= read(sockfd, buf, 4);
			val_time= (*(uint32_t *)buf);
			time = ntohl(val_time);
			time_t time_format=time;
			timeinfo = localtime(&time_format);
			strftime (buf,MAXBUFL,"Last file modification: %Y-%m-%d %H:%M:%S.",timeinfo);
			trace( err_msg("(%s) --- received time information '%s'",prog_name, buf) );

		
			if ( (fp=fopen(fnamestr, "w+"))!=NULL) {
				
				printf("File copy status: open\n");
				
				for (i=0; i<file_bytes; i++) {// i know size of file--->i can read and stop right well (-1 EFO???)

					read(sockfd, &c, sizeof(char));
					fwrite(&c, sizeof(char), 1, fp);

					
				}
				

				n=fclose(fp);

				if(n!=0){ fprintf(stderr,"Error on close file! \n"); return -1;}

				else{

				trace( err_msg("(%s) --- received and wrote file '%s'",prog_name, fnamestr) );
				
				}

			} 
			else {
				trace( err_msg("(%s) --- cannot open file '%s'",prog_name, fnamestr) );
				return -1;
			}

		} 
		else {
			trace ( err_msg("(%s) - protocol error: received response '%s'", prog_name, buf) );
			return -1;
		}

	} 
	else {
		trace ( err_msg("Timeout waiting for an answer from server\n'") );
		
	}

}// if command== GET
else{ 

	printf("ATTENTION !!! INVALID COMMAND !!! ERROR !!! \n");}

}//close while to transfer files, exit only if the client write the QUIT command

	return 0;
}

		
       
