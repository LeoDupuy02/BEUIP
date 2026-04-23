#include "creme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

char network[255][525];
int network_index = 0;
pthread_mutex_t mutex_network = PTHREAD_MUTEX_INITIALIZER;

int sid_global = -1;
char mon_pseudo[LBUF];
// Adresse de broadcast de mon réseau
char broadcast_addr[] = "192.168.1.255"; 

int n_mess = 0;
char mess_stock[20][2][LBUF];

int checkvalid(char* buf) {
    return strncmp(buf + 1, "BEUIP", 5);
}

// Fonctions utilitaires (pas de blocage du mutex)

int checkexists(char* ip, char* pseudo) {
    char formatted_ip[16];
    snprintf(formatted_ip, 16, "%-15s", ip);
    for(int i = 0; i < network_index; i++) {
        if (strncmp(network[i], formatted_ip, 15) == 0 && strcmp(network[i] + 15, pseudo) == 0)
            return 1;
    }
    return 0;
}

void add_client(char* ip, char* buf) {
    if (network_index < 255) {
        memset(network[network_index], 0, 525);
        snprintf(network[network_index], 16, "%-15s", ip);
        strncpy(network[network_index] + 15, buf + 6, 510);
        network_index++;
    }
}

int getpseudo(char* ip, char* pseudo_out) {
    char formatted_ip[16]; 
    snprintf(formatted_ip, 16, "%-15s", ip);
    for (int i = 0; i < network_index; i++) {
        if (strncmp(network[i], formatted_ip, 15) == 0) {
            strncpy(pseudo_out, network[i] + 15, 511);
            return 1;
        }
    }
    return 0;
}

void memorize(char *name, char *m) {
    strncpy(mess_stock[n_mess][0], name, 512);
    strncpy(mess_stock[n_mess][1], m, 512);
    n_mess = (n_mess + 1) % 20;
}

void send_initial_broadcast(int sid, char* pseudo_in) {
    struct sockaddr_in baddr;
    char bmsg[LBUF];
    baddr.sin_family = AF_INET;
    baddr.sin_port = htons(PORT);
    baddr.sin_addr.s_addr = inet_addr(broadcast_addr);
    bmsg[0] = '1';
    memcpy(bmsg + 1, "BEUIP", 5);
    strncpy(bmsg + 6, pseudo_in, LBUF - 7);
    sendto(sid, bmsg, strlen(pseudo_in) + 7, 0, (struct sockaddr *)&baddr, sizeof(baddr));
}

// Thread du serveur UDP
void *serveur_udp(void *p) {
    serv_params_t *params = (serv_params_t *)p;
    struct sockaddr_in SockConf, Sock;
    socklen_t ls = sizeof(Sock);
    char buf[LBUF];
    
    strncpy(mon_pseudo, params->pseudo, LBUF);

    if ((sid_global = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        pthread_exit(NULL);
    }

    memset(&SockConf, 0, sizeof(SockConf));
    SockConf.sin_family = AF_INET;
    SockConf.sin_port = htons(PORT);
    SockConf.sin_addr.s_addr = INADDR_ANY;

    if (bind(sid_global, (struct sockaddr *)&SockConf, sizeof(SockConf)) < 0) {
        perror("bind");
        pthread_exit(NULL);
    }

    int bc = 1;
    setsockopt(sid_global, SOL_SOCKET, SO_BROADCAST, &bc, sizeof(bc));

    send_initial_broadcast(sid_global, mon_pseudo);

    while (1) {
        int n = recvfrom(sid_global, buf, LBUF, 0, (struct sockaddr *)&Sock, &ls);
        if (n < 0) continue;
        buf[n] = '\0';

        char* sender_ip = inet_ntoa(Sock.sin_addr);
        if (checkvalid(buf) != 0) continue;

        pthread_mutex_lock(params->mutex);

        switch (buf[0]) {
            case '0': // Départ
            {
                memorize(buf + 6, "Départ du réseau");
                char formatted_ip[16];
                snprintf(formatted_ip, 16, "%-15s", sender_ip);
                for (int i = 0; i < network_index; i++) {
                    if (strncmp(network[i], formatted_ip, 15) == 0) {
                        memcpy(network[i], network[network_index - 1], 525);
                        network_index--;
                        break;
                    }
                }
                break;
            }
            case '1': // Hello
                if (!checkexists(sender_ip, buf + 6)) add_client(sender_ip, buf);
                // Ack
                buf[0] = '2';
                strncpy(buf + 6, mon_pseudo, LBUF - 7);
                sendto(sid_global, buf, strlen(mon_pseudo) + 7, 0, (struct sockaddr *)&Sock, ls);
                break;
            case '2': // Ack
                if (!checkexists(sender_ip, buf + 6)) add_client(sender_ip, buf);
                break;
            case '9': // Message entrant
            {
                char s_pseudo[512];
                if (getpseudo(sender_ip, s_pseudo)){
                    memorize(s_pseudo, buf + 6);
                } else {
                    memorize(sender_ip, buf + 6);    
                }
                printf("\n[Nouveau message reçu de %s, tapez 'beuip mess log' pour lire]\n", buf+6);
                break;
            }
        }
        pthread_mutex_unlock(params->mutex);
    }
    return NULL;
}

// Fonction remplaçant les commandes locales (sécurisée par Mutex)
void commande(char octet1, char *message, char *pseudo_dest) {
    if (sid_global == -1 && octet1 != '3' && octet1 != '6') return; // Serveur non lancé

    pthread_mutex_lock(&mutex_network);

    switch(octet1) {
        case '3': // Afficher Liste
            if(network_index == 0) {
                printf("Réseau vide !\n");
            } else {
                for(int i = 0; i < network_index; i++) printf("%s\n", network[i]);
            }
            break;

        case '6': // Afficher Logs
            if(n_mess == 0) {
                printf("Pas de message en attente\n");
            } else {
                for(int i = 0; i < n_mess; i++) {
                    printf("%s : %s\n", mess_stock[i][0], mess_stock[i][1]);
                    memset(mess_stock[i][0], 0, LBUF);
                    memset(mess_stock[i][1], 0, LBUF);
                }
            }
            n_mess = 0;
            break;

        case '4': // Envoyer MP
        {
            int found = 0;
            for (int i = 0; i < network_index; i++) {
                char entry_ip[16], entry_pseudo[512];
                sscanf(network[i], "%s %s", entry_ip, entry_pseudo);
                
                if (strcmp(entry_pseudo, pseudo_dest) == 0) {
                    struct sockaddr_in target_addr;
                    char msg_out[LBUF];
                    
                    memset(msg_out, 0, LBUF);
                    msg_out[0] = '9';
                    memcpy(msg_out + 1, "BEUIP", 5);
                    strncpy(msg_out + 6, message, LBUF - 7);
                    
                    target_addr.sin_family = AF_INET;
                    target_addr.sin_port = htons(PORT);
                    target_addr.sin_addr.s_addr = inet_addr(entry_ip);
                    
                    sendto(sid_global, msg_out, strlen(message) + 7, 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
                    found = 1;
                    printf("Message envoyé à %s !\n", pseudo_dest);
                    break;
                }
            }
            if (!found) printf("Utilisateur %s non trouvé.\n", pseudo_dest);
            break;
        }

        case '5': // Envoyer à tout le monde
        {
            char msg_out[LBUF];
            memset(msg_out, 0, LBUF);
            msg_out[0] = '9';
            memcpy(msg_out + 1, "BEUIP", 5);
            strncpy(msg_out + 6, message, LBUF - 7);
            
            for (int i = 0; i < network_index; i++) {
                struct sockaddr_in target;
                char target_ip[16];
                sscanf(network[i], "%s", target_ip);
                
                target.sin_family = AF_INET;
                target.sin_port = htons(PORT);
                target.sin_addr.s_addr = inet_addr(target_ip);
                sendto(sid_global, msg_out, strlen(message) + 7, 0, (struct sockaddr *)&target, sizeof(target));
            }
            printf("Message broadcasté à tout le monde !\n");
            break;
        }

        case '0': // Notifier départ
        {
            char msg_out[LBUF];
            memset(msg_out, 0, LBUF);
            msg_out[0] = '0';
            memcpy(msg_out + 1, "BEUIP", 5);
            strncpy(msg_out + 6, mon_pseudo, LBUF - 7);
            
            for (int i = 0; i < network_index; i++) {
                struct sockaddr_in target;
                char target_ip[16];
                sscanf(network[i], "%s", target_ip);
                
                target.sin_family = AF_INET;
                target.sin_port = htons(PORT);
                target.sin_addr.s_addr = inet_addr(target_ip);
                // Envoi sans attente (sans AR) comme demandé par le TP
                sendto(sid_global, msg_out, strlen(mon_pseudo) + 7, 0, (struct sockaddr *)&target, sizeof(target));
            }
            break;
        }
    }
    pthread_mutex_unlock(&mutex_network);
}