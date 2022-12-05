#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#define RCVSIZE 1024
#define windowSize 1
#define TIMEOUT 200000


int main (int argc, char *argv[])
{
    //timeout
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT;

    //fdset
    fd_set readfs;
    FD_ZERO(&readfs);

    // Buffer de reception publique
    char msgBufferPublic[RCVSIZE];

    // Buffer de lecture du fichier
    char fileBuffer[RCVSIZE - 6];

    // Adresse publique
    struct sockaddr_in public_adress;
    socklen_t alen = sizeof(public_adress);

    // Adresse client
    struct sockaddr_in client_adress;

    if (argc != 2)
    {
        printf("Utilisation : ./server <port>\n");
        return -1;
    }

    // Obtention du port publique
    int public_port = atoi(argv[1]);

    // Creation de la socket publique
    int public_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (public_socket < 0)
    {
        perror("Erreur creation socket publique.\n");
        return -1;
    }

    // Permet a la socket d'etre reutilise immediatement en cas de redemarage
    int option = 1;
    setsockopt(public_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    // Initialisation de l'adresse publique
    public_adress.sin_family = AF_INET;
    public_adress.sin_addr.s_addr = INADDR_ANY;
    public_adress.sin_port = htons(public_port);

    // Connexion socket et adresse publique
    if (bind(public_socket, (struct sockaddr *)&public_adress, sizeof(public_adress)) < 0)
    {
        perror("Erreur connexion socket-adresse publique.\n");
        close(public_socket);
        return -1;
    }

    // Port unique du client
    int client_port = 8000;
    char str_client_port[4];

    while (1)
    {
        // Creation de la socket client
        int client_socket = socket(AF_INET, SOCK_DGRAM, 0);

        if (client_socket < 0)
        {
            perror("Erreur creation socket client.\n");
            return -1;
        }

        // Permet a la socket client d'etre reutilise immediatement en cas de redemarage
        setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

        printf("Attente d'une demande de connexion d'un client sur le port %d...\n", public_port);

        while ((strcmp(msgBufferPublic, "SYN") != 0))
        {
            recvfrom(public_socket, msgBufferPublic, RCVSIZE, 0, (struct sockaddr *)&public_adress, &alen);
        }

        printf("Demande de connexion recue.\n");

        char syn_ack[11] = "SYN-ACK";

        // Port unique du client
        // incremente de 1 a chaque nouvelle connexion
        client_port += 1;

        // Convertion numero de port en string
        sprintf(str_client_port, "%04d", client_port);

        printf("Port attribue au client : %s.\n", str_client_port);

        // Initialisation de l'adresse client
        client_adress.sin_family = AF_INET;
        client_adress.sin_addr.s_addr = INADDR_ANY;
        client_adress.sin_port = htons(client_port);

        // Connexion socket et adresse client
        if (bind(client_socket, (struct sockaddr *)&client_adress, sizeof(client_adress)) < 0)
        {
            perror("Erreur connexion socket-adresse client");
            close(client_socket);
            close(public_socket);
            return -1;
        }

        // Concatenation de SYN-ACK et du numero de port
        strcat(syn_ack, str_client_port);

        // Envoie du SYN-ACK avec le numero de port client
        sendto(public_socket, syn_ack, 11, 0, (struct sockaddr *)&public_adress, alen);

        printf("Attente confirmation du client...\n");

        while ((strcmp(msgBufferPublic, "ACK") != 0))
        {
            recvfrom(public_socket, msgBufferPublic, 3, 0, (struct sockaddr *)&public_adress, &alen);
        }

        printf("Communication etablie.\n");

        // Division du processus pour prendre en charge la multiconnexion
        int n = fork();

        if (n != 0) // Si on est dans le processus parent
        {
            close(client_socket); // On ferme la socket client et on reitere la boucle
        }
        else // Sinon on est dans le processus enfant
        {
            close(public_socket); // On ferme la socket publique

            // Buffer de reception publique
            char msgBufferClient[RCVSIZE];

            // Attente de reception du nom du fichier demande
            recvfrom(client_socket, msgBufferClient, RCVSIZE, 0, (struct sockaddr *)&client_adress, &alen);

            printf("Demande recu du fichier %s.\n", msgBufferClient);

            // Overture du fichier a envoyer
            FILE* file = NULL;
            file = fopen(msgBufferClient, "rb"); //Ouverture binaire

            if (file == NULL)
            {
                printf("Erreur ouverture du fichier.\n");
                return -1;
            }

            printf("Envoie du fichier...\n");

            // Le numero de la trame qui sera incremente
            int number_segment = 0;

            // Segment d'aquitement
            char ack_segment[9];

            // Trame a envoyer
            char segment[RCVSIZE];

            int size = 1;

            FD_ZERO(&readfs);
            FD_SET(client_socket, &readfs);
            int s, i = 0;

            // Lecture du fichier et envoie des trames
            // Tant que la taille des elements lu n'est pas nule (fin de fichier)
            while ((size = fread(fileBuffer, 1, RCVSIZE - 6, file)) > 0)
            {
                // Incrementation du numero de segment               
                number_segment++;

                // Ajout du numero de segment au debut de la trame
                sprintf(segment, "%06d", number_segment);

                // Segment d'aquitement
                char ack_segment[9] = "ACK";
                strcat(ack_segment, segment);

                // Ajout du bloc de fichier dans la suite du segment
                memcpy(segment+6, fileBuffer, size);

                // Tant qu'on a pas recu l'aquitement de la trame on la renvoi
                while (strcmp(msgBufferClient, ack_segment) != 0)
                {
                    // Envoie de la trame
                    sendto(client_socket, segment, size + 6, 0, (struct sockaddr*)&client_adress, alen);

                    printf("Segment %d envoye.\n", number_segment);

                    FD_ZERO(&readfs);
                    FD_SET(client_socket, &readfs);

                    s = select(client_socket+1, &readfs, NULL, NULL, &timeout);

                    if (s > 0) {
                        // Attente de l'aquitement de la trame
                        recvfrom(client_socket, msgBufferClient, RCVSIZE, 0, (struct sockaddr *)&client_adress, &alen);
                        printf("Aquitement %s recu.\n", msgBufferClient);
                    }
                    else
                    {
                        printf("Aquitement du semgment %d non recu.\n", number_segment);
                    }

                    timeout.tv_sec = 0;
                    timeout.tv_usec = TIMEOUT;
                }
            }

            // Le transfert de fichier est termine

            char endMsg[3] = "FIN";
            sendto(client_socket, endMsg, 3, 0,(struct sockaddr*)&client_adress, sizeof(client_adress));

            printf("Fin du transfert");

            fclose(file);
            return 0;
        }
    }
}
