#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int
read_lines(char *buf, char *arg_arr[])
{
  char *p = buf;
  char n;
  int arg_num = 0;
  arg_arr[0] = buf;
  while(read(0, &n, 1) == 1){
    //printf("while loop %d\n", n);
    if(arg_num == MAXARG - 1){ 
      fprintf(2, "arguments exceed max limit:%d\n", MAXARG);
      break;
    }
    
    if('\n' == n){
      *p++ = 0;
      arg_num++;
      arg_arr[arg_num] = p;
      continue;
    }
    if('n' == n){
      p--;
      if(p >= buf && *p == '\\'){
        *p++ = 0;
        arg_num++;
        arg_arr[arg_num] = p;
        continue;
      } 
      p++;
      *p++ = n;
      continue;
    }
    *p++ = n;
    continue;   
  } 
  //printf("read_lines:%s, %d\n", buf, p - buf);
  // support echo "a\nb" | xargs echo c
/*  if(strlen(buf) > 0 && *(--p) != 0){
    *(++p) = 0;
    return arg_num + 1;
  }
  printf("read_lines2:%s, %d\n", buf, p - buf);
*/

  // support echo "a\n" | xargs echo 
  return arg_num;

}

int
main(int argc, char *argv[])
{
  if(argc < 2)
    fprintf(2, "no command parameter");
  
  char buf[512];
  char *arg_arr[MAXARG];
  int arg_lines = read_lines(buf, arg_arr);
  //printf("buf:%s, %d\n", arg_arr[0], arg_lines);

  // construct new argv for exec()
  char *new_argv[argc + 1];
  for(int m = 1; m < argc; m++){
    new_argv[m - 1] = argv[m]; 
  }
  new_argv[argc] = 0;
  for(int n = 0; n < arg_lines; n++){
    new_argv[argc - 1] = arg_arr[n];
    if(fork() == 0){
      exec(new_argv[0], new_argv);
    } else {
      wait(0);
    } 
  }
  exit(0);

}
