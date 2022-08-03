#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int fd[2];
    if (pipe(fd) < 0){
        write(1, "Error!", 6);
        exit(1);
    }
    
    char buf[10];
    int pid = fork();
    
    if(pid == 0) { // child
        
        read(fd[0], buf, 1);
        printf("%d: received ping\n", getpid());
        
        write(fd[1], "x", 1); // child write next
    } else { // parent
        write(fd[1], "x", 1);  // parent write first
        
        read(fd[0], buf, 1);
        printf("%d: received pong\n", getpid());
    }
    exit(0);
}
