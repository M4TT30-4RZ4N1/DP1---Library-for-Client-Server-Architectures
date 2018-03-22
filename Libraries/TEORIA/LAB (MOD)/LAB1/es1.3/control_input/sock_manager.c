#include <stdlib.h>
#include <string.h>
#include "sock_manager.h"
#include "errlib.h"


//stampo a video l'indirizzo ipv4 in formato stringa
void showAddr(char *str, struct sockaddr_in *a)
{
    char *p;

    p = inet_ntoa(a->sin_addr);
	printf("%s %s", str, p);
	printf("!%u\n", ntohs(a->sin_port));
}

//inizializzo
ClientSocket *s_init(size_t buflen) {

	ClientSocket *cs = (ClientSocket *) malloc(sizeof(ClientSocket));

	cs->buf_len = buflen;
	cs->rx_buf = (char *) malloc(buflen);
	cs->tx_buf = (char *) malloc(buflen);

	return cs;
}

//libero memoria allocata e chiudo socket
void s_end(ClientSocket *sh) {
	
	free(sh->rx_buf);
	free(sh->tx_buf);
	Close(sh->socket_id);
	free(sh);
}

//connessione al socket
void s_connect(ClientSocket *sh) {

	Connect(sh->socket_id, (struct sockaddr *) &sh->dest_addr, sizeof(sh->dest_addr));
    	printf("Connection: done.\n");

}

//invio dei dati da STDIN--> SOCKET
void s_stdin_send(ClientSocket *sh) {

	fgets(sh->tx_buf, sh->buf_len, stdin);
	int len = strlen(sh->tx_buf);

	Writen(sh->socket_id, sh->tx_buf, len);
}

//associo la destinazione(server) al socket lato client
void s_setDestination(ClientSocket *sh, struct sockaddr_in * dest) {
	sh->dest_addr = *dest;
}

//creo  socket-TCP
void tcp_createSocket(ClientSocket *sh, const char *s_addr, const char *s_port) {

	// compatibilità IPV4
    int result = inet_aton(s_addr, &sh->dest_IPaddr);
    if (!result)
 	  err_quit("Invalid address");
	strcpy(sh->dest_IP, s_addr);

	//porta, in linux >= 1500 (range)
	sh->dest_port = atoi(s_port);
	if(sh->dest_port < 0)
	  err_quit("Invalid port");
	sh->dest_port_net = htons(sh->dest_port);

	//creazione del socket-TCP
    printf("Creating socket\n");
    sh->socket_id = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    printf("done. Socket fd number: %d\n", (int) sh->socket_id);

	// preparo la struttura di destinazione
    bzero(&sh->dest_addr, sizeof(sh->dest_addr));
    sh->dest_addr.sin_family = AF_INET;
    sh->dest_addr.sin_port   = sh->dest_port_net;
    sh->dest_addr.sin_addr   = sh->dest_IPaddr;
}

//creo socket-UDP
void udp_createSocket(ClientSocket *sh, const char *s_addr, const char *s_port) {

	// devo riempire (sockaddr_in).(in_addr).(unsigned long) --> (*)
	uint32_t tmp = inet_addr(s_addr);
    if (tmp == INADDR_NONE)
		err_sys("Invalid address");

	strcpy(sh->dest_IP, s_addr);

	sh->dest_port = atoi(s_port);
	if(sh->dest_port < 0)
	  err_quit("Invalid port");
	sh->dest_port_net = htons(sh->dest_port);

	// creo il socket-UDP
    printf("Creating socket\n");
    sh->socket_id = Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    printf("done. Socket fd number: %d\n",(int) sh->socket_id);

	// preparo la struttura di destinazioneaddress structure
    bzero(&sh->dest_addr, sizeof(sh->dest_addr));
    sh->dest_addr.sin_family = AF_INET;
    sh->dest_addr.sin_port   = sh->dest_port_net;
    sh->dest_addr.sin_addr.s_addr = tmp; // (*) <---

}

// specifico interfaccia e comportamento del server
void udp_localConfig(ClientSocket *sh, uint32_t local_if, const char *s_port) {

    if (local_if == INADDR_NONE)
		err_sys("Invalid address");

	uint16_t port = atoi(s_port);
	if(port < 0)
	  err_quit("Invalid port");
	uint16_t portn = htons(port);

	// creo socket
    printf("Creating socket\n");
    sh->socket_id = Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    printf("Create socket: done. Socket fd number: %d\n",(int) sh->socket_id);

	// preparo struttura sorgente
    bzero(&sh->local_addr, sizeof(sh->local_addr));
    sh->local_addr.sin_family = AF_INET;
    sh->local_addr.sin_port   = portn;
    sh->local_addr.sin_addr.s_addr = INADDR_ANY;

	// bind alla struttura locale
	showAddr("Binding to address", &sh->local_addr);
    Bind(sh->socket_id, (struct sockaddr *) &sh->local_addr, sizeof(sh->local_addr));
    printf("Bind: done.\n");

}

void udp_recvAll(ClientSocket *sh, int len, struct sockaddr_in * sender) {

    //in udp uso un socket solo per contattare più server, devo identificare/specificare il server
    socklen_t fromlen = sizeof(struct sockaddr_in);

    // Receive datagram
    int n = Recvfrom(sh->socket_id, sh->rx_buf, len, 0, (struct sockaddr *) sender, &fromlen);

    sh->rx_buf[n] = '\0';

    showAddr("Received response from", sender);
    printf(": [%s]\n", sh->rx_buf);

}

/* FUNZIONI EXTRA */

void read_stdin(char* line, int len){
	
	char c;
	int i=0;
	
	while((c=fgetc(stdin))!='\n'){
		line[i]=c;
		i++;
		if(i>len){
		   break;
		}	
	}
	
	
}
//funzione per contare il numero di parametri separati da spazio digitati
int control_parameters_stdin(char* line){

	int l= strlen(line);
	int i=0;
	char prec, curr;
	int n=0;

	
	prec= line[0];
	for(i=1; i<l; i++){ //ogni volta che trovo un carattere con dopo uno spazio o
		curr=line[i];
		if(prec!=' ' && curr==' '){//fine parola
			n++;
		}
		if(curr!=' ' && i==l-1){ //devo contare l'ultima parola
			n++;
		}
		prec=curr;
	}
	
	return n;
}
