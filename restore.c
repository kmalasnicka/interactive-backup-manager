#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include "copy.h"
#include "utils.h"

//restore source target -> wszystko co jest w targecie ma byc w source, wszystko czego nie ma w target znika z source 

static void remove_extra(const char *source, const char *target){ //usuwamy z source wszystko to czego nie ma w target
    DIR *d = opendir(source); //otwieramy source
    if(!d) return;

    struct dirent *ent;
    struct stat st;

    while((ent = readdir(d))){ //iterujemy sie po wszystkich elementach w source
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        char src[PATH_MAX];
        char tgt[PATH_MAX];
        //dla kazdego elementu w source tworzymy pelna sciezke 
        snprintf(src, PATH_MAX, "%s/%s", source, ent->d_name);
        snprintf(tgt, PATH_MAX, "%s/%s", target, ent->d_name);

        if(!path_exists(tgt)){ //jesli nie ma tego elementu w targecie
            if(lstat(src, &st) == -1) continue;
            if(S_ISDIR(st.st_mode)){ //jelsi katalog to rekurencja
                remove_extra(src, tgt);
                rmdir(src); //usuwamy katalog
            }else { 
                unlink(src);
            }
        }
    }
    closedir(d);
}

void restore_dir(const char *source, const char *target){
    DIR *d = opendir(target); //otwieramy target
    if(!d) return; 

    struct dirent *ent;
    struct stat st_src;
    struct stat st_tgt;
//jesli source nie istnieje to go tworzymy 
    if(mkdir(source, 0755) == -1 && errno != EEXIST){
        perror("mkdir");
        closedir(d);
        return;
    } 
    while((ent = readdir(d))){ //iterujemy sie po elementach w targecie
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        char src[PATH_MAX];
        char tgt[PATH_MAX];
        //dla kazdego pliku robimy pelna sciezke
        snprintf(src, PATH_MAX, "%s/%s", source, ent->d_name);
        snprintf(tgt, PATH_MAX, "%s/%s", target, ent->d_name);

        if(lstat(tgt, &st_tgt) == -1) continue; //chemy widziec symlinki ale nie podazac za nimi
        if(S_ISDIR(st_tgt.st_mode)){ //katalog to rekurencja
            restore_dir(src, tgt);
        } else if(S_ISREG(st_tgt.st_mode)){//plik
    //st_mtime - czas ostatniej modyfikacji jak sie rozni to zawartosc plikow moze byc inna
            if(!path_exists(src) || lstat(src, &st_src) == -1 || st_src.st_mtime < st_tgt.st_mtime){ //kopiujemy jak nie istnieje w source, nie da sie go sprawdzic lstat nie dziala albo istnieje ale ma inne mtime
                copy_files(tgt, src, st_tgt.st_mode & 0777); //kopiujemy z target do source
            }
        } else if(S_ISLNK(st_tgt.st_mode)){
            char buf[PATH_MAX];
            ssize_t len = readlink(tgt, buf, sizeof(buf) - 1);
            if(len == -1) continue;
            buf[len] = '\0';

            unlink(src);
            symlink(buf, src);
        }
    }
    closedir(d);
}

void restore_backup(const char *source, const char *target){
    restore_dir(source, target);
    remove_extra(source, target);
}
