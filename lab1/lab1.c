#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h> 
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc!=2)
    {
        fprintf(stderr, "Usage: ./a.out filename.\n"); 
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0)
    {
        fprintf(stderr, "Usage: ./a.out filename.\n"); 
        return 0;
    }

    struct stat st;
    if (stat(argv[1], &st) == 0) {
        fprintf(stderr, "Error: File already exists.\n");
        return 1;
    }

    int status;
    int max_length=100;
    char buf[max_length];
    pid_t child, Pid, Ppid;
    child= fork();
    if(child<0)
    {
        perror("fork");
        return 1;
    }

    int fd = open(argv[1], O_CREAT | O_APPEND | O_WRONLY, 0644);
    if (fd==-1)
    {
        perror("open");
        return 1;
    }

    if(child==0)
    {
        //child
        Pid= getpid();
        Ppid= getppid();
        snprintf(buf, sizeof(buf), "[CHILD] getpid()=%d, getppid()=%d\n", Pid, Ppid);
        if(write(fd, buf, strlen(buf)) < strlen(buf))
        {
            perror("write child");
            return 1; 
        }
        exit(0);
    }
    else {
        //parent
        Pid= getpid();
        Ppid= getppid();
        snprintf(buf, sizeof(buf), "[PARENT] getpid()=%d, getppid()=%d\n", Pid, Ppid);
        
        if (wait(&status) == -1)
        {
            perror("wait");
            return 1;
        }
        
        if(write(fd, buf, strlen(buf)) < strlen(buf))
        {
            perror("write parent");
            return 1;
        }

        if (close(fd)==-1)
        {
            perror("close");
            return 1; 
        }
        exit(0);
    }

    return 0;
}
