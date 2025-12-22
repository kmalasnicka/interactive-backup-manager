#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "backup.h"
#include "utils.h"
#include "copy.h"
#include "monitor.h"

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n",__FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

void backup(char *source, char *target){ //robimy kopie calego katalogu source do target

    if(is_subpath(source, target) == 1 || is_subpath(target, source) == 1){ //jesli target jest w source lub na odwrot to jest to blad!
        fprintf(stderr, "source and target cannot be subdirectories of each other\n");
        exit(EXIT_FAILURE);
    }

    struct stat st;
    if(lstat(source, &st) == -1){ //sprawdzamy czy source istnieje
        fprintf(stderr, "source does not exist\n");
        exit(EXIT_FAILURE);
    }

    if(!S_ISDIR(st.st_mode)){ //sprawdzamy czy source jest katalogiem
        fprintf(stderr, "source is not a directory\n");
        exit(EXIT_FAILURE);
    }

    if(lstat(target, &st) == -1){ //sprawdzamy czy target istnieje jak nie to tworzyny
        if(mkdir(target, 0755) == -1 && errno != EEXIST) ERR("mkdir"); 
    } else {
        if(!S_ISDIR(st.st_mode)){ // target istnieje ale nie jest katalogiem
            fprintf(stderr, "target exists but is not a directory\n");
            exit(EXIT_FAILURE);
        }
        if(is_empty(target) == 0){ //target musi byc pusty
            fprintf(stderr, "target directory must be empty\n");
            exit(EXIT_FAILURE);
        }
    }
    copy_dir(source, target); 
    start_monitoring(source, target); //zaczynamy monitorowanie
}
