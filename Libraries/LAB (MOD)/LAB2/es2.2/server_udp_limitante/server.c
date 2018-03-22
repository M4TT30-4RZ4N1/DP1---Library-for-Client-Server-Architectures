
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

#define MAXBUFL 255
#define MAXCLIENTS 10

#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif

char *prog_name;

/* use sockaddr_storage instead of sockaddr since the former is guaranteed to be large enough to hold an IPv6 address if needed */
struct sockaddr_storage clients[MAXCLIENTS];
int token[MAXCLIENTS];
int tot_clients;

void handle_clients_udp(int sockfd);
void handle_clients_udp(int sockfd) {

	char buf[MAXBUFL+1]; //  \0 
	int recv_size;

	struct sockaddr_storage peer_addr; //the istance of client (not in list maybe)
	socklen_t peer_addr_len;

	peer_addr_len = sizeof(struct sockaddr_storage); 

	recv_size = recvfrom(sockfd, buf, MAXBUFL, 0, (struct sockaddr *) &peer_addr, &peer_addr_len);
	buf[recv_size]='\0';
	trace( err_msg("(%s) --- received string '%s'",prog_name, buf) );
		
	
	in_port_t port;
	char text_addr[INET6_ADDRSTRLEN];
	port = ((struct sockaddr_in *)&peer_addr)->sin_port;

	//convert src address into dest addr(text_addr) of any address family (AF_UNSPECT)
	inet_ntop(AF_UNSPEC, &( ((struct sockaddr_in *)&peer_addr)->sin_addr), text_addr, sizeof(text_addr));


	trace( err_msg("(%s) --- source IP address port %s %u", prog_name, text_addr, ntohs(port)) );
	
	int flag_blocked = 0;
	
	//src --> it's the new client 
	//list  --> all the client already registered

	int i;
	int found=0;

	for (i=0; i<tot_clients; i++) { //controll into the list
		struct sockaddr_in *src = (struct sockaddr_in *)&peer_addr;
		struct sockaddr_in *list = (struct sockaddr_in *)&(clients[i]);

		//if (src->sin_port == el->sin_port && src->sin_addr.s_addr == el->sin_addr.s_addr) {
		//NB: per ogni pacchetto UDP la porta cambia: non con TCP (connection oriented)
		
		//match
		if (src->sin_addr.s_addr == list->sin_addr.s_addr){
			printf("Found in the list: %s \n" ,inet_ntoa(list->sin_addr));
			found=1;
			break;
			}
	}

	//no match thanks (cause no break)
	if (found==0) {

		// Address not found

		if (tot_clients < MAXCLIENTS) {
		//i have space to insert the client to the list
			struct sockaddr_in *list = (struct sockaddr_in *)&(clients[tot_clients]);
			struct sockaddr_in *src = (struct sockaddr_in *)&peer_addr;
			list->sin_family = AF_UNSPEC;
			list->sin_port = src->sin_port;
			list->sin_addr = src->sin_addr;
			token[tot_clients]=1;
		
			printf("Address new client: %s \n", inet_ntoa(list->sin_addr));
			
			tot_clients++;	
			trace( err_msg("(%s) --- the address has been added to the list", prog_name) );
		} 
		else {
			trace( err_msg("(%s) --- cannot add more than %d clients to the list", prog_name, MAXCLIENTS) );
		}
	} 
	else {  //found==1
		//Address has been found 
		if (token[i]>=3) {
			flag_blocked = 1;
			trace( err_msg("(%s) --- the address has already sent 3 or more packets -> blocked", prog_name) );
		} 
		else {
			trace( err_msg("(%s) --- the address has sent only %d packets: adding one", prog_name, token[i]) );
			
			//update token
			token[i]++;
		}
	}


	if ( ! flag_blocked) {
		sendto(sockfd, buf, recv_size, 0, (struct sockaddr *) &peer_addr, peer_addr_len);
	}
}


int main (int argc, char *argv[]) {

	int sock_udp;
	short port;
	struct sockaddr_in servaddr;
	int i;

	// for errlib to know the program name 
	prog_name = argv[0];

	// check arguments 
	if (argc!=2)
		err_quit ("usage: %s <port>\n", prog_name);

	port=atoi(argv[1]);

	for (i=0; i<MAXCLIENTS; i++) {
		memset(&(clients[i]), 0, sizeof(struct sockaddr_storage));
		token[i] = 0;
	}
	tot_clients = 0;

	// create socket 
	sock_udp = Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	// specify address to bind to 
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl (INADDR_ANY);

	Bind(sock_udp, (SA*) &servaddr, sizeof(servaddr));

	trace ( err_msg("(%s) socket created",prog_name) );
	trace ( err_msg("(%s) listening for UDP packets on %s:%u", prog_name, inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port)) );

	while (1) {
		trace( err_msg ("(%s) waiting for a packet ...", prog_name) );
	
		handle_clients_udp(sock_udp);
	}

	return 0;
}

