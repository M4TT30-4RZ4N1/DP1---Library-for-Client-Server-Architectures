#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <string.h>
#include <time.h>

#include "errlib.h"
#include "sockwrap.h"

#define TRUE 1
#define FALSE 0

#define maxClients 10
#define maxM 3

typedef struct {
    unsigned long id;
    unsigned short count;
} client_req;
    
int isAllowed(struct sockaddr_in *sender, client_req *req, int *index);
char *prog_name;

int main(int argc, char **argv) {
    
    const short maxlen = 31;
    char buf[maxlen + 1];
    int mysocket;
    int index = 0;   //indici basato su aritmetica dei puntatori, deve essere passato per riferimento
    client_req *req; //array di struttura client_req
    struct sockaddr_in sender;
    struct sockaddr_in me;
    ssize_t byter;
    socklen_t sockl;
    int control;

    /* argv[1] = port number */
    if(argc != 2) {
        printf("Wrong parameters\n");
        exit (-1);
    }
    
    // initialization
    prog_name = argv[0];
    mysocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    if(mysocket==-1){
    	printf("Error : %s \n", strerror(errno));
    }
    req = (client_req *) malloc(maxClients  * sizeof(client_req));

    me.sin_family       = AF_INET;
    me.sin_port         = htons((uint16_t) atoi(argv[1]));     
    me.sin_addr.s_addr  = INADDR_ANY;
    control= bind(mysocket, (struct sockaddr *) &me, sizeof(me));
    if(control==-1){
    	printf("Error : %s \n", strerror(errno));
    }
    
    while(TRUE) {
    
        sockl = sizeof(struct sockaddr_in);
        printf("\n...Receiving...\n");
        byter = recvfrom(mysocket, buf, maxlen, 0, (SA *) &sender, &sockl);
        if(byter==-1){
    	printf("Error : %s \n", strerror(errno));
        }
        
        
        if(isAllowed(&sender, req, &index)) {
            buf[byter] = '\0';
            printf("%u bytes received. They say:\n", (unsigned int) byter);
            
            
            
            printf("%s\nSending back...\n", buf);
	    control=sendto(mysocket, buf, strlen(buf), 0, (struct sockaddr *) &sender, sizeof(sender));
	    if(control==-1){
    		printf("Error : %s \n", strerror(errno));
    	    }
        } else {
            printf("Response is not allowed\n");
        }

    }

    return 0;
}

int isAllowed(struct sockaddr_in *sender, client_req *req, int *index) {
    
    int i;
    client_req *tmp = req;

	//search into the list
    for(i = 0; i < (*index); i++) {
        //printf("%u\n", tmp->id);
        if(tmp->id == sender->sin_addr.s_addr) { //trovato
            
            if(tmp->count == maxM) {
                return FALSE;
            }

            tmp->count++;
            return TRUE;
        }
    
        tmp += sizeof(client_req); //mi sposto di un blocco
    }

    /* 
     * insert into the vector
     * if it is full, restart from 0, the 'oldest' one
     */
    tmp = req;
    if((*index) != maxClients ) {
        tmp += (*index) * sizeof(client_req);
        (*index)++;
    
    } else {
        (*index) = 0;
    }

    tmp->id = sender->sin_addr.s_addr;
    tmp->count = 1;
    
    return TRUE;
}



