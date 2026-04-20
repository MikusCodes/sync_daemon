// Jak skompilowac w program?
//
// gcc main.c -o sync_daemon
//
// Jak wywolac?
// ./sync_daemon <path1> <path2> <arguments>
// <path1> and <path2> cant be the same and they are needed for this to work
// <arguments>:
// -R (Recursion) includes directories in soft works
// -B <size> (Border) set border between big and small files
// -P <time> (Pause) time in seconds how often software should sync directories
//
// Sprawdzenie czy proces żyje
// ps aux | grep sync_daemon
//
// Obserwowanie logów demona
// journalctl -t sync_daemon -f
//
// Znajdź PID swojego demona
// PID=$(pgrep sync_daemon)

// Wymuszenie natychmiastowej synchronizacji
// kill -SIGUSR1 $PID

// Zakończenie pracy demona
// kill -SIGUSR2 $PID
#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <utime.h>
#include <dirent.h>

int isDirectory(char *arg);
void parseArguments(int argc, char *argv[]);
void checkSource(int argc, char *source, char *destination);
void detachFromTerminal(void);
void initDeamon(void);
void setupSignals(void);
void my_handler(int sig);
void exitFunction(int sig);
void copyFile(const char *src_path, const char *dst_path, long long border);
void removeDirectoryRecursively(const char *path);
void syncDirectories(const char *src_dir, const char *dst_dir);

int pauseTime = 300;
int recursion = 0;
long long sizeBorder = 10024;
volatile int signalReceived = 0;
volatile int exitSignal = 0;

int main(int argc, char *argv[])
{
    checkSource(argc, argv[1], argv[2]);
    parseArguments(argc, argv);
    initDeamon();
    setupSignals();

    while (!exitSignal)
    {
        syslog(LOG_INFO, "Demon usypia.");
        sleep(pauseTime);

        if (exitSignal)
        {
            break;
        }

        if (signalReceived)
        {
            syslog(LOG_INFO, "Demon wybudzony natychmiastowo.");
            signalReceived = 0;
        }
        else
        {
            syslog(LOG_INFO, "Demon wybudzony naturalnie.");
        }

        syncDirectories(argv[1], argv[2]);
    }

    syslog(LOG_INFO, "Demon konczy dzialanie.");
    closelog();
    return 0;
}

int isDirectory(char *arg)
{
    struct stat s;

    if (stat(arg, &s) == 0)
    {
        if (S_ISDIR(s.st_mode))
        {
            return 1; // True
        }
    }

    return 0; // False
}

void checkSource(int argumentCount, char *source, char *destination)
{

    if (argumentCount < 3)
    {
        printf("\nNieodpowiednia liczba argumentow.\n");
        exit(-1);
    }

    if (!isDirectory(source) && !isDirectory(destination))
    {
        printf("\nZaden z podanych argumentow nie jest sciezka do katalogu\n");
        exit(-1);
    }

    if (!isDirectory(source))
    {
        printf("\nZrodlo nie jest sciezka do katalogu\n");
        exit(-1);
    }

    if (!isDirectory(destination))
    {
        printf("\nCel nie jest sciezka do katalogu\n");
        exit(-1);
    }

    if (strcmp(source, destination) == 0)
    {
        printf("\nSciezki sa takie same!\n");
        exit(-1);
    }

    if (isDirectory(source) && isDirectory(destination))
    {
        return;
    }
}

void parseArguments(int argc, char *argv[])
{
    for (int i = 3; i < argc; i++)
    {
        if (strcmp(argv[i], "-P") == 0 && argc > i + 1) // pause
        {
            i++;
            pauseTime = atoi(argv[i]);
            if(pauseTime<0)
            {
                printf("\nPrzerwawa miedzy sprawdzaniem nie moze byc mniejsza niz 0.\n");
                exit(-1);
            }
        }
        else if (strcmp(argv[i], "-R") == 0) // Recursion
        {
            recursion = 1;
        }
        else if (strcmp(argv[i], "-B") == 0 && argc > i + 1) // Border
        {
            i++;
            sizeBorder = atoll(argv[i]); // atoll dla long long
            if(sizeBorder<1)
            {
                printf("\nRozmair bufforu nie moze byc mniejsza niz 1.\n");
                exit(-1);
            }
        }
        else
        {
            printf("Nieznany argument lub brak wartosci: %s\n", argv[i]);
        }
    }
}

void initDeamon(void)
{
    pid_t pid;
    pid = fork();

    if (pid < 0)
    {
        exit(EXIT_FAILURE);
    }

    else if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }
    detachFromTerminal();
}

void detachFromTerminal(void)
{
    if (setsid() < 0)
    {
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    umask(0);
    openlog("sync_daemon", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Demon synchronizacji uruchomiony.");
}

void setupSignals(void)
{
    struct sigaction myAction;
    myAction.sa_flags = 0;
    sigfillset(&myAction.sa_mask);

    // Rejestracja SIGUSR1 (Budzenie)
    myAction.sa_handler = my_handler;
    if (sigaction(SIGUSR1, &myAction, NULL) < 0)
    {
        perror("Błąd rejestracji SIGUSR1");
        exit(EXIT_FAILURE);
    }

    // Rejestracja SIGUSR2 (Zamykanie)
    myAction.sa_handler = exitFunction;
    if (sigaction(SIGUSR2, &myAction, NULL) < 0)
    {
        perror("Błąd rejestracji SIGUSR2");
        exit(EXIT_FAILURE);
    }
}

void my_handler(int sig)
{
    syslog(LOG_INFO, "Daemon received signal SIGUSR1\n");
    signalReceived = 1;
}

void exitFunction(int sig)
{
    syslog(LOG_INFO, "Daemon received signal SIGUSR2\n");
    exitSignal = 1;
}

void copyFile(const char *src_path, const char *dst_path, long long border)
{
    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) return;

    struct stat st;
    if (fstat(src_fd, &st) < 0)
    {
        close(src_fd);
        return;
    }

    int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dst_fd < 0)
    {
        close(src_fd);
        return;
    }

    if (st.st_size < border)
    {
        char buffer[8192];
        ssize_t bytes_read;
        while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0)
        {
            char *out_ptr = buffer;
            ssize_t bytes_to_write = bytes_read;
            while (bytes_to_write > 0)
            {
                ssize_t bytes_written = write(dst_fd, out_ptr, bytes_to_write);
                if (bytes_written < 0) break;
                bytes_to_write -= bytes_written;
                out_ptr += bytes_written;
            }
        }
    }
    else
    {
        if (st.st_size > 0)
        {
            char *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0);
            if (map != MAP_FAILED)
            {
                ssize_t total_written = 0;
                while (total_written < st.st_size)
                {
                    ssize_t bytes_written = write(dst_fd, map + total_written, st.st_size - total_written);
                    if (bytes_written < 0) break;
                    total_written += bytes_written;
                }
                munmap(map, st.st_size);
            }
        }
    }

    close(src_fd);
    close(dst_fd);

    struct utimbuf new_times;
    new_times.actime = st.st_atime;
    new_times.modtime = st.st_mtime;
    utime(dst_path, &new_times);

    syslog(LOG_INFO, "Skopiowano plik: %s", src_path);
}

void removeDirectoryRecursively(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                removeDirectoryRecursively(full_path);
            }
            else
            {
                unlink(full_path);
                syslog(LOG_INFO, "Usunieto plik: %s", full_path);
            }
        }
    }
    closedir(dir);
    rmdir(path);
    syslog(LOG_INFO, "Usunieto katalog: %s", path);
}

void syncDirectories(const char *src_dir, const char *dst_dir)
{
    DIR *dir = opendir(src_dir);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char src_path[1024];
        char dst_path[1024];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entry->d_name);

        struct stat src_stat;
        if (lstat(src_path, &src_stat) == -1) continue;

        if (S_ISDIR(src_stat.st_mode))
        {
            if (recursion)
            {
                struct stat dst_stat;
                if (lstat(dst_path, &dst_stat) == -1)
                {
                    mkdir(dst_path, src_stat.st_mode);
                    syslog(LOG_INFO, "Utworzono katalog: %s", dst_path);
                }
                syncDirectories(src_path, dst_path);
            }
        }
        else if (S_ISREG(src_stat.st_mode))
        {
            struct stat dst_stat;
            int needs_copy = 1;

            if (lstat(dst_path, &dst_stat) == 0)
            {
                if (S_ISREG(dst_stat.st_mode) && dst_stat.st_mtime >= src_stat.st_mtime)
                {
                    needs_copy = 0;
                }
            }

            if (needs_copy)
            {
                copyFile(src_path, dst_path, sizeBorder);
            }
        }
    }
    closedir(dir);

    dir = opendir(dst_dir);
    if (!dir) return;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char src_path[1024];
        char dst_path[1024];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entry->d_name);

        struct stat src_stat;
        if (lstat(src_path, &src_stat) == -1)
        {
            struct stat dst_stat;
            if (lstat(dst_path, &dst_stat) == 0)
            {
                if (S_ISDIR(dst_stat.st_mode))
                {
                    if (recursion)
                    {
                        removeDirectoryRecursively(dst_path);
                    }
                }
                else if (S_ISREG(dst_stat.st_mode))
                {
                    unlink(dst_path);
                    syslog(LOG_INFO, "Usunieto plik: %s", dst_path);
                }
            }
        }
    }
    closedir(dir);
}