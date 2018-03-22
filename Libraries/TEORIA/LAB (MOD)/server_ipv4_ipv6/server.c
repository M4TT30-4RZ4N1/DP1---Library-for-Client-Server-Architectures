
/**************************************************************************/
/* Header files needed for this sample program                            */
/**************************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define TCP "TCP"
#define UDP "UDP"

/**************************************************************************/
/* Constants used by this program                                         */
/**************************************************************************/
#define SERVER_PORT     1500
#define BUFFER_LENGTH    250
#define FALSE              0

// in main() before call this: do->  struct sockaddr_in6 serveraddr |memset(&serveraddr, 0, sizeof(serveraddr));|sd= init_Server(TCP, &serveraddr);
//this function initialize socket, bind using ipv6
// an ipv6 server is able to answer to ipv4 clients using ipv4-mapped
//pointer is: &serveraddr
int init_Server_dual_stack(char* protocol,struct sockaddr_in6* pointer){

	  int on=1;
	  int sd;
	  
//create socket TCP/UDP
	if(strcmp(protocol,TCP)==0)
	{
		if ((sd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
	      {
		 perror("socket() failed");
		 return -1;
	      }
	}
	else if(strcmp(protocol,UDP)==0)
	{
		if ((sd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
	      {
		 perror("socket() failed");
		 return -1;
	      }
	}
	else
	{
		perror("socket() failed");
		return -1;
	}

      
      if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,(char *)&on,sizeof(on)) < 0)
      {
         perror("setsockopt(SO_REUSEADDR) failed");
         
      }

      // an IPV6 server with dual-stack can:
      // recevice IPV6 pakcets and send back on IPV6 address
      // recevice IPV4 packets and send back on IPV4-mapped address
      pointer->sin6_family = AF_INET6;
      pointer->sin6_port   = htons(SERVER_PORT);
      pointer->sin6_addr   = in6addr_any;
      
      if (bind(sd, (struct sockaddr *) pointer,sizeof(*pointer)) < 0)
      {
         perror("bind() failed");
         return -1;
      }


      return sd;
}

//in main() do:
// struct sockaddr_in6 clientaddr
//  int addrlen=sizeof(clientaddr); | print_client_address(sdconn,&clientaddr, &addrlen);
//pointer is: &clientaddr
void print_client_ipv6_address_tcp(int accepted_socket,struct sockaddr_in6* pointer, int* size){

	 char str[INET6_ADDRSTRLEN];
	   
	 getpeername(accepted_socket, (struct sockaddr *) pointer, size);
         if(inet_ntop(AF_INET6, &pointer->sin6_addr, str, sizeof(str))) {
            printf("Client address is %s\n", str);
            printf("Client port is %d\n", ntohs(pointer->sin6_port));
         }
}

void main()
{
   /***********************************************************************/
   /* Variable and structure definitions.                                 */
   /***********************************************************************/
   int sd=-1, sdconn=-1;
   struct sockaddr_in6 serveraddr, clientaddr;
   

	 
      memset(&serveraddr, 0, sizeof(serveraddr));
      sd= init_Server_dual_stack(TCP, &serveraddr);
      /*
      if i call in the main():
      sd= init_Server(UDP, &serveraddr);
      NB: [NO LISTEN, NO ACCEPT]
       */   
      if (listen(sd, 10) < 0)
      {
         perror("listen() failed");
         return -1;
      }

      printf("Ready for client connect().\n");
      do
   	{
      
      if ((sdconn = accept(sd, NULL, NULL)) < 0)
      {
         perror("accept() failed");
         break;
      }
      else
      {
         /*****************************************************************/
         /* Display the client address.  Note that if the client is       */
         /* an IPv4 client, the address will be shown as an IPv4 Mapped   */
         /* IPv6 address.                                                 */
         /*****************************************************************/
       
         int addrlen=sizeof(clientaddr);
         print_client_ipv6_address_tcp(sdconn,&clientaddr, &addrlen);
      }

      printf("...Protocol...\n");

     

   } while (FALSE);

   /***********************************************************************/
   /* Close down any open socket descriptors                              */
   /***********************************************************************/
   if (sd != -1)
      close(sd);
   if (sdconn != -1)
      close(sdconn);
}


