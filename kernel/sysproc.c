#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint32 max_page = 32;
  uint64 start_page;
  int num_page; 
  uint64 user_abits;
  uint32 kbits = 0;
  pagetable_t pagetable = myproc()->pagetable;
  pte_t *pte;
  
  //vmprint(pagetable);

  if(argaddr(0, &start_page) < 0)
    return -1;
  if(argint(1, &num_page) < 0)
    return -1;
  if(argaddr(2, &user_abits) < 0)
    return -1;

  if(num_page > max_page)
    num_page = max_page;
  for(int i = 0; i < num_page; i++){ 
    pte = walk(pagetable, start_page + i * PGSIZE, 0);
    if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0)
      return -1; 
    if((*pte) & PTE_A){
      kbits = kbits | (1 << i);
      *pte = (*pte) & (~PTE_A);
    }
  }
  return copyout(pagetable, user_abits, (char *)&kbits, max_page);
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
