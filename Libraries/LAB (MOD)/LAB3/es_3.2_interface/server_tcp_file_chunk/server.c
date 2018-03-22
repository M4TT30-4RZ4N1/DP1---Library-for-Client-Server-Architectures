
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

char prog_name_with_pid[MAXBUFL];

#define MAX_CHILDREN 3
int n_children; /* number of children still to be waited for */

/* The signal handler is used to wait on zombie processes, i.e. those terminated
   while waiting for the Accept to return */
/* The difficulty is that sig_chld is not guaranteed to get one single signal for each
   terminated child process: thus, cycle on waitpid() until it returns > 0 */

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


int ftp_sender(int mysocket, char* prog_name) {

	char buf[MAXBUFL+1]; /* +1 to make room for \0 */

	int ret_val = 0;



	while (1) {
		printf("(%s) - waiting for commands...\n",prog_name);
		printf("\n=====================================\n");
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
		printf("(%s) - received string '%s'\n",prog_name, buf);
		
		// get the command, compare n first characters 
		if (nread > strlen(MSG_GET) && strncmp(buf,MSG_GET,strlen(MSG_GET))==0) {
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
				write (mysocket, MSG_ERR, strlen(MSG_ERR) );
			}
		} else if (nread >= strlen(MSG_QUIT) && strncmp(buf,MSG_QUIT,strlen(MSG_QUIT))==0) {
			printf("(%s) --- client asked to terminate connection\n", prog_name);
			ret_val = 1;
			break;
		} else {
			write (mysocket, MSG_ERR, strlen(MSG_ERR) );
			ret_val = -1;
			break;
		}

	}
	
	printf("=====================================\n");
	return ret_val;
}

int main(int argc, char *argv[]) {

	int listenfd, connfd, err=0;
	short port;
	struct sockaddr_in servaddr, cliaddr;
	socklen_t cliaddrlen = sizeof(cliaddr);

	pid_t childpid;
	struct sigaction action;
	int sigact_res;
	

	// for errlib to know the program name 
	prog_name = argv[0];

	// check arguments
	if (argc!=2)
		err_quit ("usage: %s <port>\n", prog_name);
	port=atoi(argv[1]);

	// create socket
	listenfd = Socket(AF_INET, SOCK_STREAM, 0);
	

	// specify address to bind to
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl (INADDR_ANY);

	Bind(listenfd, (SA*) &servaddr, sizeof(servaddr));


	err_msg("(%s) socket created",prog_name);
	err_msg("(%s) listening on %s:%u", prog_name, inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));

	Listen(listenfd, LISTENQ);
	

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
		connfd = Accept (listenfd, (SA*) &cliaddr, &cliaddrlen);
		trace ( err_msg("(%s) - new connection from client %s:%u", prog_name, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port)) );


		err_msg("(%s) - fork() to create a child", prog_name);
		
		if ( (childpid = Fork()) == 0) {
			// Child 
			int cpid = getpid();
			sprintf(prog_name_with_pid, "%s child %d", prog_name, cpid);
			prog_name = prog_name_with_pid;
			Close(listenfd);
			err = ftp_sender(connfd,prog_name_with_pid);   //protocol
			if(err!=0){
				err_msg("(%s) - Error : protocol\n", prog_name);
			}			
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

