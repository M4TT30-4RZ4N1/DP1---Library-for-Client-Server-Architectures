# Makefile for Square client and server
#
EXE 	=	SquareClient SquareServer 
SRC 	=	client/SquareClient.c server/SquareServer.c 

all:		$(SRC) errlib.h errlib.c  sockwrap.h sockwrap.c types.h types.c
		cc -g -DTRACE -o SquareClient client/*.c *.c -I client -lpthread -lm
		cc -g -DTRACE -o SquareServer server/*.c *.c -I server -lpthread -lm

types.h:	types.xdr 
		rpcgen -h -o types.h types.xdr

types.c:	types.xdr 
		rpcgen -c -o types.c types.xdr
