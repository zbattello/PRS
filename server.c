#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>

// Taille enoctet des segments
#define RCVSIZE 1500

// Nombre de segments par fenetre
#define WINSIZE 300

// Delai d'attente en micro-secondes
// avant renvoi de la trame
#define TIMEOUT 2000


int main (int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Utilisation : ./server <port>\n");
        return -1;
    }

    // Adresse publique
    struct sockaddr_in public_adress;
    socklen_t alen = sizeof(public_adress);

    // Adresse client
    struct sockaddr_in client_adress;

    // Buffer de reception publique
    char msgBufferPublic[RCVSIZE];

    // Buffer de lecture du fichier
    char fileBuffer[RCVSIZE - 6];

    // timeout
    struct timeval timeout;

    //fdset pour le select
    fd_set readfs;

    // Obtention du port publique
    int public_port = atoi(argv[1]);

    // Port unique du client
    int client_port = 8000; // Sera incremente a chaque client
    char str_client_port[4];

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

        char syn_ack[11] = "SYN-ACK";

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

            struct timeval tv;
            gettimeofday(&tv, NULL);
            unsigned long begin = 1000000 * tv.tv_sec + tv.tv_usec;

            // Buffer de reception du client
            char msgBufferClient[RCVSIZE];

            // Nom du fichier a envoyer
            char file_name[RCVSIZE];

            // Attente de reception du nom du fichier demande
            recvfrom(client_socket, msgBufferClient, RCVSIZE, 0, (struct sockaddr *)&client_adress, &alen);

            // Enregistrement du nom du fichier dans file_name
            memcpy(file_name, msgBufferClient, RCVSIZE);

            printf("~~~~~~~~~~~~~~~~~~\n");

            printf("Demande recu du fichier '%s'.\n", file_name);

            // Obtention de la taille du fichier
            struct stat sb;
            stat(file_name, &sb);
            unsigned int file_size = sb.st_size;

            // Overture du fichier a envoyer
            FILE* file = NULL;
            file = fopen(file_name, "rb"); //Ouverture binaire

            if (file == NULL)
            {
                printf("Erreur ouverture du fichier.\n");
                return -1;
            }

            printf("Envoie du fichier...\n");

            // Le numero de la trame qui sera incremente
            int number_segment = 0;

            // Le numero de la fenetre qui sera incremente
            int number_window = 0;

            // La fenetre a envoyer de segments
            char window[WINSIZE][RCVSIZE];

            // Nombre de segment envoye
            int segment_send = 0;

            // Nombre de segment aquite
            int segment_ack = 0;

            // Numero du segment perdu
            int lost_segment = 0;

            char str_ack[6];
            int number_ack;

            int size = 1;
            int win_size;

            // Liste des numeros de segment
            int tab_segment[WINSIZE];
            int tab_size[WINSIZE];

            int k;

            // Lecture et transfert du fichier

            // Tant qu'il reste des octets a lire dans le fichier
            while (size > 0)
            {
                number_window++;
                // Generation de la liste des trames a envoyer (la fenetre)
                for (k = 0; k < WINSIZE; k++)
                {
                    // Incrementation du numero de segment               
                    number_segment++;

                    // Lecture de (RCVSIZE-6) octets du fichier
                    size = fread(fileBuffer, 1, RCVSIZE - 6, file);

                    tab_segment[k] = number_segment;
                    tab_size[k] = size;

                    // Si tout les octets ont ete lu
                    if (size == 0)
                    {
                        // On sort de la boucle
                        break;
                    }

                    // Ajout du numero de segment au debut de la trame
                    sprintf(window[k], "%06d", number_segment);

                    // Ajout du bloc de fichier dans la suite du segment
                    memcpy(window[k] + 6, fileBuffer, size);
                }

                if (tab_size[0] == 0) {
                    // La nouvelle fenetre est vide de base. Plus rien a envoyer
                    break;
                }

                // La taille de la fenetre
                int win_size = k;

                // Tant que le plus grand numero d'ACK recu ne
                // corespond pas au dernier segment de la fenetre
                do {
                    // Envoie des trames de la fenetre
                    // a partir du dernier ACK recu
                    for (int i = segment_ack % WINSIZE; i < win_size; i++)
                    {
                        // Envoie de la trame
                        sendto(client_socket, window[i], tab_size[i] + 6, 0, (struct sockaddr*)&client_adress, alen);

                        // MaJ du nombre de segments envoyes
                        segment_send = (tab_segment[i] > segment_send) ? tab_segment[i] : segment_send;
                    }

                    // Reception des ACK
                    do {
                        FD_ZERO(&readfs);
                        FD_SET(client_socket, &readfs);

                        // Reinitialisation du delai
                        timeout.tv_sec = 0;
                        timeout.tv_usec = TIMEOUT;

                        int s = select(client_socket+1, &readfs, NULL, NULL, &timeout);

                        // Si on a recu un ACK
                        if (s > 0)
                        {
                            // Reception de l'aquitement de la trame
                            recvfrom(client_socket, msgBufferClient, RCVSIZE, 0, (struct sockaddr *)&client_adress, &alen);

                            // Extraction du numero d'ACK
                            for (int j = 0; j < 6; j++)
                            {
                                str_ack[j] = msgBufferClient[j+3];
                            }
                            number_ack = atoi(str_ack);

                            // Si le numero d'ACK est le plus grand
                            if (number_ack > segment_ack)
                            {
                                // Alors mise-a-jour du nombre d'ACK recu
                                segment_ack = number_ack;
                            }
                        }

                        // Si on a rien recu avant le timeout
                        else
                        {
                            break;
                        }

                        // Tant que le nombre d'aquitement recu est inferieur au nombre de segment envoye
                    } while (segment_ack < segment_send);

                } while (segment_ack < segment_send);
            }

            // Le transfert de fichier est termine

            char endMsg[3] = "FIN";
            sendto(client_socket, endMsg, 3, 0,(struct sockaddr*)&client_adress, sizeof(client_adress));

            printf("Fin du transfert de '%s'.\n", file_name);

            fclose(file);

            gettimeofday(&tv, NULL);
            unsigned long end = 1000000 * tv.tv_sec + tv.tv_usec;

            float exec_time = (float)(end - begin) / 1000000;
            float debit = (float)(file_size) * 0.000001 / exec_time;

            printf("Taille du fichier : %f mega-octets\n", (float)(file_size) * 0.000001);
            printf("Temps d'execution : %f secondes\n", exec_time);
            printf("Debit : %f mo/s\n", debit);

            printf("~~~~~~~~~~~~~~~~~~\n");

            return 0;
        }
    }
}
