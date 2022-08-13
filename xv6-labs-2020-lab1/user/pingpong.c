#include "kernel/types.h"
#include "user/user.h"
#define RD 0 //pipe的read端
#define WR 1 //pipe的write端

int main(int argc, char const *argv[]) 
{
    int p_fd[2], c_fd[2];
    pipe(p_fd);//父用管道，信息传递：父进程->子进程
    pipe(c_fd);//子用管道，信息传递：子进程->父进程
    char buf[1];
    int pid=fork();//使用fork创建一个子进程
    int exit_status=0;

     if(pid == 0){//在子进程中
       close(p_fd[WR]);// 关闭管道父进程写端
       close(c_fd[RD]);//关闭子进程读端

        if(read(p_fd[0],buf,sizeof(char))!=sizeof(char)){//读取父进程发过来的消息
            fprintf(2, "child read() error!\n");
            exit_status = 1; //标记出错
         }
         else{
            fprintf(1,"%d: received ping\n",getpid());
         }   

        //子进程写入
        if (write(c_fd[WR],buf,sizeof(char)) != sizeof(char)) {
            fprintf(2, "child write() error!\n");
            exit_status = 1;
        }
        
        close(p_fd[RD]);
        close(c_fd[WR]);
    } 
    else if(pid<0){//出现错误

        fprintf(2, "fork() error!\n");
        close(p_fd[RD]);
        close(p_fd[WR]);
        close(c_fd[RD]);
        close(c_fd[WR]);
        exit(1);
    }
    else{ //在父进程中
        close(p_fd[RD]);
        close(c_fd[WR]);
       
       //父进程写入
       if ( write(p_fd[WR],buf,sizeof(char)) != sizeof(char)) {
            fprintf(2, "parent write() error!\n");
            exit_status = 1;
        }
       
        if(read(c_fd[RD],buf,sizeof(char))!=sizeof(char)){//父进程接收
            fprintf(2, "child read() error!\n");
            exit_status = 1; //标记出错
        }
        else{
            fprintf(1,"%d: received pong\n",getpid());
        }
           
        close(p_fd[WR]);
        close(c_fd[RD]);
    }
    exit(exit_status);
}