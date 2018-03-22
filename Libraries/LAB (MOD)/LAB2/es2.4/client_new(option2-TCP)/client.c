
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

int myXDR_ENCODE(FILE *stream_socket);
int myXDR_DECODE(FILE *stream_socket);

int main (int argc, char *argv[]) {

	int sockfd,control;
	char *dest_ip;
	int dest_port;
	struct sockaddr_in servaddr;
	//in_addr_t dest_addr;
	struct in_addr dest_addr_aton;

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


	//apertura file stream xdr sul socket
	// NB ne uso uno unico, flusso lettura/scrittura riconosciuto in base a ENCODE/DECODE
	FILE *stream_socket = fdopen(sockfd, "r+");
	if (stream_socket == NULL){
		printf("(Y) error - fdopen() failed\n");
	}
	
	control=myXDR_ENCODE(stream_socket);
	if(control!=0){
		printf("Error XDR ENCODE protocol. \n");
	}
	
	control=myXDR_DECODE(stream_socket);
	if(control!=0){
		printf("Error XDR DECODE protocol. \n");
	}
	
	Close(sockfd);
	
	fflush(stream_socket);
	fclose(stream_socket);
		
	
	
	return 0;
}

int myXDR_ENCODE(FILE *stream_socket){
	
	int op1, op2, ret_val=0;
	
	
	// puntatori xdr
	XDR xdrs_w;  //write
	
	xdrstdio_create(&xdrs_w, stream_socket, XDR_ENCODE);
	
	
	/*
	*ENCODE
	*Alloco e Popolo struttura/tipo: out.
	*
	*/
	
	printf("Please insert the first operand to send to the server: ");
	scanf("%d", &op1);
	printf("Please insert the second operand to send to the server: ");
	scanf("%d", &op2);
	
	if ( ! xdr_int(&xdrs_w, &op1) ) {
		printf("(Y) --- cannot write with XDR ...\n");
		ret_val= -1;
	} else {
		printf("(Y) --- data sent with XDR.\n");
	}
	
	if ( ! xdr_int(&xdrs_w, &op2) ) {
		printf("(Y) --- cannot write with XDR ...\n");
		ret_val= -1;
	} else {
		printf("(Y) --- data sent with XDR.\n");
	}
	
	xdr_destroy(&xdrs_w);
	
	return ret_val;	
}

int myXDR_DECODE(FILE *stream_socket){
	
	int res, ret_val=0;
	/*
	*DECODE
	*Alloco struttura/tipo: in.
	*
	*/
	
	XDR xdrs_r;  //read
	
	xdrstdio_create(&xdrs_r, stream_socket, XDR_DECODE);
		
	if (! xdr_int(&xdrs_r, &res) ) {
		printf("(Y) --- cannot read with XDR ...\n");
		ret_val= -1;
	} else {
		printf("(Y) --- data received with XDR: %d \n", res);
	}
		
	xdr_destroy(&xdrs_r);
	
	
	return ret_val;
}


/* SOME USEFUL XDR TYPES 

bool_t xdr_bool(XDR *xdrs, bool_t *bp);

bool_t xdr_char(XDR *xdrs, char *cp);

bool_t xdr_double(XDR *xdrs, double *dp);

bool_t xdr_enum(XDR *xdrs, enum_t *ep);

bool_t xdr_float(XDR *xdrs, float *fp);

bool_t xdr_long(XDR *xdrs, long *lp);
----------------------------------------------------------------------------------------------
xdrmem_create(&xdrs_w, buf, MAXBUFL, XDR_ENCODE);

	//append data to the stream XDR
	xdr_int(&xdrs_w, &op1);
	xdr_int(&xdrs_w, &op2);

	//amount of buffer used
	xdr_len = xdr_getpos(&xdrs_w);
	
	Write(sockfd, buf, xdr_len);
-----------------------------------------------------------------------------------------------
bool_t xdr_bytes(XDR *xdrs, char **sp, unsigned int *sizep,
                        unsigned int maxsize);

              A  filter primitive that translates between counted byte strings
              and their external representations.   The  argument  sp  is  the
              address  of  the  string  pointer.   The length of the string is
              located at address sizep; strings cannot be longer than maxsize.
              This routine returns one if it succeeds, zero otherwise.
------------------------------------------------------------------------------------------------

}/


