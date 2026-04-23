#ifndef CREME_H
#define CREME_H

#include <pthread.h>
#include <arpa/inet.h>

#define LBUF 512
#define PORT 9999
#define LPSEUDO 23

// Structure pour la liste chaînée
struct elt {
    char nom[LPSEUDO + 1];
    char adip[16];
    struct elt *next;
};

// Structure pour passer les paramètres au thread
typedef struct {
    char pseudo[LBUF];
    pthread_mutex_t *mutex;
} serv_params_t;

// Variables globales shared
// Début liste réseau
extern struct elt *tete_reseau; 
extern pthread_mutex_t mutex_network;
extern int sid_global;
extern char mon_pseudo[LBUF];

// Stockage des messages
extern int n_mess;
extern char mess_stock[20][2][LBUF];

void *serveur_udp(void *p);
void commande(char octet1, char *message, char *pseudo_dest);

// Liste chainée
void ajouteElt(char *pseudo, char *adip);
void supprimeElt(char *adip);
void listeElts(void);
void vider_liste(void);

// Utilitaires réseau
int checkexists(char *ip, char *pseudo);
int getpseudo(char *ip, char *pseudo_out);
void envoyer_broadcasts(int sid, char octet, char *payload);
int checkvalid(char *buf);
void memorize(char *name, char *m);

#endif