#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int
is_prime(int value){
  if(value < 2){
    return 0;
  }
  for(int i = 2; i * i <= value; i++){
    if(value % i == 0){
      return 0;
    }
  }
  return 1;
}

void
child_proc(int *pipe_fd){
  close(pipe_fd[1]);

  int i;
  int n;
  int output_flag = 0;
  int fork_flag = 0;
  int p[2]; 
  pipe(p);

  while(read(pipe_fd[0], &i, sizeof(int)) > 0){
    if(output_flag == 1){
      if(fork_flag == 0){
        fork_flag = 1;
        if(fork() > 0){  //parent
          close(p[0]);
        } else { // child
          child_proc(p);
        }
      }  
      if(i % n == 0){
        continue;
      } 
      write(p[1], &i, sizeof(int));
      continue;
    }
    //close file, exit, wait

    if(is_prime(i) == 1){
      printf("prime %d\n", i);
      n = i;
      output_flag = 1;
    } 
  }  
  close(pipe_fd[0]);
  close(p[1]);
  if(wait(0) < 0){
    close(p[0]);
  }
  exit(0);
}  
  

int
main(int argc, char *argv[])
{
  int n = 2;
  int i = 2;
  int output_flag = 0;
  int fork_flag = 0;
  int p[2]; 
  pipe(p);

  for(; i < 36; i++){
    if(output_flag == 1){
      if(fork_flag == 0){
        fork_flag = 1;
        if(fork() > 0){  //parent
          close(p[0]);
        } else { // child
          child_proc(p);
        }
      }  
      if(i % n == 0){
        continue;
      } 
      write(p[1], &i, sizeof(int));
      continue;
    }

    if(is_prime(i) == 1){
      printf("prime %d\n", i);
      n = i;
      output_flag = 1;
    }
  }
  close(p[1]);
  if(wait(0) < 0){
    close(p[0]);
  }
  exit(0);
}
