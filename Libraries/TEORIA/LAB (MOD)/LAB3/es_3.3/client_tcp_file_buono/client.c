
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <string.h>
#include <time.h>

#include "errlib.h"
#include "sockwrap.h"

#define MAXBUFL 255
#define MAX_STR 1023

#define MSG_GET "GET"
#define MSG_OK "+OK"
#define MSG_ERR "-ERR"
#define MSG_Q "Q"
#define MSG_A "A"

#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif

char *prog_name;

int main (int argc, char *argv[]) {

	int sockfd, err=0;
	char *dest_h, *dest_p;
	struct sockaddr_in destaddr;
	struct sockaddr_in *solvedaddr;
	char buf[MAXBUFL];
	struct addrinfo *list;
	int err_getaddrinfo;


	// for errlib to know the program name 
	prog_name = argv[0];

	// check arguments
	if (argc!=3)
		err_quit ("usage: %s <dest_host> <dest_port>", prog_name);
	dest_h=argv[1];
	dest_p=argv[2];

	getaddrinfo(dest_h, dest_p, NULL, &list);
	solvedaddr = (struct sockaddr_in *)list->ai_addr;

	// create socket 
	sockfd = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// specify address to bind to 
	memset(&destaddr, 0, sizeof(destaddr));
	destaddr.sin_family = AF_INET;
	destaddr.sin_port = solvedaddr->sin_port;
	destaddr.sin_addr.s_addr = solvedaddr->sin_addr.s_addr;

	trace ( err_msg("(%s) socket created",prog_name) );

	Connect ( sockfd, (struct sockaddr *)&destaddr, sizeof(destaddr) );

	trace ( err_msg("(%s) connected to %s:%u", prog_name, inet_ntoa(destaddr.sin_addr), ntohs(destaddr.sin_port)) );

	

	char command[MAXBUFL];

	int flag_reading_file = 0;
	int flag_end = 0;
	char fnamestr[MAX_STR+1];
	int filecnt = 0;  // this is needed to generate a new filename for writing
	FILE *fp;  // need to keep the info between different iterations in the next while
	int file_ptr;  // need to keep the info between different iterations in the next while
	uint32_t file_bytes;  // need to keep the info between different iterations in the next while

	printf("Client ready, waiting for commands (GET file,Q,A):\n");

	while (1) {

		if (flag_reading_file == 0 && flag_end == 1) {
			break;
		}

		fd_set rset;
		FD_ZERO(&rset);
		FD_SET(sockfd, &rset);
		FD_SET(fileno(stdin), &rset);  // Set stdin
		/* sockfd+1 is the maximum since the others in which we are interested are 0,1,2 which are always less than sockfd */
		Select(sockfd+1, &rset, NULL, NULL, NULL); // This will block until something happens

/*SERVER*/	if (FD_ISSET(sockfd, &rset)) {
			// socket is readable

	
			if (flag_reading_file == 0) { //NO READING FILE
				int nread = 0; char c;
				do {
					// NB: do NOT use Readline since it buffers the input
					Read(sockfd, &c, sizeof(char));
					buf[nread++]=c;
				} while (c != '\n' && nread < MAXBUFL-1);
				
				buf[nread]='\0';

				printf("nread=%d\n", nread);
				trace( err_msg("(%s) --- received string '%s'",prog_name, buf) );
                        
				if (nread >= strlen(MSG_OK) && strncmp(buf,MSG_OK,strlen(MSG_OK))==0) {
					// Use a number, otherwise the requested filenames should be remembered
					sprintf(fnamestr, "down_%d", filecnt);
					filecnt++;
					
					//read file_dimension
					int n = Read(sockfd, buf, 4);
					file_bytes = ntohl((*(uint32_t *)buf));
					trace( err_msg("(%s) --- received file size '%u'",prog_name, file_bytes) );

					//read time_stamp
					uint32_t val_time;
					struct tm * timeinfo;
					int time;		
					n= Read(sockfd, buf, 4);
					val_time= (*(uint32_t *)buf);
					time = ntohl(val_time);
					time_t time_format=time;
					timeinfo = localtime(&time_format);
					strftime (buf,MAXBUFL,"Last file modification: %Y-%m-%d %H:%M:%S.",timeinfo);
					trace( err_msg("(%s) --- received time information '%s'",prog_name, buf) );
                        		
					//file open
					if ( (fp=fopen(fnamestr, "w+"))!=NULL) {
						trace( err_msg("(%s) --- opened file '%s' for writing",prog_name, fnamestr) );
						file_ptr = 0;   //COUNTER THAT AT THE END OF TRANSFER MUST BE EQUAL TO SIZE OF THE FILE
						flag_reading_file = 1; //I' M READING!!!!
					} else {
						trace( err_quit("(%s) --- cannot open file '%s'",prog_name, fnamestr) );
					}
				} else if (nread >= strlen(MSG_ERR) && strncmp(buf,MSG_ERR,strlen(MSG_ERR))==0) {
					trace ( err_msg("(%s) - received '%s' from server: maybe a wrong request?", prog_name, buf) );
				} else {
					trace ( err_msg("(%s) - protocol error: received response '%s'", prog_name, buf) );
					break;
				}


			} else if (flag_reading_file==1) {  //READ OPERATION
				char c;
				Read (sockfd, &c, sizeof(char));
				fwrite(&c, sizeof(char), 1, fp);
				file_ptr++;
									
				//CLOSE FILE WITH CONTROL ON DIMENSION-- CAUSE IF I'WAITING SOME BYTES I CANNOT CLOSE
				if (file_ptr == file_bytes) {
					fclose(fp);
					trace( err_msg("(%s) --- received and wrote file '%s'",prog_name, fnamestr) );
					flag_reading_file = 0;
				}

			} else {
				trace ( err_msg("(%s) - flag_reading_file error '%d'", prog_name, flag_reading_file) );
				break;
			}
		}
/*KEYBOARD*/    if (FD_ISSET(fileno(stdin), &rset)) {

			
			// standard input is readable, fgets read until \n or EOF
			if ( (fgets(command, MAXBUFL, stdin)) == NULL) { 
				// Ask the server to terminate if EOF is sent from stdin
				strcpy(command,MSG_Q);
			}
			else if( (strncmp(command,MSG_GET,strlen(MSG_GET))==0) && (command[strlen(MSG_GET)]==' ')){

				trace( err_msg("(%s) --- GET command sent to server",prog_name) );
				// TODO: here send to server the GET filename
				Write(sockfd, command, strlen(command));

			}
			else if((strncmp(command,MSG_Q,strlen(MSG_Q))==0) && (strlen(MSG_A)==(strlen(command)-1))){

				printf("Q command received\n");
				// TODO: here quit only after the file was succesful transfered
									sprintf(command, "QUIT\n");
				Write(sockfd, command, strlen(command));
				//for no memory leaks
				freeaddrinfo(list);
				Shutdown(sockfd, SHUT_WR); //SHUT_WR disable send operation, i can still read and then the socket will be closed
				//SHUT_RDWR block read, write streaming equal to close
				flag_end = 1;
				err_msg("(%s) - waiting for file tranfer to terminate", prog_name);

			}
				
			else if((strncmp(command,MSG_A,strlen(MSG_A))==0) && (strlen(MSG_A)==(strlen(command)-1))){
				
				printf("A command received\n");
				// TODO: here quit during the file transfer
				//for no memory leaks
				freeaddrinfo(list);
				Close (sockfd);   // This will give "connection reset by peer at the server side
				err_quit("(%s) - exiting immediately", prog_name);
			}
			else{ 	
				trace( err_msg("(%s) --- INVALID COMMAND FORMAT, waiting for commands (GET file,Q,A):\n",prog_name) );
			}

// NB. i'm using strlen(command)-1 cause in the command i have the \n !!!
			
		}//keyboard
	}//while

	return 0;
}

