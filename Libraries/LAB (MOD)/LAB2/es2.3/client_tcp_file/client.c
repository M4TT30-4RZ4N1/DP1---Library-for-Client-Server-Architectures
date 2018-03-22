
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
#include "sock_manager.h"

#define MAXBUFL 255
#define MAX_STR 1023

#define MSG_OK "+OK"
#define MSG_GET "GET"
#define MSG_QUIT "QUIT"
#define MSG_ERR "-ERR"

#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif

char *prog_name;

int ftp_receiver(int sockfd){

	char fname[MAX_STR];
	char garbage[MAX_STR];
	char buf[MAXBUFL];
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

	printf("(Y) - data has been sent\n");

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
		printf("(Y) --- received string '%s'\n", buf);
		
		//MSG_OK defined as +OK

		if (nread >= strlen(MSG_OK) && strncmp(buf,MSG_OK,strlen(MSG_OK))==0) {
			//the server has sent a line formatted as the protocol

			
			sprintf(fnamestr, "copy_of_%s", fname);
			
		
			//read size_of_file that is still in the socket		
		
			n= read(sockfd, buf, 4);
			file_bytes = ntohl((*(uint32_t *)buf));
			printf("(Y) --- received file size '%u'\n",file_bytes);

			//read time_stamp
			struct tm * timeinfo;
			int time;		
			n= read(sockfd, buf, 4);
			val_time= (*(uint32_t *)buf);
			time = ntohl(val_time);
			time_t time_format=time;
			timeinfo = localtime(&time_format);
			strftime (buf,MAXBUFL,"Last file modification: %Y-%m-%d %H:%M:%S.\n",timeinfo);
			printf("(Y) --- received time information '%s'", buf);

		
			if ( (fp=fopen(fnamestr, "w+"))!=NULL) {
				
				printf("File copy status: open\n");
				
				for (i=0; i<file_bytes; i++) {// i know size of file--->i can read and stop right well (-1 EFO???)

					read(sockfd, &c, sizeof(char));
					fwrite(&c, sizeof(char), 1, fp);

					
				}
				

				n=fclose(fp);

				if(n!=0){ printf("Error on close file! \n"); return -1;}

				else{

					printf("(Y) --- received and wrote file '%s'", fnamestr);
					return 0;
				
				}

			}else {
				printf("(Y) --- cannot open file '%s'\n", fnamestr);
				return -1;
			}

		} //NOT OK
		else {
			printf("(Y) - protocol error: received response '%s'\n",  buf);
			return -1;
		}
	} 
	else {//TIMEOUT---BLOCCATO
		printf("Timeout waiting for an answer from server\n'");
		
	}

	}// if command== GET
	else{ 

		printf("ATTENTION !!! INVALID COMMAND !!! ERROR !!! \n");}
		return -1;
	}//close while to transfer files, exit only if the client write the QUIT command
	
	return 0;
}


int main (int argc, char *argv[]) {

	int sockfd, control;
	char dest_h[MAXBUFL], dest_p[MAXBUFL];
	struct sockaddr_in destaddr;
	struct sockaddr_in *solvedaddr;
	struct addrinfo *list;
	


	/* for errlib to know the program name */
	prog_name = argv[0];

	/* check arguments */
	if (argc!=3)
		err_quit ("usage: %s <dest_host> <dest_port>", prog_name);
	strcpy(dest_h,argv[1]);
	strcpy(dest_p,argv[2]);

	control=getaddrinfo(dest_h, dest_p, NULL, &list);

	if(control!=0){ err_quit ("error resolve address", prog_name);}

	solvedaddr = (struct sockaddr_in *)list->ai_addr;

	/* create socket */
	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockfd==-1){
		printf("(Y) --- error: protocoln");
	}
	/* specify address to bind to */
	memset(&destaddr, 0, sizeof(destaddr));
	destaddr.sin_family = AF_INET;
	destaddr.sin_port = solvedaddr->sin_port;
	destaddr.sin_addr.s_addr = solvedaddr->sin_addr.s_addr;

	printf("(Y) --- socket created\n" );

	control= connect( sockfd, (struct sockaddr *)&destaddr, sizeof(destaddr) );
	if(control==-1){
		printf("(Y) --- error: %s", strerror(errno));
	}

	printf("(Y) connected to %s:%u\n", inet_ntoa(destaddr.sin_addr), ntohs(destaddr.sin_port));


	control=ftp_receiver(sockfd);
	if(control==-1){
		printf("(Y) --- error: protocol\n");
	}


//for no memory leaks
	freeaddrinfo(list);
	
	Close (sockfd);

	return 0;
}

