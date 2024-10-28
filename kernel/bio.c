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

#define NBUCKET 13

uint hash(int x){
  return x % NBUCKET;
}

struct {
  struct spinlock global_lock;
  struct spinlock lock[NBUCKET];
  struct buf buf[NBUF];
  struct buf bucket[NBUCKET];
} bcache;

char bcache_name[NBUCKET][16];

void
binit(void)
{
  struct buf *b;
  initlock(&bcache.global_lock, "bcache");
  for (int i = 0; i < NBUCKET; i++){
    snprintf(bcache_name[i], sizeof(bcache_name[i]), "bcache%d", i);
    initlock(&bcache.lock[i], bcache_name[i]);

    // Create linked list of buffers
    bcache.bucket[i].prev = &bcache.bucket[i];
    bcache.bucket[i].next = &bcache.bucket[i];
  }

  for(int i=0; i<NBUF; i++){
    uint key = hash(i);
    b = &bcache.buf[i];
    b->next = bcache.bucket[key].next;
    b->prev = &bcache.bucket[key];
    
    initsleeplock(&b->lock, "buffer");
    
    bcache.bucket[key].next->prev = b;
    bcache.bucket[key].next = b;
  }
}

static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint bucket_key = hash(blockno);
  acquire(&bcache.lock[bucket_key]);

  // Is the block already cached?
  for(b = bcache.bucket[bucket_key].next; b != &bcache.bucket[bucket_key]; b = b->next){
    //判断命中
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[bucket_key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.bucket[bucket_key].prev; b != &bcache.bucket[bucket_key]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[bucket_key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.lock[bucket_key]);
  acquire(&bcache.global_lock);
  for (int i = 0; i < NBUCKET; i++){
    //跳过未命中的桶
    if (i == bucket_key){
      continue;
    }
    acquire(&bcache.lock[i]);
    for(b = bcache.bucket[i].prev; b != &bcache.bucket[i]; b = b->prev){
      // 寻找空闲块
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // 从第i个桶内移除该块
        b->prev->next = b->next;
        b->next->prev = b->prev;

        // 将该块加入bucket_key号桶
        acquire(&bcache.lock[bucket_key]);
        b->next = &bcache.bucket[bucket_key];
        b->prev = bcache.bucket[bucket_key].prev;
        bcache.bucket[bucket_key].prev->next = b;
        bcache.bucket[bucket_key].prev = b;
        release(&bcache.lock[bucket_key]);
        
        release(&bcache.lock[i]);
        release(&bcache.global_lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[i]);
  }
  release(&bcache.global_lock);
  panic("bget: no buffers");
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

  uint bucket_key = hash(b->blockno);
  acquire(&bcache.lock[bucket_key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;

    b->next = bcache.bucket[bucket_key].next;
    b->prev = &bcache.bucket[bucket_key];
    bcache.bucket[bucket_key].next->prev = b;
    bcache.bucket[bucket_key].next = b;
  }
  release(&bcache.lock[bucket_key]);
}

void
bpin(struct buf *b) {
  uint bucket_key = hash(b->blockno);
  acquire(&bcache.lock[bucket_key]);
  b->refcnt++;
  release(&bcache.lock[bucket_key]);
}

void
bunpin(struct buf *b) {
  uint bucket_key = hash(b->blockno);
  acquire(&bcache.lock[bucket_key]);
  b->refcnt--;
  release(&bcache.lock[bucket_key]);
}


