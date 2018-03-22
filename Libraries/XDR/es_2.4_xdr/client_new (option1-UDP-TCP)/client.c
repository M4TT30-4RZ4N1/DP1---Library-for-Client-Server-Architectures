
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

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

int myXDR_ENCODE(int mysocket);
int myXDR_DECODE(int mysocket);

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

//---------------------------------------------------------------------------------------------------------------------------
	//apertura file stream xdr sul socket
	// NB ne uso uno unico, flusso lettura/scrittura riconosciuto in base a ENCODE/DECODE
	
	
	control=myXDR_ENCODE(sockfd);
	if(control!=0){
		printf("Error XDR ENCODE protocol. \n");
	}
	
	control=myXDR_DECODE(sockfd);
	if(control!=0){
		printf("Error XDR DECODE protocol. \n");
	}
	
	Close(sockfd);
		
	return 0;
}

int myXDR_ENCODE(int mysocket){
	
	int op1, op2, ret_val=0;
	char buf_w[ MAXBUFL];
	
	
	// puntatori xdr
	XDR xdrs_w;  //write
	unsigned int xdr_len;
	
	xdrmem_create(&xdrs_w, buf_w,MAXBUFL, XDR_ENCODE);
	
	
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
	
	//amount of buffer used
	xdr_len = xdr_getpos(&xdrs_w);
	
	//SE ERO UDP sendto
//ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,const struct sockaddr *dest_addr, socklen_t addrlen);
	writen(mysocket, buf_w, xdr_len);//TCP
	
	xdr_destroy(&xdrs_w);
	
	return ret_val;	
}

int myXDR_DECODE(int mysocket){
	
	int res, ret_val=0;
	char buf_r[ MAXBUFL];
	
	/*
	*DECODE
	*Alloco struttura/tipo: in.
	*
	*/
	
	XDR xdrs_r;  //read
	
	xdrmem_create(&xdrs_r, buf_r, MAXBUFL, XDR_DECODE);
	
	//NB qua non so le dimensioni, leggo della lunghezza del buffer
	
	//SE ERO UDP recvfrom
//ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
	readn(mysocket, buf_r, MAXBUFL);//TCP
		
	if (! xdr_int(&xdrs_r, &res) ) {
		printf("(Y) --- cannot read with XDR ...\n");
		ret_val= -1;
	} 
	
	printf("(Y) --- data received with XDR:SUM(%d) \n",res);
		
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

}*/


