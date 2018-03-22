
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <string.h>
#include <time.h>

#include "errlib.h"
#include "sockwrap.h"

#define LISTENQ 15
#define MAXBUFL 255
#define CHUNK 1024

#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif

#define MSG_ERR "-ERR\r\n"
#define MSG_OK  "+OK\r\n"
#define MSG_QUIT  "QUIT"
#define MSG_GET "GET"

#define MAX_STR 1023

char *prog_name;


int ftp_sender(int mysocket) {

	char buf[MAXBUFL+1]; /* +1 to make room for \0 */

	int ret_val = 0;



	while (1) {
		printf("(Y) - waiting for commands...\n");
		int nread = 0; char c;
		do {
			// NB: do NOT use Readline (input buffered)
			int n = read(mysocket, &c, sizeof(char));
			if (n==1)
				buf[nread++]=c;
			else
				break;
		} while (c != '\n' && nread < MAXBUFL-1);

		if (nread == 0)
			return 0;
		
		buf[nread]='\0'; //cause i read until \n

		// terminate string buf before : remove CR-LF 
		while (nread > 0 && ((buf[nread-1]=='\r' || buf[nread-1]=='\n'))) {
			buf[nread-1]='\0';
			nread--;
		}
		printf("(Y) - received string '%s'\n", buf);
		
		// get the command, compare n first characters 
		if (nread > strlen(MSG_GET) && strncmp(buf,MSG_GET,strlen(MSG_GET))==0) {
			char fname[MAX_STR+1];
			char file_name[MAX_STR+1];
			strcpy(fname, buf+4);// GET namefile.txt\r\n
			strcpy(file_name,fname);
			printf("(Y) - client asked to send file '%s'\n",fname);

			struct stat info;
			int ret = stat(fname, &info); //stat return the info about the file                ret

//---------------------------------------------------------------------------------------------------------------------
/* struct stat info;

 dev_t       st_dev;     ID del dispositivo contenente file  
 ino_t       st_ino;     numero dell'inode 
 mode_t      st_mode;    protezione  
 nlink_t     st_nlink;   numero di hard links 
 uid_t       st_uid;     user ID del proprietario 
 gid_t       st_gid;     group ID del proprietario 
 dev_t       st_rdev;    ID del dispositivo (se il file speciale) 
 off_t       st_size;    dimensione totale, in byte  
 blksize_t   st_blksize; dimensione dei blocchi di I/O del filesystem 
 blkcnt_t    st_blocks;  numero di blocchi assegnati  
 time_t      st_atime;   tempo dell'ultimo accesso 
 time_t      st_mtime;   tempo dell'ultima modifica 
 time_t      st_ctime;   tempo dell'ultimo cambiamento 

*/
//----------------------------------------------------------------------------------------------------------------------

			


			if (ret == 0) {//success

				FILE *fp;
				if ( (fp=fopen(fname, "r+")) != NULL) {//fname deprecated
					
					//NB: strlen, not sizeof(MSG_OK), otherwise the '\0' byte will create problems when receiving data 
					write (mysocket, MSG_OK, strlen(MSG_OK) );
					printf("(Y) - sent to the client: %s \n",MSG_OK);

					//send the file size as uint_32
					int file_size = info.st_size;
					uint32_t val_size = htonl(file_size);
					write(mysocket, &val_size, sizeof(file_size));
					printf("(Y) - sent '%d' - converted in network order - to client\n", file_size);
					
					//send timestamp
					struct tm * timeinfo;
					int time= info.st_mtime;
					timeinfo = localtime(&info.st_mtime);
					uint32_t val_time = htonl(time);
					write(mysocket, &val_time, sizeof(time));
					strftime (buf,MAXBUFL,"Last file modification: %Y-%m-%d %H:%M:%S.\n",timeinfo);
					printf("(Y) --- converted in network order and sent to client: %s\n", buf);

					//INVIO FILE A BLOCCHI
					int size_buf;
					
					//per identificare se file grande o piccolo
					if(file_size > CHUNK)
						size_buf=CHUNK;
					else
						size_buf=file_size;
						
					
					 while((nread = fread(buf, sizeof(char), size_buf, fp)) > 0) {

						writen(mysocket, buf, nread);
/*
						if(sigpipe) {
						  printf("CHILD %d - Client closed socket\n", pid);
						  receive_requests = 0;
						  fclose(file);
						  break;
						}
*/
						file_size= file_size-nread;

						if(file_size > CHUNK)
							size_buf=CHUNK;
						else
							size_buf=file_size; //(ultimo blocco da leggere e scrivere)

      					}
					printf("(Y) --- sent file '%s' to client\n",file_name);
					fclose(fp);
					
				} else {
					ret = -1;
				}
			}
			if (ret != 0) {	
				write (mysocket, MSG_ERR, strlen(MSG_ERR) );
			}
		} else if (nread >= strlen(MSG_QUIT) && strncmp(buf,MSG_QUIT,strlen(MSG_QUIT))==0) {
			printf("(Y) --- client asked to terminate connection\n");
			ret_val = 1;
			break;
		} else {
			write (mysocket, MSG_ERR, strlen(MSG_ERR) );
		}

	}
	return ret_val;
}


int main (int argc, char *argv[]) {

	int listenfd, connfd, err=0;
	int control=0;
	short port;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t cliaddrlen = sizeof(cliaddr);

	// for errlib to know the program name
	prog_name = argv[0];

	// check arguments 
	if (argc!=2)
		err_quit ("usage: %s <port>\n", prog_name);
	port=atoi(argv[1]);

	// create socket 
	control=listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(control==-1){
		printf("Errore: %s\n",strerror(errno));
	}

	// specify address to bind to
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl (INADDR_ANY);

	control=bind(listenfd, (SA*) &servaddr, sizeof(servaddr));
	if(control==-1){
		printf("Errore: %s\n",strerror(errno));
	}

	printf("(Y) --- socket created\n");
	printf("(Y) --- listening on %s:%u\n",inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port) );

	control=listen(listenfd, LISTENQ);
	if(control==-1){
		printf("Errore: %s\n",strerror(errno));
	}

	while (1) {
		printf("(Y) --- waiting for connections ...\n" );

		connfd = accept (listenfd, (SA*) &cliaddr, &cliaddrlen);
		if(connfd==-1){
			printf("Errore: %s\n",strerror(errno));
		}
		printf("(Y) --- new connection from client %s:%u\n",inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port) );

		err = ftp_sender(connfd);

		control=close (connfd);
		if(control==-1){
			printf("Errore: %s\n",strerror(errno));
		}
		printf("(Y) --- connection closed by %s",(err==0)?"client":"server");
	}
	return 0;
}

