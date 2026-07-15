#include "nemu.h"
#include "device/mmio.h"
#include "memory/mmu.h"

#define PMEM_SIZE (128 * 1024 * 1024)

#define pmem_rw(addr, type) *(type *)({\
    Assert(addr < PMEM_SIZE, "physical address(0x%08x) is out of bound", addr); \
    guest_to_host(addr); \
    })

uint8_t pmem[PMEM_SIZE];

/* Memory accessing interfaces */

uint32_t paddr_read(paddr_t addr, int len) {
  int r=is_mmio(addr);
  if(r==-1)
    return pmem_rw(addr, uint32_t) & (~0u >> ((4 - len) << 3));
  else
    return mmio_read(addr, len, r);
}

void paddr_write(paddr_t addr, int len, uint32_t data) {
  int r=is_mmio(addr);
  if(r==-1)
    memcpy(guest_to_host(addr), &data, len);
  else
    mmio_write(addr, len, data, r);
}

/* PDX: 页目录索引, PTX: 页表索引, OFF: 页内偏移 */
#define PDX(va) (((uint32_t)(va) >> 22) & 0x3ff)
#define PTX(va) (((uint32_t)(va) >> 12) & 0x3ff)
#define OFF(va) ((uint32_t)(va) & 0xfff)
#define PTE_ADDR(pte) ((uint32_t)(pte) & ~0xfff)

paddr_t page_translate(vaddr_t addr, bool iswrite) {
  PDE pde;
  pde.val = paddr_read(cpu.cr3.page_directory_base * PAGE_SIZE + PDX(addr) * 4, 4);
  assert(pde.present);
  pde.accessed = 1;
  paddr_write(cpu.cr3.page_directory_base * PAGE_SIZE + PDX(addr) * 4, 4, pde.val);

  PTE pte;
  pte.val = paddr_read(pde.page_frame * PAGE_SIZE + PTX(addr) * 4, 4);
  assert(pte.present);
  pte.accessed = 1;
  if (iswrite) pte.dirty = 1;
  paddr_write(pde.page_frame * PAGE_SIZE + PTX(addr) * 4, 4, pte.val);

  return pte.page_frame * PAGE_SIZE + OFF(addr);
}

uint32_t vaddr_read(vaddr_t addr, int len) {
  if (cpu.cr0.paging) {
    /* 检查是否跨页 */
    if (OFF(addr) + len > PAGE_SIZE) {
      /* 跨页读取: 分两次读取并拼接 */
      int len1 = PAGE_SIZE - OFF(addr);
      int len2 = len - len1;
      paddr_t paddr1 = page_translate(addr, false);
      paddr_t paddr2 = page_translate(addr + len1, false);
      uint32_t low = paddr_read(paddr1, len1);
      uint32_t high = paddr_read(paddr2, len2);
      return low | (high << (len1 * 8));
    }
    paddr_t paddr = page_translate(addr, false);
    return paddr_read(paddr, len);
  }
  return paddr_read(addr, len);
}

void vaddr_write(vaddr_t addr, int len, uint32_t data) {
  if (cpu.cr0.paging) {
    /* 检查是否跨页 */
    if (OFF(addr) + len > PAGE_SIZE) {
      /* 跨页写入: 拆分数据分别写入 */
      int len1 = PAGE_SIZE - OFF(addr);
      int len2 = len - len1;
      paddr_t paddr1 = page_translate(addr, true);
      paddr_t paddr2 = page_translate(addr + len1, true);
      paddr_write(paddr1, len1, data & ((1 << (len1 * 8)) - 1));
      paddr_write(paddr2, len2, data >> (len1 * 8));
      return;
    }
    paddr_t paddr = page_translate(addr, true);
    paddr_write(paddr, len, data);
    return;
  }
  paddr_write(addr, len, data);
}
