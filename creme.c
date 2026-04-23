#include "creme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <netdb.h>

struct elt *tete_reseau = NULL;
pthread_mutex_t mutex_network = PTHREAD_MUTEX_INITIALIZER;

int sid_global = -1;
char mon_pseudo[LBUF];
int n_mess = 0;
char mess_stock[20][2][LBUF];

int checkvalid(char* buf) {
    return strncmp(buf + 1, "BEUIP", 5);
}

/* ==========================================
   GESTION DE LA LISTE CHAÎNÉE TRIÉE
   ========================================== */

void ajouteElt(char *pseudo, char *adip) {
    // 1. Création du nouvel élément
    struct elt *nouveau = malloc(sizeof(struct elt));
    if (!nouveau) return;
    
    strncpy(nouveau->nom, pseudo, LPSEUDO);
    nouveau->nom[LPSEUDO] = '\0';
    strncpy(nouveau->adip, adip, 15);
    nouveau->adip[15] = '\0';
    nouveau->next = NULL;

    // 2. Insertion en tête (si liste vide ou nouveau pseudo avant le premier)
    if (tete_reseau == NULL || strcmp(nouveau->nom, tete_reseau->nom) < 0) {
        nouveau->next = tete_reseau;
        tete_reseau = nouveau;
        return;
    }

    // 3. Recherche de la bonne position pour garder le tri alphabétique
    struct elt *courant = tete_reseau;
    while (courant->next != NULL && strcmp(nouveau->nom, courant->next->nom) > 0) {
        courant = courant->next;
    }

    // Éviter les doublons stricts (même pseudo)
    if (strcmp(nouveau->nom, courant->nom) == 0 || (courant->next && strcmp(nouveau->nom, courant->next->nom) == 0)) {
        free(nouveau);
        return;
    }

    // 4. Insertion au milieu ou à la fin
    nouveau->next = courant->next;
    courant->next = nouveau;
}

void supprimeElt(char *adip) {
    struct elt *courant = tete_reseau;
    struct elt *precedent = NULL;

    while (courant != NULL) {
        if (strcmp(courant->adip, adip) == 0) {
            if (precedent == NULL) { // Si c'est le premier élément
                tete_reseau = courant->next;
            } else { // Si c'est au milieu ou à la fin
                precedent->next = courant->next;
            }
            free(courant);
            return;
        }
        precedent = courant;
        courant = courant->next;
    }
}

void listeElts(void) {
    struct elt *courant = tete_reseau;
    if (courant == NULL) {
        printf("Réseau vide !\n");
        return;
    }
    printf("--- Utilisateurs connectés ---\n");
    while (courant != NULL) {
        printf("%-15s %s\n", courant->adip, courant->nom);
        courant = courant->next;
    }
}

void vider_liste(void) {
    struct elt *courant = tete_reseau;
    struct elt *suivant;
    while (courant != NULL) {
        suivant = courant->next;
        free(courant);
        courant = suivant;
    }
    tete_reseau = NULL;
}

int checkexists(char *ip, char *pseudo) {
    struct elt *courant = tete_reseau;
    while (courant != NULL) {
        if (strcmp(ip, courant->adip) == 0 && strcmp(pseudo, courant->nom) == 0) return 1;
        courant = courant->next;
    }
    return 0;
}

int getpseudo(char *ip, char *pseudo_out) {
    struct elt *courant = tete_reseau;
    while (courant != NULL) {
        if (strcmp(ip, courant->adip) == 0) {
            strncpy(pseudo_out, courant->nom, LBUF - 1);
            return 1;
        }
        courant = courant->next;
    }
    return 0;
}

/* ==========================================
   RÉSEAU ET BROADCAST MULTI-INTERFACES
   ========================================== */

void envoyer_broadcasts(int sid, char octet, char *payload) {
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];
    char bmsg[LBUF];
    
    memset(bmsg, 0, LBUF);
    bmsg[0] = octet;
    memcpy(bmsg + 1, "BEUIP", 5);
    if (payload) strncpy(bmsg + 6, payload, LBUF - 7);
    int msg_len = payload ? (strlen(payload) + 7) : 6;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, "lo") == 0) continue; // Ignorer localhost

        if (ifa->ifa_broadaddr != NULL) {
            int s = getnameinfo(ifa->ifa_broadaddr, sizeof(struct sockaddr_in),
                                host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s == 0) {
                struct sockaddr_in baddr;
                baddr.sin_family = AF_INET;
                baddr.sin_port = htons(PORT);
                baddr.sin_addr.s_addr = inet_addr(host);
                sendto(sid, bmsg, msg_len, 0, (struct sockaddr *)&baddr, sizeof(baddr));
            }
        }
    }
    freeifaddrs(ifaddr); // Nettoyage de la liste système
}

void memorize(char *name, char *m) {
    strncpy(mess_stock[n_mess][0], name, 512);
    strncpy(mess_stock[n_mess][1], m, 512);
    n_mess = (n_mess + 1) % 20;
}

/* ==========================================
   SERVEUR UDP THREADÉ
   ========================================== */

void *serveur_udp(void *p) {
    serv_params_t *params = (serv_params_t *)p;
    struct sockaddr_in SockConf, Sock;
    socklen_t ls = sizeof(Sock);
    char buf[LBUF];
    
    strncpy(mon_pseudo, params->pseudo, LBUF);

    if ((sid_global = socket(AF_INET, SOCK_DGRAM, 0)) < 0) pthread_exit(NULL);

    memset(&SockConf, 0, sizeof(SockConf));
    SockConf.sin_family = AF_INET;
    SockConf.sin_port = htons(PORT);
    SockConf.sin_addr.s_addr = INADDR_ANY;

    if (bind(sid_global, (struct sockaddr *)&SockConf, sizeof(SockConf)) < 0) pthread_exit(NULL);

    int bc = 1;
    setsockopt(sid_global, SOL_SOCKET, SO_BROADCAST, &bc, sizeof(bc));

    // Utilisation de la nouvelle fonction pour le scan des interfaces réseau
    envoyer_broadcasts(sid_global, '1', mon_pseudo);

    while (1) {
        int n = recvfrom(sid_global, buf, LBUF, 0, (struct sockaddr *)&Sock, &ls);
        if (n < 0) continue;
        buf[n] = '\0';

        char* sender_ip = inet_ntoa(Sock.sin_addr);
        if (checkvalid(buf) != 0) continue;

        pthread_mutex_lock(params->mutex);

        switch (buf[0]) {
            case '0':
                memorize(buf + 6, "Départ du réseau");
                supprimeElt(sender_ip);
                break;
            case '1':
                if (!checkexists(sender_ip, buf + 6)) ajouteElt(buf + 6, sender_ip);
                buf[0] = '2';
                strncpy(buf + 6, mon_pseudo, LBUF - 7);
                sendto(sid_global, buf, strlen(mon_pseudo) + 7, 0, (struct sockaddr *)&Sock, ls);
                break;
            case '2':
                if (!checkexists(sender_ip, buf + 6)) ajouteElt(buf + 6, sender_ip);
                break;
            case '9': {
                char s_pseudo[LBUF];
                if (getpseudo(sender_ip, s_pseudo)) memorize(s_pseudo, buf + 6);
                else memorize(sender_ip, buf + 6);
                printf("\n[Nouveau message de %s (tapez 'beuip mess log')]\n", buf+6);
                break;
            }
        }
        pthread_mutex_unlock(params->mutex);
    }
    return NULL;
}

/* ==========================================
   INTERFACE DES COMMANDES LOCALES
   ========================================== */

void commande(char octet1, char *message, char *pseudo_dest) {
    if (sid_global == -1 && octet1 != '3' && octet1 != '6') return;

    pthread_mutex_lock(&mutex_network);

    switch(octet1) {
        case '3': // Afficher la liste
            listeElts();
            break;

        case '6': // Afficher les logs
            if(n_mess == 0) printf("Pas de message en attente\n");
            else {
                for(int i = 0; i < n_mess; i++) {
                    printf("%s : %s\n", mess_stock[i][0], mess_stock[i][1]);
                    memset(mess_stock[i][0], 0, LBUF);
                    memset(mess_stock[i][1], 0, LBUF);
                }
                n_mess = 0;
            }
            break;

        case '4': // Envoyer MP
        {
            struct elt *courant = tete_reseau;
            int found = 0;
            while (courant != NULL) {
                int cmp = strcmp(pseudo_dest, courant->nom);
                if (cmp == 0) { // Cible trouvée
                    struct sockaddr_in target_addr;
                    char msg_out[LBUF];
                    memset(msg_out, 0, LBUF);
                    msg_out[0] = '9';
                    memcpy(msg_out + 1, "BEUIP", 5);
                    strncpy(msg_out + 6, message, LBUF - 7);
                    
                    target_addr.sin_family = AF_INET;
                    target_addr.sin_port = htons(PORT);
                    target_addr.sin_addr.s_addr = inet_addr(courant->adip);
                    sendto(sid_global, msg_out, strlen(message) + 7, 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
                    found = 1;
                    printf("Message envoyé à %s !\n", pseudo_dest);
                    break;
                } else if (cmp < 0) {
                    break; // Sortie anticipée (liste triée)
                }
                courant = courant->next;
            }
            if (!found) printf("Utilisateur %s non trouvé.\n", pseudo_dest);
            break;
        }

        case '5': // Envoyer à tout le réseau (Parcours complet de la liste)
        {
            char msg_out[LBUF];
            memset(msg_out, 0, LBUF);
            msg_out[0] = '9';
            memcpy(msg_out + 1, "BEUIP", 5);
            strncpy(msg_out + 6, message, LBUF - 7);
            
            struct elt *courant = tete_reseau;
            while (courant != NULL) {
                struct sockaddr_in target;
                target.sin_family = AF_INET;
                target.sin_port = htons(PORT);
                target.sin_addr.s_addr = inet_addr(courant->adip);
                sendto(sid_global, msg_out, strlen(message) + 7, 0, (struct sockaddr *)&target, sizeof(target));
                courant = courant->next;
            }
            printf("Message envoyé à toute la liste !\n");
            break;
        }

        case '0': // Notifier du départ (Parcours de liste + destruction)
        {
            char msg_out[LBUF];
            memset(msg_out, 0, LBUF);
            msg_out[0] = '0';
            memcpy(msg_out + 1, "BEUIP", 5);
            strncpy(msg_out + 6, mon_pseudo, LBUF - 7);
            
            struct elt *courant = tete_reseau;
            while (courant != NULL) {
                struct sockaddr_in target;
                target.sin_family = AF_INET;
                target.sin_port = htons(PORT);
                target.sin_addr.s_addr = inet_addr(courant->adip);
                sendto(sid_global, msg_out, strlen(mon_pseudo) + 7, 0, (struct sockaddr *)&target, sizeof(target));
                courant = courant->next;
            }
            vider_liste(); // Libération de tous les noeuds de la liste
            break;
        }
    }
    pthread_mutex_unlock(&mutex_network);
}