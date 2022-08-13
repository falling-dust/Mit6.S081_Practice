#include "kernel/types.h"
#include "user/user.h"

#define RD 0
#define WR 1

void useChildProcess(int p[]){
  int cp[2];
  int x,y;
  close(p[WR]);//关闭父进程的写端

  if(read(p[RD],&x,sizeof(int)))//读取父进程的第一个写入
  {
    fprintf(1, "prime %d\n", x);
    pipe(cp);//
    if(fork() != 0){//对于当前进程
      close(cp[RD]);//关闭当前的读端
      while(read(p[0],&y,sizeof(int))){//读取剩下所有的数
        if(y%x != 0){//将不含因数x的数写入下一个管道
	        write(cp[WR], &y, sizeof(int));
	      }
      }
	    close(p[RD]);
	    close(cp[WR]);
	    wait(0);
    }
    else{
     useChildProcess(cp);
   }
  }
   exit(0);
}

int main(int argc, char *argv[])
{
  int p[2];
  pipe(p);

  if(fork() != 0){
    close(p[RD]);//关闭父进程的读端

    for(int i = 2; i <= 35; i++){//对2-35的数进行写入操作
    	write(p[WR], &i, sizeof(int));
    }
    close(p[WR]);//关闭写端
    wait(0);
  }
  else{//在子进程
    useChildProcess(p);//
  }

  exit(0);
}

