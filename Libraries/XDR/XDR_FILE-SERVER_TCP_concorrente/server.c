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
#include "types.h"

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
int xdr_enable=0;
char prog_name_with_pid[MAXBUFL];
int sigpipe;
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

int ftp_sender_xdr(int mysocket,char* progname){

	int stat_ret, i;
	struct stat info;
	FILE *fp;
	off_t fsize;
	time_t fupdatetime;
	
	// Streams comunicazione
    	XDR xdrs_in;
   	XDR xdrs_out;
    	FILE *stream_socket_r;
    	FILE *stream_socket_w;
    	char  *obuf;	   /* transmission buffer */
	
    	struct message message;
    	struct message response; // da provare statico
    
    	char c;
	
	xdrs_in.x_ops = NULL;
	xdrs_out.x_ops = NULL;
			
	printf("(%s) -  Server Application FTP-XDR_MODE start:\n",progname);
	while(1){
		
		stream_socket_r = fdopen(mysocket, "r");

		if (stream_socket_r == NULL){
			err_sys ("(%s) error - fdopen() failed", progname);
		}

		xdrstdio_create(&xdrs_in, stream_socket_r, XDR_DECODE);
		
		//PULIZIA STRUTTURA
		memset(&message, 0, sizeof(message));
		message.tag = 0;
		message.message_u.filename = NULL;
		message.message_u.fdata.contents.contents_len = 0;
		message.message_u.fdata.contents.contents_val = NULL;
		message.message_u.fdata.last_mod_time = 0;

		//RICEZIONE
		if(!xdr_message(&xdrs_in, &message)){
			printf("(%s) - ERROR (cannot read message with XDR)", progname) ;
			xdr_destroy(&xdrs_in);
			break;
		}else{
			printf("(%s) - Received a Message!\n", progname);
			printf("(%s) - Required file: %s\n", progname,message.message_u.filename);
			if(message.tag == GET){

				stat_ret = stat(message.message_u.filename, &info);
				if(stat_ret == 0){//stat() worked

					if( (fp = fopen(message.message_u.filename, "r")) != NULL){//file opened
						fsize = info.st_size;
						fupdatetime = info.st_mtime;
						//alloco in base alla dimensione e leggo file
						obuf = (char *) malloc(fsize*sizeof(char));
						for (i=0; i<fsize; i++) {
							fread(&c, sizeof(char), 1, fp);
							obuf[i] = c;
						}
						
						fclose(fp);
						//POPOLO MESSAGGIO DI RISPOSTA
						response.tag = OK;
						response.message_u.filename = message.message_u.filename;
						response.message_u.fdata.contents.contents_val = obuf;
						response.message_u.fdata.contents.contents_len = fsize;
						response.message_u.fdata.last_mod_time = fupdatetime;

						stream_socket_w = fdopen(mysocket, "w");

						xdrstdio_create(&xdrs_out, stream_socket_w, XDR_ENCODE);
						//RISPONDO
						if(!xdr_message(&xdrs_out, &response)){
							printf("(%s) - Error in transmitting data \n", progname);
							exit(1);
						}
						printf("(%s) - Transmission file done!\n", progname);
						fflush(stream_socket_w);
						xdr_destroy(&xdrs_out);
						free(obuf);//NO MEMORY LEAKS

					}else{ // error! file connot be opened
						printf("(%s) - File can not be opened \n", progname);
						stream_socket_w = fdopen(mysocket, "w");

						xdrstdio_create(&xdrs_out, stream_socket_w, XDR_ENCODE);

						response.tag = ERR;
						if(!xdr_message(&xdrs_out, &response)){
							printf("(%s) - Error in transmitting data \n", progname);
							exit(1);
						}
						fflush(stream_socket_w);
						xdr_destroy(&xdrs_out);
					}
				}else{
					printf("(%s) - File stat() failed\n", progname);
					stream_socket_w = fdopen(mysocket, "w");

					xdrstdio_create(&xdrs_out, stream_socket_w, XDR_ENCODE);

					response.tag = ERR;
					if(!xdr_message(&xdrs_out, &response)){
						printf("(%s) - Error in transmitting data \n", progname);
						exit(1);
					}
					fflush(stream_socket_w);
					xdr_destroy(&xdrs_out);
					break;
				}
			}else{ 
				if(message.tag == QUIT){
					printf("(%s) - The client asked to terminate \n",progname);
					Close(mysocket);
					return 0;
				}else{//error!
					printf("(%s) - An error occured\n", progname);
					stream_socket_w = fdopen(mysocket, "w");

					xdrstdio_create(&xdrs_out, stream_socket_w, XDR_ENCODE);

					response.tag = ERR;
					if(!xdr_message(&xdrs_out, &response)){
						printf("(%s) - Error in transmitting data \n", progname);
						exit(1);
					}
					fflush(stream_socket_w);
					xdr_destroy(&xdrs_out);
					break;
				}
			}
			//free(message);
		}
		//~ free(file);
		//~ free(tag);
		//~ free(response);
	}
	return 1;


}

int main(int argc, char *argv[]) {

	//signal(SIGINT, SIG_IGN); //no close on ctrl+c
	
	int listenfd, connfd, err=0;
	short port;
	struct sockaddr_in6 serveraddr, clientaddr;
	socklen_t addrlen=sizeof(clientaddr);
	pid_t childpid;
	struct sigaction action;
	int sigact_res;
	
	// for errlib to know the program name 
	prog_name = argv[0];

	// check arguments
	if (argc==2){
		xdr_enable=0;
		port=atoi(argv[1]);
	}else if(argc==3){
		if(strcmp("-x",argv[1])==0){
			xdr_enable=1;
			port=atoi(argv[2]);
		}
		else{
			err_quit ("usage: %s <port> or %s <-x> <port>\n", prog_name, prog_name);
		}
	}else{
		err_quit ("usage: %s <port> or %s <-x> <port>\n", prog_name, prog_name);
	}
		
	

	//clear server struct
	memset(&serveraddr, 0, sizeof(serveraddr));
      	listenfd= init_Server_dual_stack(TCP,port, &serveraddr);
      	
     	 /*
     	 if i call in the main():
     	 sd= init_Server(UDP, &serveraddr);
      	 NB: [NO LISTEN, NO ACCEPT]
      	 */   

	err_msg("(%s) socket created",prog_name);
	
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

		err_msg("(%s) - fork() to create a child", prog_name);
		
		if ( (childpid = Fork()) == 0) {
			// Child 
			int cpid = getpid();
			sprintf(prog_name_with_pid, "%s child %d ...", prog_name, cpid);
			prog_name = prog_name_with_pid;
			Close(listenfd);
			print_client_ipv6_address_tcp(connfd,prog_name,&clientaddr, &addrlen);
//TODO:START PROTOCOL SECTION
			
			if(xdr_enable==1){
				err = ftp_sender_xdr(connfd, prog_name);   //protocol
			}else{
				err = ftp_sender(connfd, prog_name); 
			}
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

