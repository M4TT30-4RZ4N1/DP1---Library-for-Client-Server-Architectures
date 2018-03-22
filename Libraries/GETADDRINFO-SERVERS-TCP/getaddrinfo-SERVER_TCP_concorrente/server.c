#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string.h>
#include <time.h>


#include "errlib.h"
#include "sockwrap.h"
#include "ftp.h"

#define LISTENQ 15
#define MAXBUFL 255
#define CHUNK 1024

#define TCP "TCP"
#define UDP "UDP"

#define MSG_ERR "-ERR\r\n"
#define MSG_OK  "+OK\r\n"
#define MSG_QUIT  "QUIT"
#define MSG_GET "GET"

#define MAX_STR 1023

char *prog_name;
int sigpipe;
char prog_name_with_pid[MAXBUFL];

#define MAX_CHILDREN 3
int n_children; /* number of children still to be waited for */

/* The signal handler is used to wait on zombie processes, i.e. those terminated
   while waiting for the Accept to return */
/* The difficulty is that sig_chld is not guaranteed to get one single signal for each
   terminated child process: thus, cycle on waitpid() until it returns > 0 */
void sigpipeHndlr(int signal) {
  	printf("SIGPIPE captured!\n");
  	sigpipe = 1;
  	return;
}

static void sig_chld(int signal) {

	err_msg ("(%s) info - sig_chld() called", prog_name);
	err_msg("(%s) - n_children not 'waited' = #%d", prog_name, n_children);
	int status;
	int pid;

	//-1 --> wait any pid, WNOHANG means that is not blocked on wait it return always something (fail or not)
	while ( (pid = waitpid(-1, &status, WNOHANG) ) > 0) {
		err_msg("(%s) info - waitpid returned PID %d", prog_name, pid);
		n_children--;
		err_msg("(%s) - n_children not 'waited' = #%d", prog_name, n_children);
	}
	
	printf("\n=====================================\n");
	return;
}

int server_protocol(int mysocket, char* progname);

int server_protocol(int mysocket, char* progname){//TODO: AGGIUNGI PARAMETRI PER IL PROTOCOLLO

	printf("(%s) --- Start Application ---\n", progname);
	
	return 0;
}

int main(int argc, char *argv[]) {

	//signal(SIGINT, SIG_IGN); //no close on ctrl+c
	
	int listenfd, connfd, err=0;
	//short port;
	struct sockaddr_in clientaddr;
	socklen_t addrlen=sizeof(clientaddr);
	pid_t childpid;
	struct sigaction action;
	int sigact_res;
	
	// for errlib to know the program name 
	prog_name = argv[0];

	// check arguments
	if (argc!=2)
		err_quit ("usage: %s <port>\n", prog_name);
	//port=atoi(argv[1]);

	//clear server struct
      	listenfd= serverConnectedTCP(argv[1]); //port 
      	
     	 /*
     	 if i call in the main():
     	 sd= init_Server(UDP, &serveraddr);
      	 NB: [NO LISTEN, NO ACCEPT]
      	 */   

	err_msg("(%s) socket created %d \n",prog_name, listenfd);
	
	 if (listen(listenfd,LISTENQ) < 0){
         	perror("listen() failed");
         	return -1;
      	}

        printf("(%s) Ready for client connect().\n",prog_name);

	signal(SIGPIPE, sigpipeHndlr);

	//personalized action associated with SIGCHLD
	memset(&action, 0, sizeof (action));
	action.sa_handler = sig_chld;
	sigact_res = sigaction(SIGCHLD, &action, NULL);
	if (sigact_res == -1)
		err_quit("(%s) sigaction() failed", prog_name);

	n_children = 0;

	while (1) {
// Avoid a race-condition--> child terminates after: (if n_children == MAX_CHILDREN) but before: (wait()) -
// to avoid the wait() being blocked incorrectly because the children has already been waited in the signal handler, the SIGCHLD signal is masked before entering the if, and re-enabled after the if

		//MASK SIGNAL HANDLER
		sigset_t x;
		sigemptyset (&x);
		sigaddset(&x, SIGCHLD);
		sigprocmask(SIG_BLOCK, &x, NULL);

		if ( n_children == MAX_CHILDREN) {
			/*perform a blocking wait - there are MAX_CHILDREN around 
			if one is zombie, this means that at least a new connection can be accepted 
			wait() eliminates the zombie, then 
			in this case, call nonblocking wait() until all zombies have been eliminated 
			this solution leaves some zombies around during the execution of the program,
			because children may terminate while the process is waiting for the accept() to return 
			this behavior is avoided using SIGCHLD */
			err_msg("(%s) - max number of children reached: NO accept now", prog_name);
			err_msg("(%s) - calling blocking system call wait() in order to wait for a child termination", prog_name);
			printf("\n=====================================\n");

// This is to test a possible race-condition: child terminates after if but before wait() - to avoid the wait() being blocked incorrectly because the children has already been waited in the signal handler, the SIGCHLD signal is masked before entering the if, and re-enabled after the if
			err_msg("(%s) - sleeping 10 seconds before calling wait()", prog_name);
			int sec = 10;
			while (sec!=0) {  // Need to test return value, if != 0 sleep syscall has been interrupted by a signal
				sec = sleep(sec);
			}
			err_msg("(%s) - sleeping ENDED", prog_name);
			printf("\n=====================================\n");

			int status;
			// wait-- blocking for a children to terminate when i have full child counter
			int wpid = waitpid(-1, &status, 0);
			err_msg ("(%s) waitpid() blocking returned child PID %d ...", prog_name, wpid);
			n_children--;	
			err_msg("(%s) - n_children not 'waited' = #%d", prog_name, n_children);
			
			printf("\n=====================================\n");
		}
// Re-enable the reception of the SIGCHLD signal. The signal  is handled only after sigprocmask re-enables it.

		sigemptyset (&x);
		sigaddset(&x, SIGCHLD);
		sigprocmask(SIG_UNBLOCK, &x, NULL);

		/* When a child is terminated, the previous blocking waitpid returns, and the signal handler is invoked 
		Other terminated children are waited for in the signal handler, in case there are some 
		The code in the signal handler always perform a nonblocking wait 
		The signal handler is useful to wait for children while the parent is in the Accept, so no zombies are left around */
		
		err_msg ("(%s) waiting for connections ...", prog_name);
		// NB: the Accept must be used because it correctly handles the SIGCHLD signal 
		connfd = Accept (listenfd,  NULL, NULL);
		
		sigpipe = 0;
		
		print_client_ipv4_address_tcp(connfd,prog_name,&clientaddr, &addrlen);

		err_msg("(%s) - fork() to create a child", prog_name);
		
		if ( (childpid = Fork()) == 0) {
			// Child 
			int cpid = getpid();
			sprintf(prog_name_with_pid, "%s child %d", prog_name, cpid);
			prog_name = prog_name_with_pid;
			Close(listenfd);
			
			struct sockaddr_in client_child;
			socklen_t addr_len=sizeof(client_child);
			char client_ip[MAX_STR], client_port[MAX_STR];
			
			retrieve_client_ip4_port(connfd,prog_name,&client_child, &addr_len,client_ip,client_port);
			printf("(%s) - client -> %s:%s \n", prog_name,client_ip,client_port);
			
			
//TODO:START PROTOCOL SECTION			
			err = ftp_sender(connfd, prog_name);   //protocol
			
			if(err!=0){
				err_msg("(%s) - Error : protocol\n", prog_name);
			}	
//END PROTOCOL SECTION			
			
						
			exit(0);//i need cause i no do
		} else {
			// Parent 
			Close (connfd);
		//err_msg ("(%s) - connection closed by %s", prog_name, (err==0)?"client":"server");
		
			n_children++;
			err_msg("(%s) - n_children not 'waited': #%d\n", prog_name, n_children);
			printf("=====================================\n");
		}
		
		
		
	}//end while

	return 0;
}

