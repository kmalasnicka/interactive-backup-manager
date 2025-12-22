#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "monitor.h"
#include "copy.h"
static int g_inotify_fd = -1;
volatile sig_atomic_t running = 1;

#define ERR(source) (perror(source), exit(EXIT_FAILURE))
#define MAX_WATCHES 1024

struct watch_entry{ //pojedynczy wpis
    int wd; //watch descriptor 
    char path[PATH_MAX]; //sciezka pliku
};

static struct watch_entry watches[MAX_WATCHES]; //tablica struktur watch_entry
static int watch_count = 0;

static void sigterm_handler(int sig){
    (void)sig; 
    running = 0; //jesli rodzic zrobi kill to handler ustawia running na 0

    if(g_inotify_fd != -1){
        close(g_inotify_fd);
        g_inotify_fd = -1;
    }
}

void start_monitoring(const char *source, const char *target){
    int inotify_fd = inotify_init(); //tworzymy inotify instance
    if(inotify_fd < 0) ERR("inotify_init");

    g_inotify_fd = inotify_fd;

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    add_watches_recursive(inotify_fd, source); //dodajemy watch na source i wszystkie podkatalogi
    monitor_loop(inotify_fd, source, target); //czekamy i obslugujemy eventy

    if(g_inotify_fd != -1){
        close(g_inotify_fd);
        g_inotify_fd = -1;
    }
}

void add_watch(int inotify_fd, const char *path){
    int wd = inotify_add_watch( 
        inotify_fd,
        path,
        IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF
    ); 

    if(wd < 0) ERR("inotify_add_watch"); 
    if(watch_count >= MAX_WATCHES) ERR("too many watches");
    //przechowujemy wd i path w tablicy watches
    watches[watch_count].wd = wd; 
    strncpy(watches[watch_count].path, path, PATH_MAX); //kopiujemy path do watches[watches_count].path 
    watch_count++;
}

void add_watches_recursive(int inotify_fd, const char *path){
    add_watch(inotify_fd, path); //watch na aktualny katalog, kernel bedzie powiadamiac o zmianach w tym katalogu
    DIR *dir = opendir(path); //otwieramy katalog do czytania wpisow
    if(!dir) return;

    struct dirent *ent; 
    struct stat st; 

    while((ent = readdir(dir)) != NULL){ //iterujemy sie po wpisach katalogu
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue; 

        char subpath[PATH_MAX]; //buffor na pelna sciezke do aktualnego wpisu
        snprintf(subpath, PATH_MAX, "%s/%s", path, ent->d_name); 
        if(lstat(subpath, &st) == -1) continue; //lstat bo nie chcemy podazac za symlinkami

        if(S_ISDIR(st.st_mode)) { // jesli to katalog to rekurecja
            add_watches_recursive(inotify_fd, subpath);
        }
    }
    closedir(dir);
}

void monitor_loop(int inotify_fd, const char *source, const char *target){
    char buffer[4096]; //bufer na eventy
    
    while(running){ //jesli running jest 0 to konczymy petle 
        ssize_t len = read(inotify_fd, buffer, sizeof(buffer)); //read blokuje proces i czeka az kernel zglosi event gdy cos sie wydarzy to read zapisuje event do buffera i zwraca liczbe bajtow len
        
        if(!running) break;
        
        if(len < 0) {
                if(!running) break;
                if(errno == EINTR) continue;
                break;
        }

        for(char *ptr = buffer; ptr < buffer + len; ){ //iterujemy sie po eventach w buforze
            struct inotify_event *event = (struct inotify_event *)ptr; // aktualny fragment bufora to struct inotify_event
            handle_event(event, inotify_fd, source, target); //przekazujemy pojedyncze zdarzenue do handle_event
            ptr += sizeof(struct inotify_event) + event->len; //przesuwamy sie do nastepnego eventu
        }
    }
}

void handle_event(struct inotify_event *event, int inotify_fd, const char *source, const char *target){
    const char *base = find_path_by_wd(event->wd); //szukamy sciezki dla tego eventu 
    if(!base) return;

    if(event->len == 0) //sprawdamy czy evenr ma nazwe
    return;

    char source_path[PATH_MAX];
    snprintf(source_path, PATH_MAX, "%s/%s", base, event->name); //budujemy pelna sciezke zrodlowa

    char target_path[PATH_MAX];
    snprintf(target_path, PATH_MAX, "%s%s", target, source_path + strlen(source)); // source_path + strlen(source_root) usuwa source z poczatku i dokleja reszte do targetu

    struct stat st;
    if(lstat(source_path, &st) == -1) return; 

    if(event->mask & IN_CREATE){
        if(S_ISDIR(st.st_mode)){ //jesli katalog
            mkdir(target_path, 0755);
            copy_dir(source_path, target_path);
            add_watches_recursive(inotify_fd, source_path);
        } else if(S_ISREG(st.st_mode)){//jesli plik
            copy_files(source_path, target_path, 0644);
        }
    }

    if(event->mask & IN_MODIFY){ //plik sie zmienil
        if(S_ISREG(st.st_mode)){
            copy_files(source_path, target_path, 0644);//nadpisujemy w targecie
        }
    }

    if(event->mask & IN_DELETE || event->mask & IN_MOVED_FROM){
        if(event->mask & IN_ISDIR){ //katalog
            rmdir(target_path); //usuwamy w target
        } else { //plik
            unlink(target_path); //usuwamy w target
        }
    }

    if(event->mask & IN_MOVED_TO){
        if(S_ISDIR(st.st_mode)){
            mkdir(target_path, 0755);//tworzymy nowa wersje w target
            copy_dir(source_path, target_path);
            add_watches_recursive(inotify_fd, source_path);
        } else if (S_ISREG(st.st_mode)){
            copy_files(source_path, target_path, 0644);
        }
    }
    
    if(event->mask & IN_DELETE_SELF){ //katalog zostal usuniety
        remove_watch(inotify_fd, event->wd); //usuwamy watch
    }
}

const char *find_path_by_wd(int wd){ //zwraca path odpowiadajacy danemu watch descriptorowi 
    for(int i = 0; i < watch_count; i++){
        if(watches[i].wd == wd){ //szukamy wpisu z tym samym wd
            return watches[i].path; //zwracamy odpowiadajaca wd sciezke
        }
    }
    return NULL;
}

void remove_watch(int inotify_fd, int wd){
    inotify_rm_watch(inotify_fd, wd); //usuwamy watch z tego katalogu
    for(int i = 0; i < watch_count; i++){
        if(watches[i].wd == wd){
            watches[i] = watches[watch_count - 1]; //usuwamy wpis z tablisy 
            watch_count--; //zmniejszamy watch_count
            return;
        }
    }
}