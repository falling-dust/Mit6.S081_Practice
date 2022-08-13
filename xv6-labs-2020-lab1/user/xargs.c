#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char* argv[])
{
    char buf[32];
    int n;
    // 对于参数不足的命令可以直接输出
    if (argc < 2) {
        while((n = read(0, buf, sizeof buf)) > 0) {
            write(1, buf, n);
        }
        exit(0);
    }

    //获取所有参数
    char* args[MAXARG];
    int numArg;
    for (int i = 1; i < argc; ++i) {
        args[i-1] = argv[i];
    }
    numArg = argc - 1;//参数个数
    
    char* p = buf;
    while ((n = read(0, p, 1)) > 0) {
        if (*p == '\n') {//遇到回车，执行一次命令
            *p = 0;
            if (fork() == 0) {//若在子进程中
                args[numArg] = buf;//将标准输入数据作为最后一个参数
                exec(args[0], args);//对所有参数执行命令
                exit(0);
            } 
            else {
                wait(0);
            }
            p = buf;
        } 
        else {
            ++p;
        }
    }
    exit(0);
}

