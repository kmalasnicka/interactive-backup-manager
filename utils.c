#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

#include "utils.h"

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

void usage(char *name){
    fprintf(stderr, "usage: %s command <source file> <target file>\n", name);
    exit(EXIT_FAILURE);
}

int is_empty(const char *path){
    DIR *d = opendir(path);
    if(!d) return -1;
    struct dirent *ent;
    while((ent = readdir(d)) != NULL){ //iterate through all directory entries 
        if(strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0){ 
            closedir(d);
            return 0; //not empty
        }
    } 
    closedir(d);
    return 1; //empty
}

int parse_line_with_quotes(char *line, char *argv[]){
    int argc = 0; 
    char *p = line; 

    while(*p && argc < MAX_ARGS){ 
        while(*p && isspace((unsigned char)*p)) p++; //pomijamy spacje tabulatory 
        if(!*p) break; //po przejsciu przez linie przerywamy

        if(*p == '"' || *p == '\''){// jesli aktualny znak to " lub '
            char quote = *p; //quote zapamietuje jaki to cudzyslow 
            p++;
            argv[argc] = p; //zapisujemy do argv wskaznik bez cudzyslowu z przodu dlatego robilismy p++
            while(*p && *p != quote) p++; //iterujemy sie dopoki nie dojdziemy do konca albo nie znjadziemy tego samego cudzyslowia co wczesniej
            if(!*p){ //nie ma zamykajacego cudzyslowia
                fprintf(stderr, "error unmatched quote\n");
                return 0;
            }
            *p = '\0'; //zastepujemy koncowy cudzyslow zerem 
            p++; //przesuwamy sie za cudzyslow
            argc++;
            //w efekcie mamy "my directrory" -> my directory\0
        } else { //argument bez cudzyslowu
            argv[argc] = p;
            argc++;
            while(*p && !isspace((unsigned char)*p)) p++;
            if(*p){
                *p = '\0';
                p++;
            }
        }
    }
    return argc;
}

int is_subpath(const char *parent, const char *child){
    char parent_real[PATH_MAX];
    char child_real[PATH_MAX];
//realpath zmienia sciezke na sciezke absolutna usuwa . i .. i przechodzi przez symlinki jesli katalog nie istnieje zwracamy -1
    if(!realpath(parent, parent_real)) return -1;
    if(!realpath(child, child_real)) return -1;

    size_t len = strlen(parent_real);

    if(strncmp(parent_real, child_real, len) != 0) return 0; //porownujemy pierwsze len znakow w tych sciezkach czyli patrezymy czy child_real sie zaczyna jak parent_real 
    return child_real[len] == '/' || child_real[len] == '\0'; //jesli parent i poczatek child sa rowne to patrzymy czy jest to podkatalog [len] = '/' lub jest to ten sam katalog [len] = '\0'
}

int path_exists(const char *path){
    struct stat st;
    if(lstat(path, &st) == 0) return 1;
    else return 0;
}