#ifndef GESCOM
#define GESCOM

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "version.h"
#include "creme.h"

#define NBMAXC 10

typedef struct _Command{
    char* nom;
    int (*fptr)(int argc, char *argv[]);
} Command;

extern int cmds_size;
extern Command cmds[NBMAXC];

int Sortie(int N, char *P[]);
int Changedir(int N, char *P[]);
int printWorkingDir(int N, char *P[]);
int Vers(int N, char *P[]);
int exec_beuip(int N, char *P[]);
void ajouteCom(char *nom, int (*fptr)(int argc, char *argv[]));
void majComInt(void);
void listeComInt(void);
int execComInt(int N, char **P);
int execComExt(char **P);

#endif