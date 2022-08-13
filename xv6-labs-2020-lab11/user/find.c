#include "kernel/types.h"

#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

//参考ls.c
void find(char *path, const char *filename)
{
  char buf[512], *p;//设计buf用于记录文件前缀
  int fd;
  struct dirent de;//存储目录下的文件信息，主要用于索引
  struct stat st;//记录详细信息

  if ((fd = open(path, 0)) < 0) {//获取要打开的文件的文件描述符
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {//获取当前文件统计信息
    fprintf(2, "find: cannot fstat %s\n", path);
    close(fd);
    return;
  }

  //判断第一个参数（目录信息）是否错误
  if (st.type != T_DIR) {
    fprintf(2, "usage: find <DIRECTORY> <filename>\n");
    return;
  }

  //判断路径是否过大
  if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
    fprintf(2, "find: path too long\n");
    return;
  }

  //将路径信息放入buffer
  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/'; //p指针指向最后一个'/'

  while (read(fd, &de, sizeof de) == sizeof de) {//读取当前目录下的所有内容
    if (de.inum == 0)
      continue;
    memmove(p, de.name, DIRSIZ); //添加路径名称
    p[DIRSIZ] = 0;               //字符串结束标志
    if (stat(buf, &st) < 0) {
      fprintf(2, "find: cannot stat %s\n", buf);
      continue;
    }

    //不要在“.”和“..”目录中递归
    if (st.type == T_DIR && strcmp(p, ".") != 0 && strcmp(p, "..") != 0) {//在下一级文件夹中递归查找文件
      find(buf, filename);
    } 
    else if (strcmp(filename, p) == 0)//查找当前目录下是否有所需文件
      printf("%s\n", buf);
  }

  close(fd);
}

int main(int argc, char *argv[])
{
  if (argc != 3) {//若参数不正确
    fprintf(2, "usage: find <directory> <filename>\n");
    exit(1);
  }
  find(argv[1], argv[2]);//参数正确，调用函数
  exit(0);
}