
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


int ftp_sender(int connfd) {

	char buf[MAXBUFL+1]; /* +1 to make room for \0 */

	int ret_val = 0;



	while (1) {
		printf("(Y) - waiting for commands...");
		int nread = 0; char c;
		do {
			// NB: do NOT use Readline (input buffered)
			int n = read(connfd, &c, sizeof(char));
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
		printf("(Y) - received string '%s'", buf);
		
		// get the command, compare n first characters 
		if (nread > strlen(MSG_GET) && strncmp(buf,MSG_GET,strlen(MSG_GET))==0) {
			char fname[MAX_STR+1];
			strcpy(fname, buf+4);// GET namefile.txt\r\n

			printf("(Y) - client asked to send file '%s'",fname);

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
				if ( (fp=fopen(fname, "r+")) != NULL) {
					
					//NB: strlen, not sizeof(MSG_OK), otherwise the '\0' byte will create problems when receiving data 
					write (connfd, MSG_OK, strlen(MSG_OK) );
					printf("(Y) - sent '%s' to client",MSG_OK);

					//send the file size as uint_32
					int size = info.st_size;
					uint32_t val_size = htonl(size);
					write (connfd, &val_size, sizeof(size));
					printf("(Y) - sent '%d' - converted in network order - to client", size);
					
					//send timestamp
					struct tm * timeinfo;
					int time= info.st_mtime;
					timeinfo = localtime(&info.st_mtime);
					uint32_t val_time = htonl(time);
					write (connfd, &val_time, sizeof(time));
					strftime (buf,MAXBUFL,"Last file modification: %Y-%m-%d %H:%M:%S.",timeinfo);
					printf("(Y) --- sent '%s' - converted in network order - to client", buf);

					
					int i;
					char c;
					for (i=0; i<size; i++) {
						//read from file and send to the client
						fread(&c, sizeof(char), 1, fp);
						write (connfd, &c, sizeof(char));
					}
					printf("(Y) --- sent file '%s' to client",fname);
					fclose(fp);
				} else {
					ret = -1;
				}
			}
			if (ret != 0) {	
				write (connfd, MSG_ERR, strlen(MSG_ERR) );
			}
		} else if (nread >= strlen(MSG_QUIT) && strncmp(buf,MSG_QUIT,strlen(MSG_QUIT))==0) {
			printf("(Y) --- client asked to terminate connection");
			ret_val = 1;
			break;
		} else {
			write (connfd, MSG_ERR, strlen(MSG_ERR) );
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

