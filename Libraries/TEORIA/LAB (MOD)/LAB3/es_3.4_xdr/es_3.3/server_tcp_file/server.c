#include <sys/stat.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>

#include <string.h>
#include <time.h>

#include "errlib.h"
#include "sockwrap.h"

#include "types.h"

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

// If client is inactive, after waiting for TIMEOUT seconds, close the connection and make the child process ready for another one
#define TIMEOUT 120

#define MAXCHILDS 10

char *prog_name;

char prog_name_with_pid[MAXBUFL];

int nchildren;
pid_t *pids;


static void sig_int(int signal) {
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


int receiver (int connfd) {

	char buf[MAXBUFL+1]; /* +1 to make room for \0 */
	int nread;
	int ret_val = 0;

	while (1) {
		trace( err_msg("(%s) - waiting for commands (max %d seconds) ...",prog_name, TIMEOUT) );
		
		fd_set rset;
		struct timeval tv;
		FD_ZERO(&rset);
		FD_SET(connfd, &rset);
		tv.tv_sec = TIMEOUT;
		tv.tv_usec = 0;
		if (Select(connfd+1, &rset, NULL, NULL, &tv) > 0) {
			int nread = 0; char c;
			do {
				// NB: do NOT use Readline since it buffers the input
				int n = read_timeout(connfd, &c, sizeof(char), 0);
				if (n==1)
					buf[nread++]=c;
				else
					break;
			} while (c != '\n' && nread < MAXBUFL-1);
			if (nread == 0)
				return 0;  // connection has been closed by client()
                
			/* append the string terminator after CR-LF */
			buf[nread]='\0';
			while (nread > 0 && (buf[nread-1]=='\r' || buf[nread-1]=='\n')) {
				buf[nread-1]='\0';
				nread--;
			}
			trace( err_msg("(%s) --- received string '%s'",prog_name, buf) );
                
			/* get the command */
			if (nread > strlen(MSG_GET) && strncmp(buf,MSG_GET,strlen(MSG_GET))==0) {
				char fname[MAX_STR+1];
				strcpy(fname, buf+4);
                
				trace( err_msg("(%s) --- client asked to send file '%s'",prog_name, fname) );
                
				struct stat info;
				int ret = stat(fname, &info);
				if (ret == 0) {
					FILE *fp;
					if ( (fp=fopen(fname, "r+")) != NULL) {
						int size = info.st_size;
						/* NB: strlen, not sizeof(MSG_OK), otherwise the '\0' byte will create problems when receiving data */
						int len = writen (connfd, MSG_OK, strlen(MSG_OK) );
						if (len != strlen(MSG_OK)) {
							trace( err_ret("(%s) --- writen() failed while sending '%s' message",prog_name, MSG_OK) );
							return 0;  // connection has been closed by client
						}
						trace( err_msg("(%s) --- sent '%s' to client",prog_name, MSG_OK) );

						uint32_t val = htonl(size);
						len = writen (connfd, &val, sizeof(uint32_t));
						if (len != sizeof(uint32_t)) {
							trace( err_ret("(%s) --- writen() failed while sending file size (4 bytes)",prog_name) );
							return 0;  // connection has been closed by client
						}
						
						trace( err_msg("(%s) --- sent '%d' - converted in network order - to client",prog_name, size) );
						
						//send timestamp
						struct tm * timeinfo;
						int time= info.st_mtime;
						timeinfo = localtime(&info.st_mtime);
						uint32_t val_time = htonl(time);
						len = writen (connfd, &val_time,  sizeof(uint32_t));
						if (len != sizeof(uint32_t)) {
							trace( err_ret("(%s) --- writen() failed while sending file size (4 bytes)",prog_name) );
							return 0;  // connection has been closed by client
						}
						strftime (buf,MAXBUFL,"Last file modification: %Y-%m-%d %H:%M:%S.",timeinfo);
						trace( err_msg("(%s) --- sent '%s' - converted in network order - to client",prog_name, buf) );


						int i;
						char c;
						for (i=0; i<size; i++) {
							fread(&c, sizeof(char), 1, fp);
							if ( (writen (connfd, &c, sizeof(char)))!= sizeof(char)) {
								trace( err_ret("(%s) --- writen() failed while sending file",prog_name) );
								fclose(fp);
								return 0;  // connection has been closed by client
							}
						}
						trace( err_msg("(%s) --- sent file '%s' to client",prog_name, fname) );
						fclose(fp);
					} else {
						ret = -1;
					}
				} else {
					// stat() failed, crash during ERR message format file
					trace( err_msg("(%s) --- stat() failed",prog_name) );
					int len = writen (connfd, MSG_ERR, strlen(MSG_ERR) );
					if (len != strlen(MSG_ERR)) {
						trace( err_ret("(%s) --- writen() failed while sending '%s' message",prog_name, MSG_OK) );
						return 0;  // connection has been closed by client
					}
				}
			} else if (nread >= strlen(MSG_QUIT) && strncmp(buf,MSG_QUIT,strlen(MSG_QUIT))==0) {
				trace( err_msg("(%s) --- client asked to terminate connection", prog_name) );
				return 1; // connection is closed by server in main(), after te request of client
			} else {
				//crash during ERR message, format message
				int len = writen (connfd, MSG_ERR, strlen(MSG_ERR) );
				if (len != strlen(MSG_ERR)) {
					trace( err_ret("(%s) --- writen() failed while sending '%s' message",prog_name, MSG_OK) );
					return 0; 
				}	
			}
		} else {
			// timeout from select() expired
			trace( err_msg("(%s) --- client did not answer for %d seconds: closing the connection", prog_name, TIMEOUT) );
			return 1; // connection is closed by server in main()
		}
	}
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
		err_msg("(%s) too many children requested, set deault max number equal to %d\n", prog_name,MAXCHILDS);
		nchildren=MAXCHILDS;
	}

	pids = calloc(nchildren, sizeof(pid_t));
	if (pids == NULL)
		err_quit("(%s) calloc() failed", prog_name);

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
		if ( (pids[i]= Fork()) > 0) {
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
				int ret = receiver(connfd);//PROTOCOL
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

