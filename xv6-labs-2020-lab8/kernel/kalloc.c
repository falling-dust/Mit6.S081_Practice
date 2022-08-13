// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  char lock_name[10];
   for(int i = 0;i < NCPU; i++) {
    snprintf(lock_name, sizeof(lock_name), "kmem_%d", i);
    initlock(&kmem[i].lock, lock_name);
  }
  freerange(end, (void*)PHYSTOP);
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

//由当前运行的CPU回收内存
  push_off();  // 关中断
  int id = cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off();  //开中断
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();// 关中断
  int id = cpuid();//获取CPU序号

  acquire(&kmem[id].lock);//上锁A
  r = kmem[id].freelist;//获取当前空闲链表
  if(r)//若空闲链表仍有空闲区域
    kmem[id].freelist = r->next;
  else {//没有空闲，窃取其他CPU
    int newid;  
    // 遍历所有CPU的空闲列表
    for(newid = 0; newid < NCPU; newid++) {
      if(newid == id)//若查到自己，跳过
        continue;
      acquire(&kmem[newid].lock);//上锁B
      r = kmem[newid].freelist;//获取某一CPU的空闲链表
      if(r) {//若有空闲区域
        kmem[newid].freelist = r->next;
        release(&kmem[newid].lock);//解锁B
        break;
      }
      release(&kmem[newid].lock);
    }
  }
  release(&kmem[id].lock);//解锁A
  
  pop_off();  //开中断

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}