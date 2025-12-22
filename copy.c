#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

#include "copy.h"

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n",__FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

void copy_files(const char *source, const char *target, mode_t mode){ //kopiujemy zawartosc source do target
    int in = open(source, O_RDONLY); 
    if(in < 0) ERR("open");
    int out = open(target, O_WRONLY | O_CREAT | O_TRUNC, mode); //otwieramy/tworzymy target
    if(out < 0) ERR("open");
    char buffer[4096]; //bufor do kopiowania
    ssize_t r; //stores num of bytes read
    while((r = read(in, buffer, sizeof(buffer))) > 0){ //czyta z in i zapisuje do bufora
        write(out, buffer, r);//zapisujemy z bufora do targetu
    }
    //closing file descriptors
    close(in);
    close(out);
}

void copy_dir(const char *source, const char *target){ //rekurencyjnie kopiujemy cale drzewo katalogow
    DIR *dir = opendir(source); //otwieramy katalog zrodlowy 
    if(!dir) ERR("opendir");

    struct stat st;
    if(lstat(source, &st) == -1) ERR("lstat"); //zapisujemy informacje o source w strukturze st
    if(mkdir(target, st.st_mode & 0777) == -1 && errno != EEXIST) ERR("mkdir"); //creates target directory, prawa dostepu jak w source, struktura katalogow jak w source
    
    struct dirent *ent; 
    while((ent = readdir(dir)) != NULL){ //iterujemy sie po zawartosci katalogu
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue; 
        //bufory na pelne sciezki
        char source_path[PATH_MAX];
        char target_path[PATH_MAX];
        //pelne sciezki
        snprintf(source_path, PATH_MAX, "%s/%s", source, ent->d_name);
        snprintf(target_path, PATH_MAX, "%s/%s", target, ent->d_name);

        if(lstat(source_path, &st) == -1) ERR("lstat");//pobieramy informacje o aktualnym wpisie
        if(S_ISDIR(st.st_mode)) //jesli katalog to rekurencja 
            copy_dir(source_path, target_path); 
        else if(S_ISREG(st.st_mode)) //normlany plik to copy_files
            copy_files(source_path, target_path, st.st_mode & 0777);  
    }
    closedir(dir);
}