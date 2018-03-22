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
#include "ftp.h"

#define LISTENQ 15
#define MAXBUFL 255
#define CHUNK 1024

#define MAX_STR 1023

// If client is inactive, after waiting for TIMEOUT seconds, close the connection and make the child process ready for another one
#define TIMEOUT 120

#define MAXCHILDS 10

char *prog_name;

char prog_name_with_pid[MAXBUFL];
int sigpipe;
int nchildren;
pid_t pids[MAXCHILDS];

void sigpipeHndlr(int signal) {
  	printf("SIGPIPE captured!\n");
  	sigpipe = 1;
  	return;
}

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

int server_protocol(int mysocket, char* progname);

int server_protocol(int mysocket, char* progname){//TODO: AGGIUNGI PARAMETRI PER IL PROTOCOLLO

	printf("(%s) --- Start Application ---\n", progname);
	
	return 0;
}

int main (int argc, char *argv[]) {

	int listenfd, connfd,err=0;
	//short port;
	struct sigaction action;
	int sigact_res;
	int i;
	int option;
	
	/* for errlib to know the program name */
	prog_name = argv[0];

	/* check arguments */
	if (argc!=3)
		err_quit ("usage: %s <port> <#children>\n", prog_name);

	//port=atoi(argv[1]);
	nchildren=atoi(argv[2]);

	if (nchildren>MAXCHILDS){
		err_msg("(%s) too many children requested, set default max number equal to %d\n", prog_name,MAXCHILDS);
		nchildren=MAXCHILDS;
	}

	/* create socket */
	listenfd = serverConnectedTCP (argv[1]); //port  


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

	
	Listen(listenfd, LISTENQ);
	
	signal(SIGPIPE, sigpipeHndlr);

	err_msg("(%s) server started\n\n", prog_name);

	for (i=0; i<nchildren; i++) {
		if ( Fork() > 0) {
			/* parent */
		} else {
			/* child */
			int childpid = getpid();
			pids[i]=childpid;
			sprintf(prog_name_with_pid, "%s child %d", prog_name, childpid);
			prog_name = prog_name_with_pid;
			err_msg(" (%s) child %d starting\n", prog_name, childpid);

			while (1) {
				struct sockaddr_in clientaddr;
				socklen_t addrlen;

				addrlen = sizeof(struct sockaddr_in6);
				//cliaddr = malloc( clilen );
				//if (cliaddr == NULL)
				//	err_quit("(%s) malloc() failed", prog_name);
		
				//each Child is on a passive socket and the kernel resolve who will accept the request
				connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &addrlen);
				
				sigpipe = 0;
				
				char client_ip[16], client_port[16];
				
				print_client_ipv4_address_tcp(connfd,prog_name, &clientaddr, &addrlen);
				
				retrieve_client_ip4_port(connfd,prog_name,&clientaddr,&addrlen,client_ip,client_port);
				
				printf("(%s)- client ip: %s port: %s\n", prog_name, client_ip,client_port);
/*TODO: PROTOCOL*/				
				err= ftp_sender(connfd,prog_name);//PROTOCOL
				
				if (err == 0) {
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

