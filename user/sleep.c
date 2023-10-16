#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char err_msg[] = "Missing an argument\n";
  if(argc < 2){
    write(2, err_msg, strlen(err_msg));
    exit(1);
  }
  
  int sleep_time = atoi(argv[1]);
  //printf("sleep_time: %d\n", sleep_time);
  if(sleep_time > 0){
    //printf("sleep_time > 0\n");
    sleep(sleep_time); 
  }
  exit(0);
}




