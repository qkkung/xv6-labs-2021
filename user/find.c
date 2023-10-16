#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), 0, DIRSIZ-strlen(p));
  return buf;
}

void
recurse_find(char *path, char *name)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    //printf("given name:%s, path name:%s, strcmp=%d\n", name, fmtname(path), strcmp(name, fmtname(path)));
    if(strcmp(name, fmtname(path)) == 0)
      printf("%s\n", path);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    //printf("T_DIR: path: %s.\n", buf);
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      //printf("de.name:%s, de.inum:%d, path=%s\n", de.name, de.inum, buf);
      if(de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      //printf("T_DIR: path: %s.\n", buf);
      recurse_find(buf, name);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc < 3){
    fprintf(2, "the number of arguments should be 2, but now is %d\n", argc -1);
    exit(-1);
  }

  struct stat st;
/*  int fd;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    exit(-1);
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    exit(-1);
  }

*/
  if(stat(argv[1], &st) < 0){
    fprintf(2, "find: cannot stat %s\n", argv[1]);
    exit(-1);
  }
  if(st.type != T_DIR){
    fprintf(2, "search path isn't the directory, but is %d\n", st.type);
    exit(-1);
  }
   
  recurse_find(argv[1], argv[2]);
  exit(0);
}
