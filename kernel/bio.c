// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;

  struct buf *buckets[NBUCKET];
  struct spinlock locks[NBUCKET];
} bcache;

extern uint ticks;


void
addbucket(struct buf *b)
{
  int hash = b->blockno % NBUCKET;
  acquire(&bcache.locks[hash]);
  b->next = bcache.buckets[hash];
  if(bcache.buckets[hash])
    bcache.buckets[hash]->prev = b;
  bcache.buckets[hash] = b;
  b->prev = 0;
  release(&bcache.locks[hash]);
}

// must already hold the bucket lock
void
delbucket(struct buf *b)
{
  int hash = b->blockno % NBUCKET;
  if(b->prev == 0){
    bcache.buckets[hash] = b->next;
    if(b->next)
      b->next->prev = 0;
    return;
  }
  
  b->prev->next = b->next;
  if(b->next)
    b->next->prev = b->prev;
}
  
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for(int i = 0; i < NBUCKET; i++)
    initlock(&bcache.locks[i], "bcache bucket"); 

  for(int j = 0; j < NBUF; j++){
    b = &bcache.buf[j];
    if(b->blockno != 0)
      panic("bcache init"); 
    addbucket(b);
  }
/*
  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
*/
}

//return 1, if find the block
//return 0, otherwise
int
searchb(uint dev, uint blockno, struct buf **bp)
{
  struct buf *b;

  int i = blockno % NBUCKET;
  acquire(&bcache.locks[i]);
  b = bcache.buckets[i];
  while(b){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      *bp = b;
      release(&bcache.locks[i]);
      acquiresleep(&b->lock);
      return 1;
    }
    b = b->next;
  }
  release(&bcache.locks[i]);
  return 0;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  if(searchb(dev, blockno, &b))
    return b;

  acquire(&bcache.lock);

  //When two processes concurrently use the same block,
  //but miss in cache. One of them will find unused buffer and put it
  //in buckets
  if(searchb(dev, blockno, &b)){
    release(&bcache.lock);
    return b;
  }
  
  struct buf *oldest = 0;
  struct buf *older = 0;
  for(int j = 0; j < NBUCKET; j++){
    acquire(&bcache.locks[j]);
    b = bcache.buckets[j]; 
    older = oldest;
    while(b){
      if(b->refcnt == 0){
        if(oldest == 0 || oldest->timestamp > b->timestamp)
          oldest = b;
      }    
      b = b->next;
    }
    // not update oldest in this loop iteration
    if(oldest == older)
      release(&bcache.locks[j]);
    else if(older)
      release(&bcache.locks[older->blockno % NBUCKET]);
  }
  
  if(oldest){
    uint i = oldest->blockno % NBUCKET;
    delbucket(oldest);
    oldest->dev = dev;
    oldest->blockno = blockno;
    oldest->valid = 0;
    oldest->refcnt = 1;
    release(&bcache.locks[i]);
    addbucket(oldest);
    release(&bcache.lock);
    acquiresleep(&oldest->lock);
    return oldest;
    }

  panic("bget: no buffers");

/*
  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
*/
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int hash = b->blockno % NBUCKET;
  acquire(&bcache.locks[hash]);
  b->refcnt--;
  b->timestamp = ticks;
  release(&bcache.locks[hash]);

/*
  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
*/
}

void
bpin(struct buf *b) {
  acquire(&bcache.locks[b->blockno % NBUCKET]);
  b->refcnt++;
  release(&bcache.locks[b->blockno % NBUCKET]);

/*
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
*/
}

void
bunpin(struct buf *b) {
  acquire(&bcache.locks[b->blockno % NBUCKET]);
  b->refcnt--;
  release(&bcache.locks[b->blockno % NBUCKET]);
/*
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
*/
}


