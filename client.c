#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define buff_size 64
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Utilisation : ./client <ip_serveur> <port_serveur>\n");
        return 0;
    }
    int sock;
    struct sockaddr_in my_addr;

    // Initializing sockets
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    // Error handling
    if (sock < 0)
    {
        printf("Error creation socket UDP\n");
        exit(1);
    }

    memset((char *)&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &my_addr.sin_addr);
    my_addr.sin_addr.s_addr = htonl(my_addr.sin_addr.s_addr);
    uint taille = sizeof(my_addr);

    char msg[buff_size] = "SYN";
    char msg_rcv[buff_size];
    char msg_ack[buff_size] = "ACK";
    char * msg_port;
    char msg_test[buff_size] = "hi";
    int portmess;
    int i;

    sendto(sock, msg, buff_size, 0, (struct sockaddr *)&my_addr, taille);
    recvfrom(sock, (char *) msg_rcv, buff_size, MSG_WAITALL, (struct sockaddr *)&my_addr, &taille);
    printf("Message on UDP socket : %s\n", msg_rcv);
    msg_port = strtok(msg_rcv, "K");
    msg_port = strtok(NULL, "K");
    printf("port: %s\n", msg_port);
    sendto(sock, msg_ack, buff_size, 0, (struct sockaddr *)&my_addr, taille);

    //MAJ addr
    portmess = atoi(msg_port);
    printf("New Port : %d\n", portmess);
    my_addr.sin_port = htons(portmess);

    sendto(sock, msg_test, buff_size, 0, (struct sockaddr *)&my_addr, taille);


    printf("End \n");

}