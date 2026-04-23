#include "gescom.h"
#include <pthread.h>

int cmds_size = 0;
Command cmds[NBMAXC];

extern char* input;
extern char* header;
extern char* HIST_FILE;
extern void freeMots(void);

// Variables pour le thread serveur
pthread_t tid_serveur;
int serveur_actif = 0;
serv_params_t thread_params;

int exec_beuip(int N, char *P[]) {
    if (N < 2) {
        printf("Usage: beuip <start pseudo | stop | list | mess log | mess pseudo msg | mess all msg>\n");
        return 1;
    }

    if (strcmp(P[1], "start") == 0 && N == 3) {
        if (serveur_actif) {
            printf("Le serveur est déjà lancé !\n");
            return 1;
        }
        strncpy(thread_params.pseudo, P[2], LBUF);
        thread_params.mutex = &mutex_network;

        if (pthread_create(&tid_serveur, NULL, serveur_udp, &thread_params) == 0) {
            serveur_actif = 1;
            printf("Serveur BEUIP lancé (Pseudo: %s)\n", P[2]);
        } else {
            perror("Erreur pthread_create");
        }
    } 
    else if (strcmp(P[1], "stop") == 0) {
        if (!serveur_actif) {
            printf("Le serveur n'est pas actif.\n");
            return 1;
        }
        // Envoi du message '0' à tout le réseau (sans tuer le processus)
        commande('0', NULL, NULL);
        
        // Annulation et attente de fin du thread
        pthread_cancel(tid_serveur);
        pthread_join(tid_serveur, NULL);
        
        if (sid_global != -1) close(sid_global);
        sid_global = -1;
        serveur_actif = 0;
        printf("Serveur BEUIP arrêté proprement.\n");
    }
    else if (strcmp(P[1], "list") == 0) {
        commande('3', NULL, NULL);
    }
    else if (strcmp(P[1], "mess") == 0 && N >= 3) {
        if (strcmp(P[2], "log") == 0) {
            commande('6', NULL, NULL);
        } else if (N >= 4) {
            // Reconstitution du message depuis les arguments
            char msg[LBUF] = "";
            for(int i = 3; i < N; i++) {
                strcat(msg, P[i]);
                if(i < N - 1) strcat(msg, " ");
            }
            if (strcmp(P[2], "all") == 0) {
                commande('5', msg, NULL);
            } else {
                commande('4', msg, P[2]);
            }
        }
    }
    return 0;
}

int Sortie(int N, char *P[]) { 
    if (serveur_actif) {
        char *Pbis[] = {"beuip", "stop"};
        exec_beuip(2, Pbis);
    }
    write_history(HIST_FILE);
    free(input);
    free(header);
    freeMots();
    exit(0);
}

int Changedir(int N, char *P[]){
    int i = 0;
    if(N > 1) i = chdir(P[1]);
    else i = chdir(getenv("HOME"));
    return i;
}

int printWorkingDir(int N, char *P[]){
    char buf[1024];
    getcwd(buf, 1024);
    printf("%s\n", buf);
    return 0;
}

int Vers(int N, char *P[]){
    printf("Version : %s\n", V);
    return 0;
}

void ajouteCom(char *nom, int (*fptr)(int argc, char *argv[])){
    if(cmds_size >= NBMAXC) exit(1);
    Command mac;
    mac.nom = nom;
    mac.fptr = fptr;
    cmds[cmds_size] = mac;
    cmds_size += 1;
}

void majComInt(void){
    ajouteCom("exit", Sortie);
    ajouteCom("cd", Changedir);
    ajouteCom("pwd", printWorkingDir);
    ajouteCom("vers", Vers);
    ajouteCom("beuip", exec_beuip); // Ajout de la commande interne beuip
}

void listeComInt(void){
    for(int i = 0; i < cmds_size; i++) printf("%s ", cmds[i].nom);
    printf("\n");
}

int execComInt(int N, char **P){
    for(int i = 0; i < cmds_size; i++){
        if(strcmp(P[0], cmds[i].nom) == 0){
            cmds[i].fptr(N, P);
            return 1; // Retourne 1 pour indiquer que c'était une commande interne
        }
    }
    return 0;
}

int execComExt(char **P){
    int pid;
    if ((pid = fork()) == -1) {
        perror("fork"); 
        return 1;
    }
    if (pid == 0) { 
        execvp(P[0], P);
        perror("execvp");
        exit(1);
    }
    else { 
        pid_t wstatus;
        if (waitpid(pid, &wstatus, WUNTRACED | WCONTINUED) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }
        return 0;
    }
}