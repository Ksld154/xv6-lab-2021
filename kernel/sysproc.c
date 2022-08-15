#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void) {
    int n;
    if (argint(0, &n) < 0)
        return -1;
    exit(n);
    return 0;  // not reached
}

uint64
sys_getpid(void) {
    return myproc()->pid;
}

uint64
sys_fork(void) {
    return fork();
}

uint64
sys_wait(void) {
    uint64 p;
    if (argaddr(0, &p) < 0)
        return -1;
    return wait(p);
}

uint64
sys_sbrk(void) {
    int addr;
    int n;

    if (argint(0, &n) < 0)
        return -1;

    addr = myproc()->sz;
    if (growproc(n) < 0)
        return -1;
    return addr;
}

uint64
sys_sleep(void) {
    int n;
    uint ticks0;

    if (argint(0, &n) < 0)
        return -1;
    acquire(&tickslock);
    ticks0 = ticks;
    while (ticks - ticks0 < n) {
        if (myproc()->killed) {
            release(&tickslock);
            return -1;
        }
        sleep(&ticks, &tickslock);
    }
    release(&tickslock);
    return 0;
}

#ifdef LAB_PGTBL
int sys_pgaccess(void) {
    uint64 va;
    int n_pages;
    uint64 result_addr;

    if (argaddr(0, &va) < 0)
        return -1;
    if (argint(1, &n_pages) < 0)
        return -1;
    if (argaddr(2, &result_addr) < 0)
        return -1;

    if (n_pages > 64) return -1;
    uint64 result_buf = 0;

    vmprint(myproc()->pagetable);
    // [TODO] scan which PTE has been accessed in page table
    for (int i = 0; i < n_pages; i++) {
        uint64 cur_va = va + (i << PGSHIFT);
        // uint64 cur_va = va + i * PGSIZE;

        pte_t* cur_pte;
        uint64 mask = 1 << 6;

        cur_pte = walk(myproc()->pagetable, cur_va, 0);
        // printf("pte: %p\n", *cur_pte);
        // printf("msk: %p\n", mask);

        // printf("%p\n", (*cur_pte) & PTE_A);

        if ((*cur_pte) & PTE_A) {
            result_buf = result_buf | (1 << i);
            *cur_pte = (*cur_pte) ^ mask;
            // printf("new pte: %x\n", *cur_pte);
        }
        // printf("bitmap: %p\n", result_buf);
    }

    copyout(myproc()->pagetable, result_addr, (char*)&result_buf, sizeof(result_buf));

    return 0;
}
#endif

uint64
sys_kill(void) {
    int pid;

    if (argint(0, &pid) < 0)
        return -1;
    return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void) {
    uint xticks;

    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);
    return xticks;
}
