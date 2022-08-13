#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //
  uint32 tail;//定义发送尾指针
  struct tx_desc *desc;

  acquire(&e1000_lock);
  tail = regs[E1000_TDT];//读取发送尾指针对应的寄存器，获取可以写入的位置
  desc = &tx_ring[tail];//tx_ring是网卡发送队列
  // 检查环是否发生溢出
  if ((desc->status & E1000_TXD_STAT_DD) == 0) {
    release(&e1000_lock);
    return -1;
  }

  // 检查尾指针对应缓冲区是否被释放，若未释放，则释放其mbuf
  if (tx_mbufs[tail]) {
    mbuffree(tx_mbufs[tail]);
  }
  //填写描述符
  desc->addr = (uint64) m->head;//addr字段指向数据的缓冲区的头部
  desc->length = m->len;
  desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  tx_mbufs[tail] = m;//将数据帧缓冲区 m 记录到缓冲区队列 mbuf 中用于之后的释放

  // 设置屏障防止指令重新排序
  __sync_synchronize();

  regs[E1000_TDT] = (tail + 1) % TX_RING_SIZE;
  release(&e1000_lock);
  
  return 0;
}

static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
  //获取软件可以被读取的第一个接收队列的索引
  int tail = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
  struct rx_desc *desc = &rx_ring[tail];

  while ((desc->status & E1000_RXD_STAT_DD)) {
    if(desc->length > MBUF_SIZE) {
      panic("e1000 len");
    }
    // 更新描述符中的长度 
    rx_mbufs[tail]->len = desc->length;
    // 将mbuf传送到网络堆栈，用于后续解封装
    net_rx(rx_mbufs[tail]);     

    // 分配一个新的mbuf，替代发送给网络栈的缓冲区,
    rx_mbufs[tail] = mbufalloc(0);
    if (!rx_mbufs[tail]) {
      panic("e1000 no mubfs");
    }
    desc->addr = (uint64) rx_mbufs[tail]->head;
    desc->status = 0;
    
    //更新接收尾指针
    tail = (tail + 1) % RX_RING_SIZE;
    desc = &rx_ring[tail];
  }
  //尾指针需要指向最后一个已被软件处理的描述符, 是终止上述循环时的描述符的前一个
  regs[E1000_RDT] = (tail - 1) % RX_RING_SIZE;
}


void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}