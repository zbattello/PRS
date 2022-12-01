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

#define RCVSIZE 1494

int max(int x, int y)
{
    if (x > y)
        return x;
    else
        return y;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Utilisation : ./server <port UDP> \n");
        exit(0);
    }
    int random_number;
    char msg_synack[7] = "SYN-ACK";
    char synAckBuff[64];
    char string2[8];

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

    bind(udp_sock, (struct sockaddr *)&my_addr_udp, sizeof(my_addr_udp));

    // structure pour récupérer infos clients
    struct sockaddr_in c_addr;
    int c_addr_size = sizeof(c_addr);


    // get max 
    maxfdp1 = max(udp_sock, udp_mess) + 1;

    while (1)
    {
        random_number = rand() % 4444 + 3333;
        printf("number : %d\n", random_number);

        printf("Socket UDP Mess: %d\n", udp_mess);
        memset((char *)&mess_udp, 0, sizeof(mess_udp));
        mess_udp.sin_family = AF_INET;
        mess_udp.sin_port = htons(random_number);
        mess_udp.sin_addr.s_addr = INADDR_ANY;
        setsockopt(udp_mess, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        bind(udp_mess, (struct sockaddr *)&mess_udp, sizeof(mess_udp));

        // clear the descriptor set        
        FD_ZERO(&f_des);
        FD_SET(udp_sock, &f_des);
        FD_SET(udp_mess, &f_des);

        // On surveille les deux sockets maintenant
        printf("Waiting for messages or connections\n");

        select(maxfdp1, &f_des, NULL, NULL, NULL);

        printf("Connection/message detected \n");

        snprintf(string2, 6, "%04d", random_number);
        memcpy(synAckBuff, msg_synack, 7);
        memcpy(synAckBuff + 7, string2, 7);

        printf("Message on synack: %s\n", synAckBuff);

        if (FD_ISSET(udp_sock, &f_des))
        {
            printf("UDP connection detected \n");
            
            char msg_udp[8];
            recvfrom(udp_sock, (char *)msg_udp, 8, MSG_WAITALL, (struct sockaddr *)&c_addr, &c_addr_size);
            printf("Message on UDP socket : %s\n", msg_udp);

            char msg_ack[8];
            if (strcmp(msg_udp, "SYN")==0){
                sendto(udp_sock, synAckBuff, RCVSIZE, 0, (struct sockaddr *)&c_addr, c_addr_size);
                recvfrom(udp_sock, (char *)msg_ack, 8, MSG_WAITALL, (struct sockaddr *)&c_addr, &c_addr_size);
                printf("Message on UDP socket : %s\n", msg_ack);
            
            }
        }

        if (FD_ISSET(udp_mess, &f_des))
        {
            printf("UDP message detected\n");

            char file_name[64];
            recvfrom(udp_mess, (char *)file_name, 64, MSG_WAITALL, (struct sockaddr *)&c_addr, &c_addr_size);
            printf("File path : %s\n", file_name);

            // Overture du fichier a envoyer
            FILE* fichier = NULL;
            fichier = fopen(file_name, "r");

            if (fichier == NULL)
            {
                printf("Erreur ouverture fichier\n");
                exit(0);
            }

            // Lecture du fichier et generations des sequences

            int numSequence = 0; //Le numero de la sequence

            int caractere;
            int k;

            do
            {
                numSequence++;
                char sequence[256] = {0}; //La sequence a transmettre
                char strSequence[6]= {0}; //Le numero en format string

                // Convertion du num de sequence en string
                sprintf(strSequence, "%d", numSequence);

                // Ajout du num de sequence dans les 6 premiers octets
                for (k = 0; k < 6; k++)
                {
                    sequence[k] = (strSequence[k] != 0) ? strSequence[k] : 48;
                }

                // Replissage de la sequence avec les octets a envoyer
                do
                {
                    caractere = fgetc(fichier);
                    sequence[k] = caractere;
                    k++;
                } while (caractere != EOF && k < 256);

                // Envoie de la sequence
                //printf("SEQUENCE:\n%s\n", sequence);

                // TODO : attendre l'aquitement de la sequence

            } while (caractere != EOF);

            //Envoie de la sequence de fin
            printf("FIN\n");

            //Fermeture du fichier
            fclose(fichier);
        }
        
        
    };
}