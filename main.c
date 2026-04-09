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

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

int isDirectory(char *arg)
{
    struct stat s;

    if(stat(arg, &s)==0)
    {
        if(S_ISDIR(s.st_mode))
        {
            return 1; // True
        }
    }

    return 0; // False
}

int main(int argc, char *argv[])
{
    if(argc<3)
    {
        printf("\nNieodpowiednia liczba argumentow.\n");
        return -1;
    }

    if(!isDirectory(argv[1]) && !isDirectory(argv[2]))
    {
        printf("\nPodany argument nie jest sciezka do katalogu\n");
        return -1;
    }

    if(strcmp(argv[1],argv[2])==0)
    {
        printf("\nSciezki sa takie same!\n");
        return -1;
    }

    int pauseTime=300;
    int recursion=0;
    long long sizeBorder=10024;

    for(int i=3;i<argc;i++)
    {
       if(strcmp(argv[i],"-P")==0&&argc>i+1) // Pause
        {
            i++;
            pauseTime=atoi(argv[i]);
        }
        if(strcmp(argv[i],"-R")==0) // Recursion
        {
            recursion=1;
        }
        if(strcmp(argv[i],"-B")==0&&argc>i+1) // Border
        {
            i++;
            sizeBorder=atoi(argv[i]);
        }
    }

    while(1)
    {
        // Testy
        printf("\nZadzialalo!\n");
        printf("\nPauza: %d\n",pauseTime);
        printf("\nRekurencja: %d\n",recursion);
        printf("\nRozmiar plikow od ktorych maja sie zaczynac duze: %lld\n",sizeBorder);
        break;
    }

    return 0;
}