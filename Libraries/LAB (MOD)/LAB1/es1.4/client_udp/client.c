
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <rpc/xdr.h>

#include <string.h>
#include <time.h>
#include <unistd.h>

#include "errlib.h"
#include "sockwrap.h"

//#define SRVPORT 1500

#define LISTENQ 15
#define MAXBUFL 255

#define MSG_ERR "wrong operands\r\n"
#define MSG_OVF "overflow\r\n"

#define MAX_UINT16T 0xffff
//#define STATE_OK 0x00
//#define STATE_V  0x01

#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif

char *prog_name;




int main (int argc, char *argv[]) {

struct sockaddr_in Serv;
short port_number;
int result=0, sockfd=0;

char buf_r[MAXBUFL+1]; 
char buf_s[MAXBUFL+1]; 
ssize_t byte_sent=0;
ssize_t byte_recv=0;
socklen_t l_server;




if( argc!= 3){
	fprintf(stderr,"Error number of parameters");
	return -1;
}

prog_name=argv[0];

port_number=atoi(argv[2]);


sockfd= socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

if(sockfd<0){
	trace( err_msg("(%s) - socket() failed",prog_name) );
}

//settaggio parametri del server ricevuti da linea di comando

memset( &Serv, 0, sizeof(Serv) );
Serv.sin_family=  AF_INET;
//utilizzo  inet_aton per traduzione indirizzo
result=inet_aton(argv[1],&Serv.sin_addr);
if(result==0){
	printf(" errore  address conversion %d\n", result);  //ritorna 1 se stringa valida
}
//Serv.sin_addr.s_addr      =  inet_addr(argv[1]);
Serv.sin_port=  htons(port_number);


    //richiesta messaggio lato client-utente
    bzero(buf_s, MAXBUFL);
    printf("Please enter msg: ");
    fgets(buf_s, MAXBUFL , stdin);
    
    //invio datagram udp
    byte_sent = sendto(sockfd, buf_s, strlen(buf_s), 0,  (struct sockaddr*) &Serv, sizeof(Serv));
    
    if (byte_sent  < 0){
      printf("ERROR in sendto");
      exit(-1);
      }


    //uso della select per gestire un timeout
	//TIMER
	struct timeval mytimer;
	mytimer.tv_sec = 6;  /* 30 Secs Timeout */
	mytimer.tv_usec = 0;  // Not init'ing this can cause strange errors

	int n;
	fd_set mysockets; //lista socket
	FD_ZERO(&mysockets);//pulizia
	FD_SET(sockfd,&mysockets);//inserimento socket nella lista

if((n=select(FD_SETSIZE,&mysockets,NULL,NULL,&mytimer))==-1){
	fprintf(stderr," select() - failed\n");
}
else if(n>0){

	//if(FD_ISSET(sockfd,&mysockets)) se avessi + socket...ma sono il client.
    
    //ricezione datagram dal server
    l_server = sizeof(Serv);
    byte_recv = recvfrom(sockfd, buf_r, MAXBUFL , 0, (struct sockaddr*) &Serv,&l_server);
    
    if (byte_recv  < 0) {
      printf("ERROR in recvfrom");
       exit(-2);
      }
      else{
      
    printf("Echo from server: %s", buf_r);
    
	}
}
else{
	fprintf(stdout,"No response - timeout has expired \n");
}
    

result=close(sockfd);

//controlla
if(result<0 ){
	printf(" errore close %d\n", result);

}
else{
	printf(" ok close %d\n",result);


}



return 0;
}
