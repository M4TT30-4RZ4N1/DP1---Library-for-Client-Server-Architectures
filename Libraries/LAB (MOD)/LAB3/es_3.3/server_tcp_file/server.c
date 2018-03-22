#include <sys/stat.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>


#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>

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

// If client is inactive, after waiting for TIMEOUT seconds, close the connection and make the child process ready for another one
#define TIMEOUT 120

#define MAXCHILDS 10

char *prog_name;

char prog_name_with_pid[MAXBUFL];

int nchildren;
pid_t *pids;


static void sig_int(int signal) {//terminate childs when the parent is terminated
	int i;
	err_msg ("(%s) info - sig_int() called", prog_name);
	for (i=0; i<nchildren; i++)
		kill(pids[i], SIGTERM);

	while( wait(NULL) > 0);  // wait for all children
		

	if (errno != ECHILD)
		err_quit("(%s) error: wait() error");

	exit(0);
}

static int read_timeout(int fd, void *ptr, size_t n, int timeout_sec) {//read each signle char with timeout

	fd_set rset;
	struct timeval tv;
	FD_ZERO(&rset);
	FD_SET(fd, &rset);
	tv.tv_sec = timeout_sec;
	tv.tv_usec = 0;

	int nread = -1;
	if (Select(fd+1, &rset, NULL, NULL, &tv) > 0) {
		nread = readn(fd, ptr, n);	
	}
	return nread;
}

int ftp_sender(int mysocket, char* prog_name) {

	char buf[CHUNK+1]; /* +1 to make room for \0 */

	int ret_val = 0;



	while (1) {
		printf("(%s) - waiting for commands...\n",prog_name);
		printf("\n=====================================\n");
		int nread = 0; char c;
		fd_set rset;
		struct timeval tv;
		FD_ZERO(&rset);
		FD_SET(mysocket, &rset);
		tv.tv_sec = TIMEOUT;
		tv.tv_usec = 0;
		
		if (Select(mysocket+1, &rset, NULL, NULL, &tv) > 0) {
			do {
				// NB: do NOT use Readline (input buffered)
				int n = read_timeout(connfd, &c, sizeof(char), 0);   //READ WITH TIMEOUT EACH CHAR
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
			printf("(%s) - received string '%s'\n",prog_name, buf);
		
			// get the command, compare n first characters 
			if (nread > strlen(MSG_GET) && strncmp(buf,MSG_GET,strlen(MSG_GET))==0){
				char fname[MAX_STR+1];
				char fname_print[MAX_STR+1];//need to print out the name after open the file
				strcpy(fname, buf+4);// GET namefile.txt\r\n
				strcpy(fname_print, fname);
				printf("(%s) - client asked to send file '%s'\n",prog_name, fname);

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
						write (mysocket, MSG_OK, strlen(MSG_OK) );
						printf("(%s) - sent to the client: %s \n",prog_name,MSG_OK);

						//send the file size as uint_32
						int file_size = info.st_size;
						uint32_t val_size = htonl(file_size);
						write(mysocket, &val_size, sizeof(file_size));
						printf("(%s) - sent '%d' - converted in network order - to client\n", prog_name, file_size);
					
						//send timestamp
						struct tm * timeinfo;
						int time= info.st_mtime;
						timeinfo = localtime(&info.st_mtime);
						uint32_t val_time = htonl(time);
						write(mysocket, &val_time, sizeof(time));
						strftime (buf,MAXBUFL,"Last file modification: %Y-%m-%d %H:%M:%S.\n",timeinfo);
						printf("(%s) - sent '%s' - converted in network order - to client\n", prog_name, buf);

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
						printf("(%s) --- sent file '%s' to client\n",prog_name, fname_print);
						fclose(fp);
					
					} else {
						ret = -1;
					}
				}
				if (ret != 0) {	
						// stat() failed, crash during ERR message format file
						trace( err_msg("(%s) --- stat() failed",prog_name) );
						int len = writen (mysocket, MSG_ERR, strlen(MSG_ERR) );
						if (len != strlen(MSG_ERR)) {
							trace( err_ret("(%s) --- writen() failed while sending '%s' message",prog_name, MSG_OK) );
							return 0;  // connection has been closed by client
				}
			} 
			
			}else if (nread >= strlen(MSG_QUIT) && strncmp(buf,MSG_QUIT,strlen(MSG_QUIT))==0) {
				printf("(%s) --- client asked to terminate connection\n", prog_name);
				ret_val = -1;
				break;
			} else {
				write (mysocket, MSG_ERR, strlen(MSG_ERR) );
				ret_val = -1;
				break;
			}
	}else {
			// timeout from select() expired
			trace( err_msg("(%s) --- client did not answer for %d seconds: closing the connection", prog_name, TIMEOUT) );
			return 1; // connection is closed by server in main()
		}
}
	
	
	printf("=====================================\n");
	return ret_val;
	
}



int main (int argc, char *argv[]) {

	int listenfd, connfd, err=0;
	short port;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t cliaddrlen = sizeof(cliaddr);
	struct sigaction action;
	int sigact_res;
	socklen_t addrlen;
	int i;
	int option;
	
	/* for errlib to know the program name */
	prog_name = argv[0];

	/* check arguments */
	if (argc!=3)
		err_quit ("usage: %s <port> <#children>\n", prog_name);

	port=atoi(argv[1]);
	nchildren=atoi(argv[2]);

	if (nchildren>MAXCHILDS){
		err_msg("(%s) too many children requested, set default max number equal to %d\n", prog_name,MAXCHILDS);
		nchildren=MAXCHILDS;
	}

	/* create socket */
	listenfd = Socket(AF_INET, SOCK_STREAM, 0);


	/* Manipulation socket: 

		-SOL_SOCKET: mean at API LEVEL
		-SOL_TCP: mean at TCP LEVEL
		-SOCKET_OPTION:{
			-SO_REUSEADDR: to reuse same port for FTP protocol
			-SO_KEEPALIVE: periodical transmission TCP to check connection, if it fail connection closed
			-SO_LINGER:specifies what should happen when the socket of a type that promises reliable delivery still has untransmitted messages when it is closed;
			- TCP_KEEPIDLE:  The time (in seconds) the connection needs to remain idle before TCP starts sending keepalive probes, if the socket option SO_KEEPALIVE has been set on this socket
			-TCP_KEEPCNT:The maximum number of keepalive probes TCP should send before dropping the connection.  This option should not be used in code intended to be portable
			-TCP_KEEPINTVL:The time (in seconds) between individual keepalive probes.This option should not be used in code intended to be portable
			}
		-OPTION is used to refer to the configuration, number of something...

	   On success, zero is returned. On error, -1
	*/
		

	/* This is needed only for multiple bind to the same address and port, or for bind after the server crashes, not to wait the timeout */
	/* Here, bind is performed in the parent process, thus this is not needed */
	/* However, it is useful to immediately restart the server if it closed while a connection is active */
	option = 1;
	int ret_s = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
	if (ret_s < 0)
		err_quit("(%s) setsockopt() failed", prog_name);	


	/* Example on how to set KEEPALIVE parameters in case the connection is IDLE */
	/* This solves the problem ONLY in case the link between server and client is broken and no activity is detected on a socket for a while */
	/* This does NOT solve the case when the server is sending data to client and it is waiting for ACK on the sent data. Default waiting time is about 9 minutes */
	/* The latter case requires the use of an application-level client-server heartbeat function using, for instance, OOB data */

	/* Set the socket using KEEPALIVE, reduce KEEPALIVE time, number of KEEPALIVE probes and interval between them */
	option = 1;
        ret_s = setsockopt(listenfd, SOL_SOCKET, SO_KEEPALIVE, &option, sizeof(option));
	if (ret_s < 0)
		err_quit("(%s) setsockopt( SO_KEEPALIVE ) failed", prog_name);	

	option = 4; // 4 seconds keepalive
        ret_s = setsockopt(listenfd, SOL_TCP, TCP_KEEPIDLE, &option, sizeof(option));
	if (ret_s < 0)
		err_quit("(%s) setsockopt( TCP_KEEPIDLE ) failed", prog_name);	

	option = 2; // 2 tries after keepalive not received
        ret_s = setsockopt(listenfd, SOL_TCP, TCP_KEEPCNT, &option, sizeof(option));
	if (ret_s < 0)
		err_quit("(%s) setsockopt( TCP_KEEPCNT ) failed", prog_name);	

	option = 3; // 3 seconds timeout for each re-try
        ret_s = setsockopt(listenfd, SOL_TCP, TCP_KEEPINTVL, &option, sizeof(option));
	if (ret_s < 0)
		err_quit("(%s) setsockopt( TCP_KEEPINTVL ) failed", prog_name);	


	/* specify address to bind to */
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl (INADDR_ANY);

	Bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	
	Listen(listenfd, LISTENQ);

	trace ( err_msg("(%s) server started\n\n", prog_name) );

	for (i=0; i<nchildren; i++) {
		if ( Fork() > 0) {
			/* parent */
		} else {
			/* child */
			int childpid = getpid();
			sprintf(prog_name_with_pid, "%s child %d", prog_name, childpid);
			prog_name = prog_name_with_pid;
			trace ( err_msg(" (%s) child %d starting\n", prog_name, childpid) );

			while (1) {
				struct sockaddr_in cliaddr;
				socklen_t clilen;

				clilen = sizeof(struct sockaddr_in);
				//cliaddr = malloc( clilen );
				//if (cliaddr == NULL)
				//	err_quit("(%s) malloc() failed", prog_name);

				//each Child is on a passive socket and the kernel resolve who will accept the request
				connfd = Accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
				trace ( err_msg("(%s) - new connection from client %s:%u\n", prog_name, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port)) );
				
				int ret= ftp_sender(connfd,prog_name);//PROTOCOL
				
				if (ret == 0) {
					err_msg("(%s) - connection terminated by client\n", prog_name);
				} else {
					err_msg("(%s) - connection terminated by server\n", prog_name);
				}
				Close(connfd);
			}
		}
		

	}
	
	
			
	/* This is to capture CTRL-C and terminate children */	
	memset(&action, 0, sizeof (action));
	action.sa_handler = sig_int;
	sigact_res = sigaction(SIGINT, &action, NULL);
	if (sigact_res == -1)
		err_quit("(%s) sigaction() failed", prog_name);

	while(1) {
		pause();
	}
	

	
	return 0;
}

