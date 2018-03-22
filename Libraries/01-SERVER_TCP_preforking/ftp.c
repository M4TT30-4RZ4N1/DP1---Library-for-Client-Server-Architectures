#include "ftp.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sys/stat.h>
#include <time.h>

#include "errlib.h"
#include "sockwrap.h"

#define LISTENQ 15
#define MAXBUFL 255
#define CHUNK 1024

#define TIMEOUT 120

#define TCP "TCP"
#define UDP "UDP"

#define MSG_ERR "-ERR\r\n"
#define MSG_OK  "+OK\r\n"
#define MSG_QUIT  "QUIT"
#define MSG_GET "GET"
#define MSG_Q "Q"
#define MSG_A "A"

#define MAX_STR 1023

extern int sigpipe;

int file_socket_receive(int mysocket, char* buf,char* filename, char* progname){

			
			
			//read size_of_file that is still in the socket	
			int n, nread;	
			uint32_t file_size;
			n= read(mysocket, buf, 4);
			file_size = ntohl((*(uint32_t *)buf));
			printf("(%s) --- received file size '%u'\n",progname,file_size);

			//read time_stamp
			struct tm * timeinfo;
			int time;
			uint32_t val_time;		
			n= read(mysocket, buf, 4);
			val_time= (*(uint32_t *)buf);
			time = ntohl(val_time);
			time_t time_format=time;
			timeinfo = localtime(&time_format);
			strftime (buf,MAXBUFL,"Last file modification: %Y-%m-%d %H:%M:%S.\n",timeinfo);
			printf("(%s) --- received time information :%s",progname ,buf);

			FILE *fp;
			char filename2[MAXBUFL+1];
			strcpy(filename2,filename);
			if ( (fp=fopen(filename, "w+"))!=NULL) {
				
				printf("(%s) --- File copy status: open\n", progname);
				
					//INVIO FILE A BLOCCHI
					int size_buf;
					
					//per identificare se file grande o piccolo
					if(file_size > CHUNK)
						size_buf=CHUNK;
					else
						size_buf=file_size;
						
					
					 while((nread =read(mysocket, buf, size_buf)) > 0) {

						fwrite(buf, sizeof(char),nread,fp);

						if(sigpipe){
						  printf("(%s) - Client closed socket\n", progname);
						  fclose(fp);
						  return -1;
						}

						file_size= file_size-nread;

						if(file_size > CHUNK)
							size_buf=CHUNK;
						else
							size_buf=file_size; //(ultimo blocco da leggere e scrivere)

      					}
				

				n=fclose(fp);

				if(n!=0){ printf("Error on close file! \n"); return -1;}

				else{

					printf("(%s) --- received and wrote file '%s'\n",progname,filename2);
					return 0;
				
				}

			}else {
				printf("(%s) --- cannot open file '%s'\n",progname, filename);
				return -1;
			}

}


int file_socket_send(int mysocket, char* buf,char* filename, char* progname){

	struct stat info;
	int ret = stat(filename, &info);
	int nread;

	if (ret == 0) {//success

		FILE *fp;
		char filename2[MAXBUFL];
		strcpy(filename2,filename);
		if ( (fp=fopen(filename, "r+")) != NULL) {//fname deprecated
					
			//NB: strlen, not sizeof(MSG_OK), otherwise the '\0' byte will create problems when receiving data 
			write (mysocket, MSG_OK, strlen(MSG_OK) );
			printf("(%s) - sent to the client: %s \n",progname,MSG_OK);

			//send the file size as uint_32
			int file_size = info.st_size;
			uint32_t val_size = htonl(file_size);
			write(mysocket, &val_size, sizeof(file_size));
			printf("(%s) - sent '%d' - converted in network order - to client\n",progname, file_size);
					
			//send timestamp
			struct tm * timeinfo;
			int time= info.st_mtime;
			timeinfo = localtime(&info.st_mtime);
			uint32_t val_time = htonl(time);
			write(mysocket, &val_time, sizeof(time));
			strftime (buf,MAXBUFL,"Last file modification: %Y-%m-%d %H:%M:%S.\n",timeinfo);
			printf("(%s) --- converted in network order and sent to client: %s\n", progname,buf);

			//INVIO FILE A BLOCCHI
			int size_buf;
					
			//per identificare se file grande o piccolo
			if(file_size > CHUNK)
				size_buf=CHUNK;
			else
				size_buf=file_size;
						
					
			 while((nread = fread(buf, sizeof(char), size_buf, fp)) > 0) {

				writen(mysocket, buf, nread);

				if(sigpipe) {
				  printf("(%s) - Client closed socket\n", progname);
				  fclose(fp);
				  return -1;
				}

				file_size= file_size-nread;

				if(file_size > CHUNK)
					size_buf=CHUNK;
				else
					size_buf=file_size; //(ultimo blocco da leggere e scrivere)

      			}
      			
			printf("(%s) --- sent file '%s' to client\n",progname,filename2);
			fclose(fp);
			
			return 0;
					
			} else {
					return -1;
				}
			}
			else{	//ret fail
				write (mysocket, MSG_ERR, strlen(MSG_ERR) );
				return -1;
			}

}



int ftp_sender(int mysocket, char* progname) {

	char buf[MAXBUFL+1]; /* +1 to make room for \0 */

	int ret_val = 0;
	int nread = 0;


	while (1) {
		printf("(%s) - waiting for commands...\n",progname);
		
		ret_val=read_socket_message(mysocket,buf);
		
		if(ret_val==0){ return 0;}

		remove_end_specials(buf);
		
		printf("(%s) - received string '%s'\n",progname, buf);
		
		// get the command, compare n first characters 
		if (strncmp(buf,MSG_GET,strlen(MSG_GET))==0) {
			char fname[MAX_STR+1];
			char file_name[MAX_STR+1];
			strcpy(fname, buf+4);// GET namefile.txt\r\n
			strcpy(file_name,fname);
			printf("(%s) - client asked to send file '%s'\n",progname,fname);

			 //stat return the info about the file                ret

//---------------------------------------------------------------------------------------------------------------------
/* struct stat info;

 dev_t       st_dev;     ID del dispositivo contenente file  
 ino_t       st_ino;     numero dell'inode 
 mode_t      st_mode;    protezione  
 nlink_t     st_nlink;   numero di hard links 
 uid_t       st_uid;     user ID del proprietario 
 gid_t       st_gid;     group ID del proprietario 
 dev_t       st_rdev;    ID del dispositivo (se il file speciale) 
 off_t       st_size;    dimensione totale, in byte  
 blksize_t   st_blksize; dimensione dei blocchi di I/O del filesystem 
 blkcnt_t    st_blocks;  numero di blocchi assegnati  
 time_t      st_atime;   tempo dell'ultimo accesso 
 time_t      st_mtime;   tempo dell'ultima modifica 
 time_t      st_ctime;   tempo dell'ultimo cambiamento 

*/
//----------------------------------------------------------------------------------------------------------------------

		ret_val= file_socket_send(mysocket,buf,fname,progname);
		
			
		} else if (nread >= strlen(MSG_QUIT) && strncmp(buf,MSG_QUIT,strlen(MSG_QUIT))==0) {
			printf("(%s) --- client asked to terminate connection\n",progname);
			ret_val = 1;
			break;
		} else {
			write (mysocket, MSG_ERR, strlen(MSG_ERR) );
		}

	}
	return ret_val;
}

int ftp_receiver(int mysocket, char* progname){

	char fname[MAX_STR];
	char garbage[MAX_STR];
	fd_set mysockets;
	struct timeval tv;
	tv.tv_sec = TIMEOUT;       
	tv.tv_usec = 0;
	char buf[MAXBUFL+1];
	int n,err;
	char fnamestr[MAX_STR+1];
	
	while(1){
	
	read_keyboard(buf,"GET/QUIT",progname);

        if(strncmp(buf,MSG_QUIT,strlen(MSG_QUIT))==0){ break; } //EXIT IF I RECEIVE QUIT COMMAND

	else if(strncmp(buf,MSG_GET,strlen(MSG_GET))==0){ 
	
		sscanf(buf,"%s %s",garbage,fname);// retrieve the name of the file

		add_end_specials(buf);
	
		write(mysocket, buf, strlen(buf));  //send command without terminator

		printf("(%s) - data has been sent\n",progname);

		FD_ZERO(&mysockets);
		FD_SET(mysocket, &mysockets);

		if ((n=select(FD_SETSIZE, &mysockets, NULL, NULL, &tv))==-1) {
			fprintf(stderr," select() - failed\n");
		}
		else if(n>0){
			
			read_socket_message(mysocket,buf);
			printf("(%s) --- received message: %s \n", progname, buf);
		
			//MSG_OK defined as +OK\n\r

			if (strncmp(buf,MSG_OK,strlen(MSG_OK))==0) {
				//the server has sent a line formatted as the protocol

			
				sprintf(fnamestr, "copy_of_%s", fname);
			
				err=file_socket_receive(mysocket, buf, fnamestr, progname);
				
				if(err==0){ 
					return 0;
				}
				else{
					return -1;
				}

			} //NOT OK
			else {
				printf("(%s) - protocol error: received response '%s'\n",progname,  buf);
				return -1;
			}
		} 
		else {//TIMEOUT---BLOCCATO
			printf("Timeout waiting for an answer from server\n'");
		}

	}// if command== GET
	else{ 
		printf("ATTENTION !!! INVALID COMMAND !!! ERROR !!! \n");}
		return -1;
	}//close while 
	
	return 0;
}

int ftp_receiver_multiplexing(int mysocket, char* progname){

	char command[MAXBUFL];
	char buf[MAXBUFL+1];
	int flag_no_file=0;
	int flag_reading_file = 0;
	int flag_end = 0;
	char fnamestr[MAX_STR+1];
	int filecnt = 0;  // this is needed to generate a new filename for writing
	FILE *fp;  // need to keep the info between different iterations in the next while
	int file_ptr;  // need to keep the info between different iterations in the next while
	uint32_t file_bytes;  // need to keep the info between different iterations in the next while

	
	printf("\n (%s) Client ready, waiting for commands (GET file,Q,A):\n",progname);
	
	while (1) {

		if (flag_reading_file == 0 && flag_end == 1) {//when i press Q, end to read last bytes then i must return
			return 0;  
		}

		fd_set rset;
		FD_ZERO(&rset);
		FD_SET(mysocket, &rset);
		FD_SET(fileno(stdin), &rset);  // Set stdin
		/* mysocket+1 is the maximum since the others in which we are interested are 0,1,2 which are always less than mysocket */
		
		Select(mysocket+1, &rset, NULL, NULL, NULL); // This will block until something happens
		

/*SERVER*/	if (FD_ISSET(mysocket, &rset)) {
			// socket is readable

	
			if (flag_reading_file == 0) { //NO READING FILE
				int nread = 0; char c;
				do {
					// NB: do NOT use Readline since it buffers the input
					Read(mysocket, &c, sizeof(char));
					buf[nread++]=c;
				} while (c != '\n' && nread < MAXBUFL-1);
				
				buf[nread]='\0';

				//printf("nread=%d\n", nread);
				//err_msg("(%s) --- received string '%s'",progname, buf);
                        
				if (nread >= strlen(MSG_OK) && strncmp(buf,MSG_OK,strlen(MSG_OK))==0) {
					
					flag_no_file=0;
					
					// Use a number, otherwise the requested filenames should be remembered
					sprintf(fnamestr, "download_%d.txt", filecnt);
					filecnt++;
					
					//read file_dimension
					int n = Read(mysocket, buf, 4);
					if(n<=0){ err_msg("(%s) --- error file_size read",progname);}
					file_bytes = ntohl((*(uint32_t *)buf));
					err_msg("(%s) --- received file size '%u'",progname, file_bytes);

					//read time_stamp
					uint32_t val_time;
					struct tm * timeinfo;
					int time;		
					n= Read(mysocket, buf, 4);
					if(n<=0){ err_msg("(%s) --- error file_time read",progname);}
					val_time= (*(uint32_t *)buf);
					time = ntohl(val_time);
					time_t time_format=time;
					timeinfo = localtime(&time_format);
					strftime (buf,MAXBUFL,"Last file modification: %Y-%m-%d %H:%M:%S.",timeinfo);
					err_msg("(%s) --- received time information '%s'",progname, buf);
                        		
					//file open
					if ( (fp=fopen(fnamestr, "w+"))!=NULL) {
						err_msg("(%s) --- opened file '%s' for writing",progname, fnamestr);
						file_ptr = 0;   //COUNTER THAT AT THE END OF TRANSFER MUST BE EQUAL TO SIZE OF THE FILE
						flag_reading_file = 1; //I' M READING!!!!
						
						printf("\n (%s) Client ready, waiting for commands (GET file,Q,A):\n",progname);
					} else {
						err_quit("(%s) --- cannot open file '%s'",progname, fnamestr);
					}
				} else if (nread >= strlen(MSG_ERR) && strncmp(buf,MSG_ERR,strlen(MSG_ERR))==0) {
					err_msg("(%s) - received '%s' from server: maybe a wrong request?", progname, buf);
					flag_no_file=1;
					
					printf("\n (%s) Client ready, waiting for commands (GET file,Q,A):\n",progname);
				} else {
					flag_no_file=1;
					printf("(%s) Error Protocol, received from server plus TCP packets:\n",progname);
					break;
				}


			} else if (flag_reading_file==1) {  //READ OPERATION
				char c;
				Read (mysocket, &c, sizeof(char));
				fwrite(&c, sizeof(char), 1, fp);
				file_ptr++;
				
				if(sigpipe){
					printf("(%s) - Client closed socket\n", progname);
				  	fclose(fp);
				   	return -1;
				}
									
				//CLOSE FILE WITH CONTROL ON DIMENSION-- CAUSE IF I'WAITING SOME BYTES I CANNOT CLOSE
				if (file_ptr == file_bytes) {
					fclose(fp);
					err_msg("(%s) --- received and wrote file '%s'",progname, fnamestr);
					flag_reading_file = 0;
					
					printf("\n (%s) Client ready, waiting for commands (GET file,Q,A):\n",progname);
				}

			} else {
				err_msg("(%s) - flag_reading_file error '%d'", progname, flag_reading_file);
				break;
			}
		}
/*KEYBOARD*/    if (FD_ISSET(fileno(stdin), &rset)) {

			if(sigpipe){
				  printf("(%s) - Client closed socket\n", progname);
				  return -1;
			}
			
			// standard input is readable, fgets read until \n or EOF
			if ( (fgets(command, MAXBUFL, stdin)) == NULL) { 
				// Ask the server to terminate if EOF is sent from stdin
				strcpy(command,MSG_Q);
			}
			else if( (strncmp(command,MSG_GET,strlen(MSG_GET))==0) && (command[strlen(MSG_GET)]==' ')){

				err_msg("(%s) --- GET command sent to server",progname);
				// TODO: here send to server the GET filename
				Write(mysocket, command, strlen(command));

			}
			else if((strncmp(command,MSG_Q,strlen(MSG_Q))==0) && (strlen(MSG_A)==(strlen(command)-1))){

				printf("(%s) --- Q command received\n",progname);
				// TODO: here quit only after the file was succesful transfered
				strcpy(command, MSG_QUIT);
				Write(mysocket, command, strlen(command));
				
				Shutdown(mysocket, SHUT_WR); //SHUT_WR disable send operation, i can still read and then the socket will be closed
				//SHUT_RDWR block read, write streaming equal to close
				flag_end = 1;
				err_msg("(%s) - waiting for file tranfer to terminate", progname);
				
				//if(flag_no_file==0)
				//	fclose(fp);
					
				//return 0;

			}
				
			else if((strncmp(command,MSG_A,strlen(MSG_A))==0) && (strlen(MSG_A)==(strlen(command)-1))){
				
				printf("(%s) --- A command received\n", progname);
				// TODO: here quit during the file transfer
				Close (mysocket);   // This will give "connection reset by peer at the server side
				if(flag_no_file==0)
					fclose(fp);
				err_msg("(%s) - exiting immediately", progname);
				return 0;
			}
			else{ 	
				err_msg("(%s) --- INVALID COMMAND FORMAT, waiting for commands (GET file,Q,A):\n",progname);
			}

// NB. i'm using strlen(command)-1 cause in the command i have the \n !!!
			
		}//keyboard
	}//while
	return -1;
}


