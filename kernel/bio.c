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

struct bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct buf buf[NBUF];
  struct bucket buckets[NBUCKET];
} bcache;

int hash(uint dev, uint blockno) {
  return (dev + blockno) % NBUCKET;
}

void update_last_access_time(struct buf *b) {
  acquire(&tickslock);
  b->last_access_time = ticks;
  release(&tickslock);
}

void
binit(void)
{
  char lockname[16] = {0};
  for (int i = 0; i < NBUCKET; i++) {
    snprintf(lockname, sizeof(lockname), "bcache_%d", i);
    // printf("Initializing bucket %d with lock %s\n", i, lockname);
    initlock(&bcache.buckets[i].lock, lockname);

    bcache.buckets[i].head.next = &bcache.buckets[i].head;
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
  }

  for (struct buf *b = bcache.buf; b < bcache.buf+NBUF; b++) {
    // LRU cache: insert from front of list
    b->last_access_time = 0;
    b->prev = bcache.buckets[0].head.prev;
    b->next = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.prev->next = b;
    bcache.buckets[0].head.prev = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  const uint bucket_idx = hash(dev, blockno);
  struct bucket *bucket = &bcache.buckets[bucket_idx];
  acquire(&bucket->lock);

  // Is the block already cached?
  for (struct buf *b = bucket->head.next; b != &bucket->head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      update_last_access_time(b);

      release(&bucket->lock);
      acquiresleep(&b->lock);
      // printf("bget: found %d %d\n", dev, blockno);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  uint min_access_time = ~0;
  struct buf *target_buf;
  uint round = 0;
  for (int i = bucket_idx; round < NBUCKET; i = (i + 1) % NBUCKET, round++) {
    // get the current bucket
    struct bucket *b = &bcache.buckets[i];

    if (i != bucket_idx) {
      if (!holding(&b->lock)) {
        acquire(&b->lock);
      } else {
        continue;
      }
    }

    // find the least recently used buffer
    for (struct buf *e = b->head.next; e != &b->head; e = e->next) {
      if (e->refcnt == 0 && e->last_access_time < min_access_time) {
        min_access_time = e->last_access_time;
        target_buf = e;
      }
    }

    if (target_buf) {
      if (i != bucket_idx) {
        target_buf->prev->next = target_buf->next;
        target_buf->next->prev = target_buf->prev;
        target_buf->prev = bucket->head.prev;
        target_buf->next = &bucket->head;
        bucket->head.prev->next = target_buf;
        bucket->head.prev = target_buf;
        release(&b->lock);
      }

      // allocate targer_buf to the current block
      target_buf->dev = dev;
      target_buf->blockno = blockno;
      target_buf->valid = 0;
      target_buf->refcnt = 1;

      // update last access time of buffer
      update_last_access_time(target_buf);

      release(&bucket->lock);
      acquiresleep(&target_buf->lock);
      // printf("bget: alloc %d %d\n", dev, blockno);
      return target_buf;
    } else {
      if (i != bucket_idx) {
        release(&b->lock);
      }
    }
  }
  release(&bucket->lock);
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

  const uint bucket_idx = hash(b->dev, b->blockno);
  struct bucket *bucket = &bcache.buckets[bucket_idx];

  acquire(&bucket->lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    update_last_access_time(b);
  }
  release(&bucket->lock);
}

void
bpin(struct buf *b) {
  const int bucket_idx = hash(b->dev, b->blockno);
  acquire(&bcache.buckets[bucket_idx].lock);
  b->refcnt++;
  release(&bcache.buckets[bucket_idx].lock);
}

void
bunpin(struct buf *b) {
  const int bucket_idx = hash(b->dev, b->blockno);
  acquire(&bcache.buckets[bucket_idx].lock);
  b->refcnt--;
  release(&bcache.buckets[bucket_idx].lock);
}


