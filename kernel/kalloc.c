// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

void freerange(void *pa_start, void *pa_end);

uint64 pa2index(uint64 pa);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock reflock;
  //uint64 free_start;       // the start addr of kernel memory
  //uint64 pg_num = (PHYSTOP - KERNBASE) / PGSIZE; // the number of kernel pages
  int    pg_refcnt[PGNUM];      // the reference count of each kernel pages
} kpgref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&kpgref.reflock, "page_refcount");
  freerange(end, (void*)PHYSTOP);
  printf("PGNUM:%p\n", PGNUM);
  memset(kpgref.pg_refcnt, 0, sizeof(kpgref.pg_refcnt));
  printf("sizeof(kpgref.pg_refcnt):%p\n", sizeof(kpgref.pg_refcnt));
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
/*
  uint64 pa0 = 0x87f5e000;
  if(pa0 == (uint64)pa){
    printf("kfree before decrease_ref:pa=%p, refcnt=%d\n", pa0, pg_refcnt(pa0)); 
    if(myproc() != 0){
      printf("kfree before decrease_ref:pid=%d\n", myproc()->pid); 
    }
  }
*/
    
  if(decrease_ref((uint64)pa) == 1){
    //printf("kfree: refcnt > 1\n");
    return;
  }

/*
  if(pa0 == (uint64)pa){
    printf("kfree after decrease_ref:pa=%p, refcnt=%d\n", pa0, pg_refcnt(pa0)); 
    if(myproc() != 0)
      printf("kfree after decrease_ref:pid=%d\n", myproc()->pid); 
  }
*/

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    increment_ref((uint64)r, 0);
    if(kpgref.pg_refcnt[pa2index((uint64)r)] != 1)
      printf("kalloc: ref=%d pa=%p pid=%d should be 1, but not\n", kpgref.pg_refcnt[pa2index((uint64)r)], r, myproc()->pid);
/*
    uint64 pa0 = 0x87f5e000;
    if(pa0 == (uint64)r){
      printf("kalloc :pa=%p, refcnt=%d\n", pa0, pg_refcnt(pa0)); 
      if(myproc() != 0){
        printf("kalloc:pid=%d\n", myproc()->pid); 
      }
    }
*/
  }
  return (void*)r;
}

uint64
pa2index(uint64 pa)
{
  return (pa - KERNBASE) / PGSIZE;
}

void
increment_ref(uint64 pa, pte_t *pte)
{
  uint64 index = pa2index(pa);
  acquire(&kpgref.reflock);
  kpgref.pg_refcnt[index] += 1;
  if(pte != 0){
    *pte &= (~PTE_W);
    *pte |= PTE_COW;
  }
  release(&kpgref.reflock);
}

int
decrease_ref(uint64 pa)
{
  uint64 index = pa2index(pa);
  int ret = 0;
  acquire(&kpgref.reflock);
  if(kpgref.pg_refcnt[index] == 0){
    release(&kpgref.reflock);
    return ret;
  }
  if(kpgref.pg_refcnt[index] > 1){
    kpgref.pg_refcnt[index] -= 1;
    ret = 1; 
  } else if(kpgref.pg_refcnt[index] == 1){
    kpgref.pg_refcnt[index] = 0;
    ret = 0;
  } else {
    printf("decrease_ref() wrong,index:%p, refcnt:%p \n", index, kpgref.pg_refcnt[index]);
  }
  release(&kpgref.reflock);
  return ret;
}

int
pg_refcnt(uint64 pa)
{
  uint64 index = pa2index(pa);
  int refcnt = 0;
  refcnt = kpgref.pg_refcnt[index]; 
  return refcnt; 
}
