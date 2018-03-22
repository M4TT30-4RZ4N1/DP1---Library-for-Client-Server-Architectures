
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

int protocol(int socketid);

int main (int argc, char *argv[]) {

struct sockaddr_in Serv;
short port_number;
int result=0, sockfd=0;


if( argc!= 3){

	fprintf(stderr,"Error number of parameters");
	return -1;
}

prog_name=argv[0];

port_number=atoi(argv[2]);


sockfd= socket(AF_INET, SOCK_STREAM, 0);

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

//tentativo connessione
result=connect( sockfd, (struct sockaddr*) &Serv, sizeof(Serv));

if(result<0 ){
	printf(" errore  connect %d\n", result);

}
else{

	printf("connection estabilished: %d\n",result);

	result= protocol(sockfd);

	if(result!=0){
		
		trace( err_msg("(%s) - protocol error detection",prog_name) );
	}



}//else connessione

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


int protocol(int socketid){

char buf_r[MAXBUFL+1]; 
char buf_s[MAXBUFL+1]; 
ssize_t byte_sent=0;
ssize_t byte_recv=0;
ssize_t dim=0;
uint16_t op1,op2;
int control=0;


	while(1){

		printf("Insert two number with the format: a b (and press enter)\n");
		//leggo da stdin e invio
		control=fscanf(stdin,"%hu %hu",&op1,&op2);

		if(control!=2){
			printf("errore formato\n");
			return -1;
		}
		else{

		sprintf(buf_s,"%hu %hu\r\n",op1,op2);

		dim=strlen(buf_s);
		byte_sent=send(socketid,buf_s,dim,0);

		if (byte_sent!=dim) {
			trace( err_msg("(%s) - cannot send operands",prog_name) );
			return -1;
		} 
		else {
			trace( err_msg("(%s) --- operands successful sent", prog_name) );
		     
		}

		//provo a ricevere
		//non usare la recv e la read che scartano i dati in eccesso
		byte_recv=Readline(socketid,buf_r,MAXBUFL);

		if (byte_recv == 0) {
			return -1;
		} 
		else if (byte_recv < 0) {
			err_ret ("(%s) error - readline() failed",prog_name);
			
			return -1;
		}

		fprintf(stdout,"\n Sum: %s", buf_r);

		}//else

	}//while



	return 0;

}












