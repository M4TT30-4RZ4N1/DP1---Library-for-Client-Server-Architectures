
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <string.h>
#include <time.h>

#include "errlib.h"
#include "sockwrap.h"

#define MAXBUFL 255
#define MAX_STR 1023

#define MSG_GET "GET"
#define MSG_OK "+OK"
#define MSG_ERR "-ERR"
#define MSG_Q "Q"
#define MSG_A "A"

#ifdef TRACE
#define trace(x) x
#else
#define trace(x)
#endif

char *prog_name;

int main (int argc, char *argv[]) {  //THIS IS A INTERFACE CREATED TO MANAGE COMANDS WITH A SPECIFIC FORMAT WITH NO BUGS

	// for errlib to know the program name 
	prog_name = argv[0];


	char command[MAXBUFL];


	printf("Client ready, waiting for commands (GET file,Q,A):\n");

	while (1) {

/*KEYBOARD in each if control the command format and command bytes, or other info that must be present with the command */s

			// standard input is readable, fgets read until \n or EOF
			if ( (fgets(command, MAXBUFL, stdin)) == NULL) { 
				// Ask the server to terminate if EOF is sent from stdin
				strcpy(command,MSG_Q);
			}
			else if( (strncmp(command,MSG_GET,strlen(MSG_GET))==0) && (command[strlen(MSG_GET)]==' ')){

				trace( err_msg("(%s) --- GET command sent to server",prog_name) );
				// TODO: here send to server the GET filename
						

			}
			else if((strncmp(command,MSG_Q,strlen(MSG_Q))==0) && (strlen(MSG_A)==(strlen(command)-1))){

				printf("Q command received\n");
				// TODO: here quit only after the file was succesful transfered

			}
				
			else if((strncmp(command,MSG_A,strlen(MSG_A))==0) && (strlen(MSG_A)==(strlen(command)-1))){
				
				printf("A command received\n");
				// TODO: here quit during the file transfer
			}
			else{ 	
				trace( err_msg("(%s) --- INVALID COMMAND FORMAT ",prog_name) );
			}

// NB. i'm using strlen(command)-1 cause in the command i have the \n !!!
			
			
			
		
	}

	return 0;
}

