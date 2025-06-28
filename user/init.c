// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"
#include "kernel/fcntl.h"

char *argv[] = { "sh", 0 };
//用户空间的“第一个进程”，它负责：设置好输入输出，启动shell,保证shell一直运行
int
main(void)//创建一个代表console的设备
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 0);//创建console设备；因为是第一个打开的文件，所以这里的文件描述符为0
    open("console", O_RDWR);
  }
  dup(0);  // stdout 复制文件描述符0，得到文件描述符1
  dup(0);  // stderr 得到文件描述符2
  //最终文件描述符0，1，2都用来代表console

  for(;;){//init会一直尝试启动sh
    printf("init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf("init: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      exec("sh", argv);//子进程调用exec，启动shell;
      printf("init: exec sh failed\n");
      exit(1);
    }

    for(;;){
      // this call to wait() returns if the shell exits,
      // or if a parentless process exits.
      wpid = wait((int *) 0);//父进程调用wait()来回收子进程
      if(wpid == pid){
        // the shell exited; restart it.
        break;
      } else if(wpid < 0){
        printf("init: wait returned an error\n");
        exit(1);
      } else {
        // it was a parentless process; do nothing.
      }
    }
  }
}
