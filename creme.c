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

void ajouteElt(char *pseudo, char *adip) {
    struct elt *n = malloc(sizeof(struct elt));
    if (!n) return;
    strncpy(n->nom, pseudo, LPSEUDO); n->nom[LPSEUDO] = '\0';
    strncpy(n->adip, adip, 15); n->adip[15] = '\0';
    if (!tete_reseau || strcmp(n->nom, tete_reseau->nom) < 0) {
        n->next = tete_reseau; tete_reseau = n; return;
    }
    struct elt *c = tete_reseau;
    while (c->next && strcmp(n->nom, c->next->nom) > 0) c = c->next;
    if (strcmp(n->nom, c->nom) == 0 || (c->next && strcmp(n->nom, c->next->nom) == 0)) {
        free(n); return;
    }
    n->next = c->next; c->next = n;
}

void supprimeElt(char *adip) {
    struct elt *c = tete_reseau, *prec = NULL;
    while (c) {
        if (strcmp(c->adip, adip) == 0) {
            if (!prec) tete_reseau = c->next;
            else prec->next = c->next;
            free(c);
            return;
        }
        prec = c; c = c->next;
    }
}

void vider_liste(void) {
    struct elt *c = tete_reseau, *suiv;
    while (c) {
        suiv = c->next;
        free(c);
        c = suiv;
    }
    tete_reseau = NULL;
}

int checkexists(char *ip, char *pseudo) {
    struct elt *c = tete_reseau;
    while (c) {
        if (strcmp(ip, c->adip) == 0 && strcmp(pseudo, c->nom) == 0) return 1;
        c = c->next;
    }
    return 0;
}

void envoyer_broadcasts(int sid, char octet, char *payload) {
    struct ifaddrs *ifaddr, *ifa;
    char bmsg[LBUF];
    memset(bmsg, 0, LBUF); bmsg[0] = octet; memcpy(bmsg + 1, "BEUIP", 5);
    if (payload) strncpy(bmsg + 6, payload, LBUF - 7);
    int len = payload ? strlen(payload) + 7 : 6;

    if (getifaddrs(&ifaddr) == -1) return;
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, "lo") == 0) continue;
        if (ifa->ifa_broadaddr) {
            char host[NI_MAXHOST];
            if (getnameinfo(ifa->ifa_broadaddr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0) {
                struct sockaddr_in baddr;
                baddr.sin_family = AF_INET; baddr.sin_port = htons(PORT); baddr.sin_addr.s_addr = inet_addr(host);
                sendto(sid, bmsg, len, 0, (struct sockaddr *)&baddr, sizeof(baddr));
            }
        }
    }
    freeifaddrs(ifaddr);
}

void cmd_list(void) {
    pthread_mutex_lock(&mutex_network);
    struct elt *c = tete_reseau;
    while (c) {
        printf("%s : %s\n", c->adip, c->nom); // Format exact consigne 4
        c = c->next;
    }
    pthread_mutex_unlock(&mutex_network);
}

void cmd_message_prive(char *pseudo_dest, char *msg) {
    pthread_mutex_lock(&mutex_network);
    struct elt *c = tete_reseau;
    while (c) {
        if (strcmp(pseudo_dest, c->nom) == 0) {
            struct sockaddr_in target;
            char bmsg[LBUF];
            memset(bmsg, 0, LBUF); bmsg[0] = '9'; memcpy(bmsg + 1, "BEUIP", 5);
            strncpy(bmsg + 6, msg, LBUF - 7);
            target.sin_family = AF_INET; target.sin_port = htons(PORT); target.sin_addr.s_addr = inet_addr(c->adip);
            sendto(sid_global, bmsg, strlen(msg) + 7, 0, (struct sockaddr *)&target, sizeof(target));
            break;
        }
        c = c->next;
    }
    pthread_mutex_unlock(&mutex_network);
}

void cmd_message_all(char *msg) {
    pthread_mutex_lock(&mutex_network);
    struct elt *c = tete_reseau;
    while (c) {
        struct sockaddr_in target;
        char bmsg[LBUF];
        memset(bmsg, 0, LBUF); bmsg[0] = '9'; memcpy(bmsg + 1, "BEUIP", 5);
        strncpy(bmsg + 6, msg, LBUF - 7);
        target.sin_family = AF_INET; target.sin_port = htons(PORT); target.sin_addr.s_addr = inet_addr(c->adip);
        sendto(sid_global, bmsg, strlen(msg) + 7, 0, (struct sockaddr *)&target, sizeof(target));
        c = c->next;
    }
    pthread_mutex_unlock(&mutex_network);
}

void cmd_stop(void) {
    pthread_mutex_lock(&mutex_network);
    envoyer_broadcasts(sid_global, '0', mon_pseudo);
    vider_liste();
    pthread_mutex_unlock(&mutex_network);
}

void handle_incoming(char *buf, char *ip, socklen_t ls, struct sockaddr_in *Sock) {
    pthread_mutex_lock(&mutex_network);
    if (buf[0] == '0') supprimeElt(ip);
    else if (buf[0] == '1') {
        if (!checkexists(ip, buf + 6)) ajouteElt(buf + 6, ip);
        buf[0] = '2'; strncpy(buf + 6, mon_pseudo, LBUF - 7);
        sendto(sid_global, buf, strlen(mon_pseudo) + 7, 0, (struct sockaddr *)Sock, ls);
    }
    else if (buf[0] == '2') {
        if (!checkexists(ip, buf + 6)) ajouteElt(buf + 6, ip);
    }
    else if (buf[0] == '9') printf("\n[Message de %s] : %s\n", ip, buf+6);
    pthread_mutex_unlock(&mutex_network);
}

void *serveur_udp(void *p) {
    serv_params_t *params = (serv_params_t *)p;
    struct sockaddr_in SockConf, Sock;
    socklen_t ls = sizeof(Sock);
    char buf[LBUF];
    strncpy(mon_pseudo, params->pseudo, LBUF);
    if ((sid_global = socket(AF_INET, SOCK_DGRAM, 0)) < 0) pthread_exit(NULL);

    memset(&SockConf, 0, sizeof(SockConf));
    SockConf.sin_family = AF_INET; SockConf.sin_port = htons(PORT); SockConf.sin_addr.s_addr = INADDR_ANY;
    if (bind(sid_global, (struct sockaddr *)&SockConf, sizeof(SockConf)) < 0) pthread_exit(NULL);
    int bc = 1; setsockopt(sid_global, SOL_SOCKET, SO_BROADCAST, &bc, sizeof(bc));
    envoyer_broadcasts(sid_global, '1', mon_pseudo);

    while (1) {
        int n = recvfrom(sid_global, buf, LBUF, 0, (struct sockaddr *)&Sock, &ls);
        if (n < 0 || strncmp(buf + 1, "BEUIP", 5) != 0) continue;
        buf[n] = '\0';
        handle_incoming(buf, inet_ntoa(Sock.sin_addr), ls, &Sock);
    }
    return NULL;
}