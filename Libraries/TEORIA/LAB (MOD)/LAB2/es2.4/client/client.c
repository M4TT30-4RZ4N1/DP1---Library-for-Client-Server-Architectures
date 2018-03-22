
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

#include "errlib.h"
#include "sockwrap.h"

#define MAXBUFL 255

#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif

char *prog_name;


int main (int argc, char *argv[]) {

	int sockfd;
	char *dest_ip;
	int dest_port;
	struct sockaddr_in servaddr;
	//in_addr_t dest_addr;
	struct in_addr dest_addr_aton;
	int op1, op2, res;
	char buf[MAXBUFL];

	XDR xdrs_w;  //write
	XDR xdrs_r;  //read
	unsigned int xdr_len;

	/* for errlib to know the program name */
	prog_name = argv[0];

	/* check arguments */
	if (argc!=3)
		err_quit ("usage: %s <dest_ip> <dest_port>", prog_name);
	dest_ip=argv[1];
	dest_port=atoi(argv[2]);

 	//dest_addr = inet_addr(dest_ip);  /* Avoid using deprecated functions */
	if (inet_aton(dest_ip, &dest_addr_aton )==0) {
		err_quit ("(%s) error - inet_aton() failed", prog_name );
	}

	/* create socket */
	sockfd = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	trace ( err_msg("(%s) socket created",prog_name) );

	/* specify address to bind to */
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(dest_port);
	//servaddr.sin_addr.s_addr = dest_addr;
	servaddr.sin_addr.s_addr = dest_addr_aton.s_addr;

	Connect ( sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr) );

	trace ( err_msg("(%s) connected to %s:%u", prog_name, inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port)) );

	printf("Please insert the first operand to send to the server: ");
	scanf("%d", &op1);
	printf("Please insert the second operand to send to the server: ");
	scanf("%d", &op2);
	
	//option (1) - send/receive
	//NEED TO KNOW DIMENSION !!!
	//in this option must use xdrmem_create

	//create stream XDR
	xdrmem_create(&xdrs_w, buf, MAXBUFL, XDR_ENCODE);

	//append data to the stream XDR
	xdr_int(&xdrs_w, &op1);
	xdr_int(&xdrs_w, &op2);

	//amount of buffer used
	xdr_len = xdr_getpos(&xdrs_w);
	
	//SE ERO UDP sendto
	write(sockfd, buf, xdr_len);//TCP
	

	sprintf(buf, "%d %d", op1, op2);

	trace ( err_msg("(%s) - data (%s) has been sent", prog_name, buf) );

	//option (2)- send/receive data on XDR - manage socket as file_stream
	//NO NEED TO KNOW DIMENSION !!!
	//function do extract data from stream are the same to upload
	//the difference is : XDR_ENCODE/ XDR_DECODE 
	//in this option must use xdrstdio_create
 
	FILE *stream_socket_r = fdopen(sockfd, "r");

	if (stream_socket_r == NULL){
		trace(err_msg ("(%s) error - fdopen() failed\n", prog_name));
	}
	else{
		xdrstdio_create(&xdrs_r, stream_socket_r, XDR_DECODE);

		if ( ! xdr_int(&xdrs_r, &res) ) {
			trace( err_msg("(%s) - cannot read res with XDR ...\n", prog_name) );
		} else {
			printf("Sum: %d\n", res);
		}
		
	}
	
	Close(sockfd);
	
	xdr_destroy(&xdrs_w);
	xdr_destroy(&xdrs_r);
	fclose(stream_socket_r);
	
		
	

	return 0;
}

