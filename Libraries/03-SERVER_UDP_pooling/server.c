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
#define MAXCLIENTS 10
#define MAXTOKEN 3

#define MAX_STR 1023

#define TIMEOUT 120

#define MAXCHILDS 10

char *prog_name;

char prog_name_with_pid[MAXBUFL];

int nchildren;
pid_t pids[MAXCHILDS];


//la limitazione è per processo, non è globale!!!!
void handle_clients_udp(int mysocket,char* progname);
void handle_clients_udp(int mysocket,char* progname) {

	char clients[MAXCLIENTS][MAXBUFL];
	int token[MAXCLIENTS];
	int tot_clients=0;

	while(1){

	//it's the structure that can contain both ipv4/ipv6 client
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len;
	peer_addr_len = sizeof(struct sockaddr_storage);
	
	char buf[MAXBUFL];//buffer for data, must be extract after recvfrom and then populate after sendto
	ssize_t nread;
	int found=0, block=0;
	//info client
	char host[NI_MAXHOST], service[NI_MAXSERV];
	int err;


/*TODO: RECEIVE DATAGRAM*/
//ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,struct sockaddr *src_addr, socklen_t *addrlen);
	nread = recvfrom(mysocket, buf, MAXBUFL, 0, (struct sockaddr *) &peer_addr, &peer_addr_len);
	
               if (nread == -1)
                   continue;               /* Ignore failed request, if i need other do other */
                   
        buf[nread]='\0';
                   
        err = getnameinfo((struct sockaddr *) &peer_addr,
        		   peer_addr_len, 
        		   host, 
        		   NI_MAXHOST,
        		   service, 
        		   NI_MAXSERV, NI_NUMERICSERV);
             
          
       if (err == 0){
       		fflush(stdout);
       		fprintf(stderr,"(%s) - Received %zd bytes from %s:%s\n",progname, nread, host, service);
                fprintf(stderr,"(%s) - Buffer: '%s'\n",progname, buf);
                
                int i;
                fprintf(stderr,"(%s) - %d\n",progname,tot_clients);
                for(i=0;i<tot_clients;i++){
                	if(strcmp(clients[i],host)==0){
                		found=1;
                		break;
                	}
                }
                
                if(found==1){ // IN LIST
                	 fprintf(stderr,"(%s) - Client %s already in the list with token: %d\n",progname,clients[i],token[i]);
                	 token[i]++;
                	 if(token[i]>MAXTOKEN){//NUMBER OF MAX ACCESS
                	 	block=1;
                	 }
                }else{	//NOT IN LIST
                	fprintf(stderr,"(%s) - New client %s first access, added to list\n",progname,host);
                	fprintf(stderr,"(%s) - %d\n",progname,tot_clients);
                	strcpy(clients[tot_clients],host);
                	token[tot_clients]=1;
                	tot_clients++;
                	fprintf(stderr,"(%s) - %d\n",progname,tot_clients);
                }
       	if(block==0){	
       		// SEND DATAGRAM TO THE ONE WHO ASK ME
//ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,const struct sockaddr *dest_addr, socklen_t addrlen);
		if (sendto(mysocket, buf, nread, 0,(struct sockaddr *) &peer_addr,peer_addr_len) != nread){
		           fprintf(stderr,"(%s) - Error sending response\n", progname);
		}else{
		        remove_end_specials(buf);
			fprintf(stderr,"(%s) - Send: '%s'\n", progname, buf);
		} 
	     }
       }
       else{
       		fprintf(stderr,"getnameinfo: %s\n", gai_strerror(err));
       }		
       		/*TODO: MODIFY DATA RECEIVED ---  PROTOCOL HEART*/
       		
       		
 }
	
}


static void sig_int(int signal) {//terminate childs when the parent is terminated
	int i;
	err_msg ("(%s) info - sig_int() called", prog_name);
	for (i=0; i<nchildren; i++)
		kill(pids[i], SIGTERM);

	while( wait(NULL) > 0);  // wait for all children
		

	if (errno != ECHILD)
		err_quit("(%s) error: wait() error",prog_name);

	exit(0);
}

int udp_protocol(int mysocket, char* progname){

while(1){

	//it's the structure that can contain both ipv4/ipv6 client
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len;
	peer_addr_len = sizeof(struct sockaddr_storage);
	
	char buf[MAXBUFL];//buffer for data, must be extract after recvfrom and then populate after sendto
	ssize_t nread;
	
	//info client
	char host[NI_MAXHOST], service[NI_MAXSERV];
	int err;


/*TODO: RECEIVE DATAGRAM*/
//ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,struct sockaddr *src_addr, socklen_t *addrlen);
	nread = recvfrom(mysocket, buf, MAXBUFL, 0, (struct sockaddr *) &peer_addr, &peer_addr_len);
               if (nread == -1)
                   continue;               /* Ignore failed request, if i need other do other */
          
        buf[nread]='\0';         
        err = getnameinfo((struct sockaddr *) &peer_addr,
        		   peer_addr_len, 
        		   host, 
        		   NI_MAXHOST,
        		   service, 
        		   NI_MAXSERV, NI_NUMERICSERV);
             
       if (err == 0){
       		
       		fprintf(stderr,"(%s) - Received %zd bytes from %s:%s\n",progname, nread, host, service);
                remove_end_specials(buf);
       		fprintf(stderr,"(%s) - Buffer: '%s'\n",progname, buf);

       }
       else
       		fprintf(stderr,"getnameinfo: %s\n", gai_strerror(err));
       		
       		/*TODO: MODIFY DATA RECEIVED ---  PROTOCOL HEART*/
       		
       		
                   
// SEND DATAGRAM TO THE ONE WHO ASK ME
//ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,const struct sockaddr *dest_addr, socklen_t addrlen);
        if (sendto(mysocket, buf, nread, 0,(struct sockaddr *) &peer_addr,peer_addr_len) != nread){
                   fprintf(stderr,"(%s) - Error sending response\n", progname);
        }else{	
                remove_end_specials(buf);
        	fprintf(stderr,"(%s) - Send: '%s'\n", progname, buf);
        }      
 }
 
}

int main (int argc, char *argv[]) {

	int sockfd,err=0;
	//short port;
	struct sigaction action;
	int sigact_res;
	int i;
	struct addrinfo hints;
        struct addrinfo *result, *rp;

	
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

	// getaddrinfo paradigm
	 memset(&hints, 0, sizeof(struct addrinfo));
         hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
         hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
         hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
         hints.ai_protocol = 0;          /* Any protocol */
         hints.ai_canonname = NULL;
         hints.ai_addr = NULL;
         hints.ai_next = NULL;

	//argv[1] is the port 
	 err = getaddrinfo(NULL, argv[1], &hints, &result);
           if (err != 0) {
               printf("(%s) - getaddrinfo: %s\n", prog_name, gai_strerror(err));
               exit(EXIT_FAILURE);
           }
           
           for (rp = result; rp != NULL; rp = rp->ai_next) {
               sockfd= socket(rp->ai_family, rp->ai_socktype,
                       rp->ai_protocol);
                       
               if (sockfd== -1)
                   continue;

               if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == 0)
                   break;                  /* Success */

               close(sockfd);
           }
           
           if (rp == NULL) {               /* No address succeeded */
               printf("(%s) - Could not bind\n", prog_name);
               exit(EXIT_FAILURE);
           }

           freeaddrinfo(result);
           
           
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
				
			//TODO:PROTOCOL
			
			//udp_protocol(sockfd, prog_name);
			udp_protocol(sockfd,prog_name_with_pid);
						
			Close(sockfd);
			
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

