#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <limits.h>

#include "backup.h"
#include "utils.h"
#include "restore.h"

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n",__FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))
volatile sig_atomic_t exiting = 0;

typedef struct backup_entry{ //lista aktywnych backupow zeby wiedziec kogo zabic
    pid_t pid; //pid dziecka dla backupu
    char source[PATH_MAX];
    char target[PATH_MAX];
    struct backup_entry *next; //kolejny backup
} backup_entry_t;

static backup_entry_t *head = NULL; //poczatek listy backupow 

void add_entry(pid_t pid, const char *source, const char *target){
    backup_entry_t *e = malloc(sizeof(*e)); //allocujemy nowy element listy
    if(!e) ERR("malloc");
    //zapisujemy pid i sciezki target i source
    e->pid = pid; 

    strncpy(e->source, source, PATH_MAX);
    e->source[PATH_MAX - 1] = '\0';

    strncpy(e->target, target, PATH_MAX);
    e->target[PATH_MAX - 1] = '\0';

    e->next = head; //dodajemy na poczatek listy 
    head = e; //pointer wskazuje na nasz element
}

void end_backup(const char *source, const char *target){
    backup_entry_t **cur = &head; 
//iterujemuy sie po liscie 
    while(*cur){
        backup_entry_t *e = *cur; //e to biezacy element 
        if(strcmp(e->source, source) == 0 && strcmp(e->target, target) == 0){ //sprawdzamy czy to ten backup ktory chcemy zakonczyc
            kill(e->pid, SIGTERM); //wysylamy sigterm do dziecka zeby zakonczyc backup
            waitpid(e->pid, NULL, 0); //czekamy az dziecko umrze
            *cur = e->next; //usuwamy element z listy
            free(e); 
            printf("backup stopped: %s -> %s\n", source, target);
            return;
        }
        cur = &(*cur)->next; //jak to nie byl ten element to sie przesuywamy dalej
    }
    printf("backup not found: %s -> %s\n", source, target);
}

void list_backups(){
    backup_entry_t *cur = head;
    if(cur == NULL){
        printf("no active backups\n");
        return;
    }
    printf("active backups:\n");
    while(cur){
        printf("%s -> %s (pid: %d)\n", cur->source, cur->target, cur->pid);
        cur = cur->next;
    }
} 

void sig_parent_handler(int sig){
    (void)sig;
    exiting = 1;
}

int main(int argc, char *argv[]){
    signal(SIGINT, sig_parent_handler);
    signal(SIGTERM, sig_parent_handler);

    char line[512]; //bufor na input
    printf("commands: \n");
    printf("add <source> <target>\n");
    printf("end <source> <target>\n");
    printf("restore <source> <target>\n");
    printf("list\n");
    printf("exit\n");

    while(!exiting){
        printf("> "); 
        fflush(stdout); 
        
        if(!fgets(line, sizeof(line), stdin)){ //czytamy linijke z stdin
            if(exiting) break;
            continue; 
        }
        if(exiting) break;

        line[strcspn(line, "\n")] = 0; //usuwamy newline
        if(strlen(line) == 0) continue;

        char *args[MAX_ARGS];
        int arg_count = parse_line_with_quotes(line, args);

        if(arg_count == 0) continue;

        char *command = args[0]; 
        if(!command) continue;

        if(strcmp(command, "add") == 0){ 

            if(arg_count < 3){
                printf("usage: add <source> <target> [target...]\n");
                continue;
            }

            char *source = args[1];
            for(int i = 2; i < arg_count; i++){
                char *target = args[i];
                pid_t pid = fork(); //tworzymy proces dla kazdego targeta
                if(pid < 0) { 
                    perror("fork"); 
                    continue;
                }
                    if(pid == 0) { //dziecko robi backup + monitoring
                        backup(source, target); 
                        exit(EXIT_SUCCESS);
                    } else {
                        add_entry(pid, source, target); //zapisujemy pid do listyw rodzicu
                    }
                }
            }
        else if(strcmp(command, "end") == 0){
            if(arg_count != 3){
                printf("usage: end <source> <target>\n");
                continue;
            }
            end_backup(args[1], args[2]);
        } else if(strcmp(command, "exit") == 0){
            break;
        } else if (strcmp(command, "list") == 0){
            list_backups();
        } else if(strcmp(command, "restore") == 0){
            if(arg_count != 3){
                printf("usage: restore <source path> <target path>\n");
                continue;
            }
            backup_entry_t *cur = head;
            while(cur){ //zatrzymujemy wszytskie aktywne backupy
                kill(cur->pid, SIGTERM);
                waitpid(cur->pid, NULL, 0);
                cur = cur->next;
            }
            while(head){ //czyscimy liste backupow
                backup_entry_t *tmp = head;
                head = head->next;
                free(tmp);
            }
            restore_backup(args[1], args[2]);
        } else {
            printf("unknown command\n");
        }
    }
    while(head){ //konczymy wszystkie aktywne backupy zeby nie zostawic dzialajacych procesow 
        kill(head->pid, SIGTERM);
        waitpid(head->pid, NULL, 0);
        backup_entry_t *tmp = head;
        head = head->next;
        free(tmp); 
    }
    return 0;
}