#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "gescom.h"

char* HIST_FILE = "hist_file";
int Mots_base_size = 64;

int Mots_size = 64;
static char ** Mots;
static int NMots = 0;
char* save;

char *header;
char *input;
char *command;

void freeMots(void){
    for(int i = 0; i < NMots; i++){
        free(Mots[i]);
        Mots[i] = NULL;
    }
    free(Mots);
    Mots = NULL;
}
 
char* head(void){
    char *env = "USER";
    char *name;
    char hostname[32];
    char type;
    
    memset(hostname, 0, sizeof(hostname)); // Sécurité Valgrind
    name = getenv(env);

    if(name != NULL){
        if(gethostname(hostname, 31) < 0){
            perror("gethostname");
            return NULL;
        }
        if(strcmp(name, "root") == 0) type = '#';
        else type = '$';
    }
    else {
        perror("getenv");
        return NULL;
    }
    
    int i = 0;
    int N = strlen(name) + strlen(hostname) + 4;
    char *header_alloc = (char *)malloc(sizeof(char) * N);
    
    for(i = 0; i < (int)strlen(name); i++){
        header_alloc[i] = name[i];
    }       
    header_alloc[i] = '@';
    for(i = strlen(name) + 1; i < (int)(strlen(name) + 1 + strlen(hostname)); i++){
        header_alloc[i] = hostname[i - strlen(name) - 1];
    }
    header_alloc[i] = type;
    header_alloc[i+1] = ' ';
    header_alloc[i+2] = '\0'; // CORRECTION VALGRIND CRITIQUE
        
    return header_alloc;  
}

int analyseCom(char *b){
    char* out;
    int cnt = 0;
    
    if (Mots == NULL) {
        Mots = malloc(sizeof(char *) * Mots_base_size);
        Mots_size = Mots_base_size;
    }
        
    while((out = strsep(&b, " \t\n")) != NULL){
        if(*out == '\0') continue;
        Mots[cnt] = strdup(out);
        cnt++;
        if(cnt >= Mots_size){
            Mots = realloc(Mots, (cnt + 1) * sizeof(char *));
            Mots_size = cnt + 1;
        }
    }
    
    Mots[cnt] = NULL;
    NMots = cnt;
    
    return cnt;
}

int main(void)
{
    signal(SIGINT, SIG_IGN);
    
    if((header = head()) == NULL){
        return 1;
    }
    
    read_history(HIST_FILE);    
    majComInt();
    
    while(1){
        input = readline(header);
        if(input == NULL){
            printf("\nEOF exit\n");
            Sortie(0, NULL);
        }
        save = input;
        
        while((command = strsep(&input, ";")) != NULL){
            if (strlen(command) == 0) continue;
            
            add_history(command);
            int N = analyseCom(command);
            if(N > 0){
                // Si ce n'est PAS une commande interne, on lance une externe
                if(execComInt(NMots, Mots) == 0){
                    execComExt(Mots);
                }   
            }
            freeMots();
        }
        free(save);
    }
    return 0;
}