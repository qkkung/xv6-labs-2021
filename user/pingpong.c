#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p[2];
  pipe(p);
  char c;
  
  if(fork() == 0){
    close(p[1]);
    read(p[0], &c, 1);
    fprintf(1, "%d: received ping\n", getpid());
    close(p[0]);
    exit(0);
  } else {
    close(p[0]);
    write(p[1], "c", 1);
    close(p[1]);
    wait(0);
    fprintf(1, "%d: received pong\n", getpid());
    exit(0);
  }
}
    
    

