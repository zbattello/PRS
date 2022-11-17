#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int max(int x, int y)
{
    if (x > y)
        return x;
    else
        return y;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Utilisation : ./server <port UDP> <port UDP mess>\n");
        exit(0);
    }

    int udp_sock, udp_mess, maxfdp1;
    int reuse = 1;
    struct sockaddr_in my_addr_udp, mess_udp;
    fd_set f_des;

    // initialisation socket UDP

    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    udp_mess = socket(AF_INET, SOCK_DGRAM, 0);

    if (udp_sock < 0 || udp_mess < 0)
    {
        printf("Error creation socket UDP\n");
        exit(0);
    }

    printf("Socket UDP : %d\n", udp_sock);
    memset((char *)&my_addr_udp, 0, sizeof(my_addr_udp));
    my_addr_udp.sin_family = AF_INET;
    my_addr_udp.sin_port = htons(atoi(argv[1]));
    my_addr_udp.sin_addr.s_addr = INADDR_ANY;
    setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    printf("Socket UDP : %d\n", udp_mess);
    memset((char *)&mess_udp, 0, sizeof(mess_udp));
    mess_udp.sin_family = AF_INET;
    mess_udp.sin_port = htons(atoi(argv[2]));
    mess_udp.sin_addr.s_addr = INADDR_ANY;
    setsockopt(udp_mess, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    bind(udp_sock, (struct sockaddr *)&my_addr_udp, sizeof(my_addr_udp));
    bind(udp_mess, (struct sockaddr *)&mess_udp, sizeof(mess_udp));

    // structure pour récupérer infos clients
    struct sockaddr_in c_addr;
    int c_addr_size = sizeof(c_addr);


    // get max 
    maxfdp1 = max(udp_sock, udp_mess) + 1;

    while (1)
    {
        // clear the descriptor set        
        FD_ZERO(&f_des);
        FD_SET(udp_sock, &f_des);
        FD_SET(udp_mess, &f_des);

        // On surveille les deux sockets maintenant
        printf("Waiting for messages or connections\n");

        select(maxfdp1, &f_des, NULL, NULL, NULL);

        printf("Connection/message detected \n");

        if (FD_ISSET(udp_sock, &f_des))
        {
            printf("UDP connection detected \n");
            
            char msg_udp[8];
            recvfrom(udp_sock, (char *)msg_udp, 8, MSG_WAITALL, (struct sockaddr *)&c_addr, &c_addr_size);
            printf("Message on UDP socket : %s\n", msg_udp);
            char msg_synack[8] = "SYNACK";
            char msg_port[8];
            sprintf(msg_port, "%d", atoi(argv[2]));
            char msg_ack[8];
            if (strcmp(msg_udp, "SYN")==0){
                sendto(udp_sock, msg_synack, 8, 0, (struct sockaddr *)&c_addr, c_addr_size);
                sendto(udp_sock, msg_port, 8, 0, (struct sockaddr *)&c_addr, c_addr_size);
                recvfrom(udp_sock, (char *)msg_ack, 8, MSG_WAITALL, (struct sockaddr *)&c_addr, &c_addr_size);
                printf("Message on UDP socket : %s\n", msg_ack);
            
            }
        }

        if (FD_ISSET(udp_mess, &f_des))
        {
            printf("UDP message detected\n");

            char msg[8];
            recvfrom(udp_mess, (char *)msg, 8, MSG_WAITALL, (struct sockaddr *)&c_addr, &c_addr_size);
            printf("Message on UDP socket messages : %s\n", msg);
        }
        
    };
}