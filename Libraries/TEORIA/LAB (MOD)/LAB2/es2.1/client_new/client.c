#include <stdlib.h>
#include <string.h>
#include "errlib.h"
#include "sockwrap.h"
#include "sock_manager.h"

char *prog_name;
#define TRUE 0
#define MAXLEN 128
#define TIMEOUT 10
#define TRIES 5

int protocol(ClientSocket *sh, int count);

int main(int argc, char **argv) {

      int len;
      int control;
    /*
    if(argc != 4 || strlen(argv[3]) > MAXLEN) {
        err_quit("Wrong parameters");
    }*/

    if(argc != 4)
        err_quit("Wrong parameters");

    // New socket with the name ready to be sent
    ClientSocket *sh = s_init(MAXLEN + 1);
    len = strlen(argv[3]);
    strcpy(sh->tx_buf, argv[3]);
    sh->tx_buf[len] = '\0';

    prog_name = argv[0];

    // Create a new socket from given ipv4 address and server port
    udp_createSocket(sh, argv[1], argv[2]);
    
    int count=0;
     
    while(count< TRIES){// 5 timeout impostati
    	count++;
    	control=protocol(sh,count);
    	if(control==0){ //inviato, non serve ritentare
    		break;
    	}
    }
    
    s_end(sh);

    return 0;
}

int protocol(ClientSocket *sh, int count){

 	// Manages timecounter for IO operations
    struct timeval tval;

    
	// Send the string in a datagram
    Sendto(sh->socket_id, sh->tx_buf, strlen(sh->tx_buf), 0, (struct sockaddr *) &(sh->dest_addr), sizeof(sh->dest_addr));

    printf("String sent, waiting for response...\n");

    FD_ZERO(&sh->rset);
    FD_SET(sh->socket_id, &sh->rset);
    tval.tv_sec = TIMEOUT;
    tval.tv_usec = 0;
    int n;
   
	   n = Select(FD_SETSIZE, &sh->rset, NULL, NULL, &tval);
	    
	    struct sockaddr_in sender;
	  
	    
	    if (n>0){
		udp_recvAll(sh, MAXLEN, &sender);
		return 0;
	    } 
	    else
		printf("TimeOut (%d): No response received after %d seconds\n",count,TIMEOUT);
		return -1;
    
        
        

}
