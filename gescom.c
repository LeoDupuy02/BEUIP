#include "gescom.h"
#include <pthread.h>

int cmds_size = 0;
Command cmds[NBMAXC];

extern char* input;
extern char* header;
extern char* HIST_FILE;
extern void freeMots(void);

pthread_t tid_serveur;
int serveur_actif = 0;
serv_params_t thread_params;

void handle_beuip_message(int N, char **P) {
    if (N < 4) return;
    char msg[LBUF] = "";
    for(int i = 3; i < N; i++) {
        strcat(msg, P[i]);
        if(i < N - 1) strcat(msg, " ");
    }
    if (strcmp(P[2], "all") == 0) cmd_message_all(msg);
    else cmd_message_prive(P[2], msg);
}

int exec_beuip(int N, char *P[]) {
    if (N < 2) return 1;

    if (strcmp(P[1], "start") == 0 && N == 3) {
        if (serveur_actif) return 1;
        strncpy(thread_params.pseudo, P[2], LBUF);
        thread_params.mutex = &mutex_network;
        if (pthread_create(&tid_serveur, NULL, serveur_udp, &thread_params) == 0) {
            serveur_actif = 1;
        }
    } 
    else if (strcmp(P[1], "stop") == 0) {
        if (!serveur_actif) return 1;
        cmd_stop();
        pthread_cancel(tid_serveur);
        pthread_join(tid_serveur, NULL);
        if (sid_global != -1) close(sid_global);
        sid_global = -1; serveur_actif = 0;
    }
    else if (strcmp(P[1], "list") == 0) cmd_list();
    else if (strcmp(P[1], "message") == 0) handle_beuip_message(N, P);
    
    return 0;
}

int Sortie(int N, char *P[]) { 
    (void)N; (void)P; // Ignore unused warnings
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
    if(N > 1) return chdir(P[1]);
    return chdir(getenv("HOME"));
}

int printWorkingDir(int N, char *P[]){
    (void)N; (void)P;
    char buf[1024];
    if (getcwd(buf, 1024)) printf("%s\n", buf);
    return 0;
}

int Vers(int N, char *P[]){
    (void)N; (void)P;
    printf("Version : %s\n", V);
    return 0;
}

void ajouteCom(char *nom, int (*fptr)(int argc, char *argv[])){
    if(cmds_size >= NBMAXC) exit(1);
    cmds[cmds_size].nom = nom;
    cmds[cmds_size].fptr = fptr;
    cmds_size += 1;
}

void majComInt(void){
    ajouteCom("exit", Sortie); ajouteCom("cd", Changedir);
    ajouteCom("pwd", printWorkingDir); ajouteCom("vers", Vers);
    ajouteCom("beuip", exec_beuip);
}

int execComInt(int N, char **P){
    for(int i = 0; i < cmds_size; i++){
        if(strcmp(P[0], cmds[i].nom) == 0){
            cmds[i].fptr(N, P);
            return 1;
        }
    }
    return 0;
}

int execComExt(char **P){
    int pid;
    if ((pid = fork()) == -1) return 1;
    if (pid == 0) { 
        execvp(P[0], P); exit(1);
    } else { 
        pid_t wstatus;
        waitpid(pid, &wstatus, WUNTRACED | WCONTINUED);
        return 0;
    }
}