#ifndef CREME_H
#define CREME_H

#include <pthread.h>
#include <arpa/inet.h>

#define LBUF 512
#define PORT 9999
#define LPSEUDO 23
#define BROADCAST_IP "192.168.88.255"

struct elt {
    char nom[LPSEUDO + 1];
    char adip[16];
    struct elt *next;
};

typedef struct {
    char pseudo[LBUF];
    pthread_mutex_t *mutex;
} serv_params_t;

extern struct elt *tete_reseau;
extern pthread_mutex_t mutex_network;
extern int sid_global;
extern char mon_pseudo[LBUF];

void *serveur_udp(void *p);
void cmd_list(void);
void cmd_message_prive(char *pseudo_dest, char *msg);
void cmd_message_all(char *msg);
void cmd_stop(void);
void vider_liste(void);

#endif