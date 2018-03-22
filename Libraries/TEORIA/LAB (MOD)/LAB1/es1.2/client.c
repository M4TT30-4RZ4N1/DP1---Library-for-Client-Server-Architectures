
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



if( argc!= 3){
fprintf(stderr,"Error number of parameters");
return -1;
}

prog_name=argv[0];

port_number=atoi(argv[2]);
printf("\nPort number: %d\n", port_number);
printf("Address: %s\n\n", argv[1]);

sockfd= socket(AF_INET, SOCK_STREAM, 0);



memset( &Serv, 0, sizeof(Serv) );
Serv.sin_family=  AF_INET;

//utilizzo  inet_aton per traduzione indirizzo
result= inet_aton(argv[1],&Serv.sin_addr);
if(result==0){
	printf(" errore  address conversion %d\n", result);  //ritorna 1 se stringa valida
}

//Serv.sin_addr.s_addr      =  inet_addr(argv[1]);
Serv.sin_port=  htons(port_number);
result=connect( sockfd, (struct sockaddr*) &Serv, sizeof(Serv));

if(result<0 ){
	printf(" errore  connect %d\n", result);

}
else{
	printf(" ok connect %d\n",result);
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
