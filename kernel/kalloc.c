// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define PA2PGIDX(p) (((p)-KERNBASE) / PGSIZE)

void freerange(void *pa_start, void *pa_end);

extern char end[];  // first address after kernel.
                    // defined by kernel.ld.

struct run {
    struct run *next;
};

struct {
    struct spinlock lock;
    struct run *freelist;
} kmem;

struct ref_cow {
    struct spinlock lock;
    int cnt[(PGROUNDUP(PHYSTOP) - KERNBASE) / PGSIZE];  // reference count for each page
} page_ref;

void kinit() {
    initlock(&kmem.lock, "kmem");
    initlock(&page_ref.lock, "refcow");
    freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
    char *p;
    p = (char *)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
        kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
    struct run *r;

    if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    acquire(&page_ref.lock);
    page_ref.cnt[PA2PGIDX((uint64)pa)]--;

    if (page_ref.cnt[PA2PGIDX((uint64)pa)] <= 0) {
        // Fill with junk to catch dangling refs.
        memset(pa, 1, PGSIZE);

        r = (struct run *)pa;

        acquire(&kmem.lock);
        r->next = kmem.freelist;
        kmem.freelist = r;
        release(&kmem.lock);
    }

    release(&page_ref.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void) {
    struct run *r;

    acquire(&kmem.lock);
    r = kmem.freelist;
    if (r)
        kmem.freelist = r->next;
    release(&kmem.lock);

    if (r) {
        memset((char *)r, 5, PGSIZE);  // fill with junk
        page_ref.cnt[PA2PGIDX((uint64)r)] = 1;
    }
    return (void *)r;
}

void kpagerefincr(uint64 pa) {
    acquire(&page_ref.lock);
    page_ref.cnt[PA2PGIDX((uint64)pa)]++;
    release(&page_ref.lock);
}

// COW_fork(): alloc new page and copy page table for child
uint64 kderef_and_alloc(uint64 pa) {
    acquire(&page_ref.lock);

    if (page_ref.cnt[PA2PGIDX((uint64)pa)] <= 1) {
        release(&page_ref.lock);
        return pa;
    }

    // alloc memory and copy old pages from parent
    uint64 new_pa = (uint64)kalloc();
    if (new_pa == 0) {  // run out of memory
        release(&page_ref.lock);
        return 0;
    }
    memmove((void *)new_pa, (void *)pa, PGSIZE);

    page_ref.cnt[PA2PGIDX((uint64)pa)]--;
    release(&page_ref.lock);

    return new_pa;
}