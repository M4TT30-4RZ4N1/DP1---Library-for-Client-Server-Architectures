
/* THIS IS A PROTOCOLS HEADR FILE CREATED BY Matteo Arzani - Politecnico di Torino */

int file_socket_send(int mysocket, char* buf,char* filename, char* progname);

int file_socket_receive(int mysocket, char* buf,char* filename, char* progname);

int ftp_sender(int mysocket, char* progname);

int ftp_receiver(int mysocket, char* progname);

int ftp_receiver_multiplexing(int mysocket, char* progname);
