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
#define BUF_SIZE 500
#define MAXBUFL 255
#define MAX_STR 1023

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


int ftp (int connfd) {

	char buf[MAXBUFL+1]; /* +1 to make room for \0 */

	int ret_val = 0;



	while (1) {
		trace( err_msg("(%s) - waiting for commands ...",prog_name) );
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
		trace( err_msg("(%s) --- received string '%s'",prog_name, buf) );

		// get the command, compare n first characters 
		if (nread > strlen(MSG_GET) && strncmp(buf,MSG_GET,strlen(MSG_GET))==0) {
			char fname[MAX_STR+1];
			strcpy(fname, buf+4);// GET namefile.txt\r\n

			trace( err_msg("(%s) --- client asked to send file '%s'",prog_name, fname) );

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
					trace( err_msg("(%s) --- sent '%s' to client",prog_name, MSG_OK) );

					//send the file size as uint_32
					int size = info.st_size;
					uint32_t val_size = htonl(size);
					write (connfd, &val_size, sizeof(size));
					trace( err_msg("(%s) --- sent '%d' - converted in network order - to client",prog_name, size) );
					
					//send timestamp
					struct tm * timeinfo;
					int time= info.st_mtime;
					timeinfo = localtime(&info.st_mtime);
					uint32_t val_time = htonl(time);
					write (connfd, &val_time, sizeof(time));
					strftime (buf,MAXBUFL,"Last file modification: %Y-%m-%d %H:%M:%S.",timeinfo);
					trace( err_msg("(%s) --- sent '%s' - converted in network order - to client",prog_name, buf) );

					
					int i;
					char c;
					for (i=0; i<size; i++) {
						//read from file and send to the client
						fread(&c, sizeof(char), 1, fp);
						write (connfd, &c, sizeof(char));
					}
					trace( err_msg("(%s) --- sent file '%s' to client",prog_name, fname) );
					fclose(fp);
				} else {
					ret = -1;
				}
			}
			if (ret != 0) {	
				write (connfd, MSG_ERR, strlen(MSG_ERR) );
			}
		} else if (nread >= strlen(MSG_QUIT) && strncmp(buf,MSG_QUIT,strlen(MSG_QUIT))==0) {
			trace( err_msg("(%s) --- client asked to terminate connection", prog_name) );
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
	short port;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t cliaddrlen = sizeof(cliaddr);

	// for errlib to know the program name
	prog_name = argv[0];

	// check arguments 
	if (argc!=2)
		err_quit ("usage: %s <port>\n", prog_name);

	port=atoi(argv[1]);

	   struct addrinfo hints;
           struct addrinfo *result, *rp;
           int sfd, s;
           struct sockaddr_storage peer_addr;
           socklen_t peer_addr_len;
           ssize_t nread;
           char buf[BUF_SIZE];

           if (argc != 2) {
               fprintf(stderr, "Usage: %s port\n", argv[0]);
               exit(EXIT_FAILURE);
           }

          
          
	   struct sockaddr* sa= rp->ai_addr;		

	   if( rp->ai_family==AF_INET6){// IPV6
		fprintf(stdout,"IPV6 SERVER !!!\n");
		struct sockaddr_in6* sa_ipv6=(struct sockaddr_in6 *) sa;
		char straddr6[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &(sa_ipv6->sin6_addr), straddr6, sizeof(straddr6));
	        trace ( err_msg("(%s) Server ready on %s:%u", prog_name, straddr6, ntohs(sa_ipv6->sin6_port)) );
	   }
 	   else if(rp->ai_family==AF_INET){ // IPV4
		fprintf(stdout,"IPV4 SERVER !!!\n");
		struct sockaddr_in* sa_ipv4=(struct sockaddr_in *) sa;
		trace ( err_msg("(%s) connected to %s:%u", prog_name, inet_ntoa(sa_ipv4->sin_addr), ntohs(sa_ipv4->sin_port)) );
	   }
	
		freeaddrinfo(result);

	trace ( err_msg("(%s) socket created",prog_name) );
	trace ( err_msg("(%s) listening on %s:%u", prog_name, inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port)) );

	Listen(listenfd, LISTENQ);

	while (1) {
		trace( err_msg ("(%s) waiting for connections ...", prog_name) );

		connfd = Accept (listenfd, (SA*) &cliaddr, &cliaddrlen);
		trace ( err_msg("(%s) - new connection from client %s:%u", prog_name, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port)) );

		err = ftp(connfd);

		Close (connfd);
		trace( err_msg ("(%s) - connection closed by %s", prog_name, (err==0)?"client":"server") );
	}
	return 0;
}

