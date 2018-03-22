#ifndef SOCKMANAGER_H
#define SOCKMANAGER_H

#include "sockwrap.h"
#define ADDRLEN 128

//Struttura per tcp e udp
typedef struct {

    size_t socket_id;
    size_t buf_len;

    char *rx_buf;
    char *tx_buf;

    char dest_IP[ADDRLEN];
    struct sockaddr_in dest_addr, local_addr;
    struct in_addr dest_IPaddr;

    uint16_t dest_port;
    uint16_t dest_port_net;

    // eventi legati al socket
    fd_set rset;
    fd_set wset;
    fd_set eset;

} ClientSocket;

// funzioni

ClientSocket * s_init(size_t buflen);
void s_end(ClientSocket *sh);
void s_connect(ClientSocket *sh);
void s_stdin_send(ClientSocket *sh);
void s_setDestination(ClientSocket *sh, struct sockaddr_in * dest);

void tcp_createSocket(ClientSocket *sh, const char *s_addr, const char *s_port);

void udp_createSocket(ClientSocket *sh, const char *s_addr, const char *s_port);
void udp_recvAll(ClientSocket *sh, int len, struct sockaddr_in * sender);
void udp_localConfig(ClientSocket *sh, uint32_t local_if, const char *s_port);


//FUNZIONI EXTRA
void read_stdin(char* line, int len);
int control_parameters_stdin(char* line);


#endif
