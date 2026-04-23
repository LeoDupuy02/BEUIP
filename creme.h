#ifndef CREME_H
#define CREME_H

#include <pthread.h>
#include <arpa/inet.h>

#define LBUF 512
#define PORT 9999

// Pour passer les params au thread
typedef struct {
    char pseudo[LBUF];
    pthread_mutex_t *mutex;
} serv_params_t;

// Variables globales partagées dans le cadre du multi threading
extern char network[255][525];
extern int network_index;
extern pthread_mutex_t mutex_network;
extern int sid_global;
extern char mon_pseudo[LBUF];

// Stockage des messages
extern int n_mess;
extern char mess_stock[20][2][LBUF];

// Prototypes
void *serveur_udp(void *p);
void commande(char octet1, char *message, char *pseudo_dest);
void add_client(char* ip, char* buf);
int checkexists(char* ip, char* pseudo);
int getpseudo(char* ip, char* pseudo_out);
void send_initial_broadcast(int sid, char* pseudo_in);
int checkvalid(char* buf);
void memorize(char *name, char *m);

#endif