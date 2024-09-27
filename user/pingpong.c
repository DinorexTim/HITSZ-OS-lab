#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    int c2f[2];   // 子进程to父进程管道
    int f2c[2];   // 子进程to父进程管道

    
    pipe(c2f);
    pipe(f2c);
    
    int pid = fork();

    if (pid == 0){
        int pid_p;
        /* 子进程读取父进程pid */
        close(f2c[1]);
        /* 子进程传递pid */
        close(c2f[0]);
        read(f2c[0], &pid_p, sizeof(pid_p));
        printf("%d: received ping from pid %d\n",getpid(),pid_p);
        int pid_parse = getpid();
        write(c2f[1],&pid_parse,sizeof(pid_parse));  
        close(f2c[0]);   
        close(c2f[1]);

    }else{
        int pid_c;
        /* 父进程传递pid */
        close(f2c[0]);   
        /* 父进程读取子进程pid */
        close(c2f[1]);
        int pid_parse = getpid();
        write(f2c[1],&pid_parse,sizeof(pid_parse));   
        read(c2f[0], &pid_c, sizeof(pid_c));
        printf("%d: received pong from pid %d\n",getpid(),pid_c);
        close(f2c[1]); 
        close(c2f[0]); 
    }
    exit(0);
}