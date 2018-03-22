# Makefile for Square client and server
#
EXE 	=	SquareClient SquareServer 
SRC 	=	SquareClient.c SquareServer.c 

all:		$(SRC) sockwrap.o sockwrap.h errlib.o errlib.h types.h types.c
		for f in $(EXE); do (cc -o $$f $$f.c sockwrap.o errlib.o types.c); done

sockwrap.o:	sockwrap.c sockwrap.h
		cc $(FLAGS) -c -o sockwrap.o sockwrap.c

errlib.o:	errlib.c errlib.h
		cc $(FLAGS) -c -o errlib.o errlib.c

types.h:	types.xdr 
		rpcgen -h -o types.h types.xdr

types.c:	types.xdr 
		rpcgen -c -o types.c types.xdr
