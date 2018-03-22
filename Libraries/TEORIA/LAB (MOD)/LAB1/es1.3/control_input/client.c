
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
#include "sock_manager.h"

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

int protocol(int socketid);



int main (int argc, char *argv[]) {

if(argc != 3) {
        err_quit("Wrong parameters");
    }

    ClientSocket *sh = s_init(128);

    prog_name = argv[0];
    char* server_A = argv[1];
    char* server_P = argv[2];

    // Create a new socket from given server port and ipv4 address
    tcp_createSocket(sh, server_A, server_P);

    s_connect(sh);
    
    int result= protocol(sh->socket_id);
    
    if(result!=0){
	trace( err_msg("(%s) - protocol error detection\n",prog_name) );
    }else{
            fprintf(stdout,"Connection closed\n");
            s_end(sh);
        }
    


	return 0;
}


int protocol(int socketid){

	char buf_r[MAXBUFL+1]; 
	char buf_s[MAXBUFL+1]; 
	ssize_t byte_sent=0;
	ssize_t byte_recv=0;
	ssize_t dim=0;
	uint16_t op1,op2;
	uint16_t sum;
	int control=0;


	while(1){

		printf("Insert two number with the format: (a b and press enter-'stop' to terminate)\n");
		
		read_stdin(buf_r,MAXBUFL);
		control= control_parameters_stdin(buf_r);
		
		if(control==1){
		//se contiene stop
			if(strstr(buf_r,"stop")){
				break;
			}
			else{
				fprintf(stdout,"errore formato\n");
				return -1;
			}	
		}else if(control==2){
		
			//leggo da stdin e invio
			sscanf(buf_r,"%hu %hu",&op1,&op2);


			sprintf(buf_s,"%hu %hu\r\n",op1,op2);

			dim=strlen(buf_s);
			byte_sent=send(socketid,buf_s,dim,0);

			if (byte_sent!=dim) {
				trace( err_msg("(%s) - cannot send operands\n",prog_name) );
				return -1;
			} 
			else {
				trace( err_msg("(%s) --- operands successful sent\n", prog_name) );
			     
			}

		//provo a ricevere
		//non usare la recv e la read che scartano i dati in eccesso
		byte_recv=Readline(socketid,buf_r,MAXBUFL);

		if (byte_recv == 0) {
			return -1;
		} 
		else if (byte_recv < 0) {
			err_ret ("(%s) error - readline() failed",prog_name);
			
			return -1;
		}
		
		sscanf(buf_r,"%hu",&sum);
		if(sum<0 || sum > 65536){
			fprintf(stdout,"\n Overflow!");
		}else{
			fprintf(stdout,"\n Sum: %s", buf_r);
		}
		fprintf(stdout,"\n===========================================================\n");

		}//else if (2)
		else{
			fprintf(stdout,"errore formato\n");
			return -1;
		}

	}//while



	return 0;

}














