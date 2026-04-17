// Jak skompilowac w program?
//
// gcc main.c -o main
//
// Jak wywolac?
// ./main <path1> <path2> <arguments>
// <path1> and <path2> cant be the same and they are needed for this to work
// <arguments>:
// -R (Recursion) includes directories in soft works
// -B <size> (Border) set border between big and small files
// -P <time> (Pause) time in seconds how often software should sync directories
#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>

int isDirectory(char *arg);
void parseArguments(int argc, char *argv[]);
void checkSource(int argc, char *source, char *destination);
void detachFromTerminal(void);
void initDeamon(void);
void setupSignals(void);
void my_handler(int sig);
void exitFunction(int sig);

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

    /*while(!exitSignal)
    {

    }*/

    while (1)
    {
        // Testy
        syslog(LOG_INFO, "Demon zyje i czeka...");
        sleep(30); // Czekaj 30 sekund przed kolejnym wpisem do logu
    }

    // potem bede sie tam dalej bawił i trzeba zrobis setsid() ustawic lidera sesja Done
    // i dzialac na katalogach
    /*

*/
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
        }
        else if (strcmp(argv[i], "-R") == 0) // Recursion
        {
            recursion = 1;
        }
        else if (strcmp(argv[i], "-B") == 0 && argc > i + 1) // Border
        {
            i++;
            sizeBorder = atoll(argv[i]); // atoll dla long long
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
        syslog(LOG_ERR, "Error with session opening\n");
        exit(EXIT_FAILURE);
    }

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