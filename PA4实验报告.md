# 计算机系统设计 PA4 实验报告

**学号**: 2313922  
**姓名**: 蔡梓涵

## 一、实验目的

在PA3实现中断与异常处理的基础上，进一步实现分页机制和分时多任务，最终让仙剑奇侠传和hello程序在计算机系统中分时运行。具体包括：

1. 在NEMU中实现i386分页机制（CR0/CR3寄存器、页表翻译、虚拟地址转换）
2. 实现用户程序的分页加载（按页映射、堆区管理）
3. 实现上下文切换与分时多任务（内核自陷、schedule调度）
4. 实现时钟中断，实现真正的抢占式分时调度

## 二、实验内容

PA4分为三个阶段：

- **阶段1**：分页机制 — 实现CR0/CR3寄存器、MOV CRx指令、page_translate地址翻译、用户程序分页加载、mm_brk堆区管理
- **阶段2**：上下文切换与分时多任务 — 实现内核自陷(_trap)、_umake构造用户进程上下文、schedule进程调度、asm_trap上下文切换
- **阶段3**：时钟中断 — 实现INTR引脚、dev_raise_intr、exec_wrapper中断轮询、时钟中断事件分发

## 三、实验过程

### 3.1 NEMU中添加CR0、CR3寄存器和INTR引脚

在`nemu/include/cpu/reg.h`中，引入mmu.h头文件并在CPU_state结构体中添加CR0、CR3和INTR字段：

```c
#include "memory/mmu.h"

// ... 在CPU_state结构体末尾添加 ...
  rtlreg_t cs;
  CR0 cr0;
  CR3 cr3;
  bool INTR;
} CPU_state;
```

在`nemu/src/monitor/monitor.c`的`restart()`函数中初始化CR0：

```c
static inline void restart() {
  cpu.eip = ENTRY_START;
  cpu.cs = 8;
  unsigned int origin = 2;
  memcpy(&cpu.eflags, &origin, sizeof(cpu.eflags));
  cpu.cr0.val = 0x60000011;
  cpu.INTR = false;
}
```

### 3.2 实现MOV CRx指令

在`nemu/src/cpu/decode/decode.c`中添加`decode_cR`解码函数，解析ModR/M字节中的CRx编号和通用寄存器编号：

```c
make_DHelper(cR) {
  ModR_M m;
  m.val = instr_fetch(eip, 1);
  assert(m.mod == 3);

  /* id_src: CRx */
  id_src->type = OP_TYPE_REG;
  id_src->reg = m.reg;
  switch (m.reg) {
    case 0: id_src->val = cpu.cr0.val; break;
    case 3: id_src->val = cpu.cr3.val; break;
    default: assert(0);
  }

  /* id_dest: 通用寄存器 */
  id_dest->type = OP_TYPE_REG;
  id_dest->reg = m.R_M;
  rtl_lr(&id_dest->val, m.R_M, id_dest->width);

#ifdef DEBUG
  snprintf(id_src->str, OP_STR_SIZE, "%%cr%d", m.reg);
  snprintf(id_dest->str, OP_STR_SIZE, "%%%s", reg_name(m.R_M, id_dest->width));
#endif
}
```

在`nemu/src/cpu/exec/data-mov.c`中实现两个执行函数：

```c
/* MOV CRx, r32 (0x0f 0x22) */
make_EHelper(mov_reg2cr) {
  switch (id_src->reg) {
    case 0: cpu.cr0.val = id_dest->val; break;
    case 3: cpu.cr3.val = id_dest->val; break;
    default: assert(0);
  }
  print_asm_template2(mov);
}

/* MOV r32, CRx (0x0f 0x20) */
make_EHelper(mov_cr2reg) {
  operand_write(id_dest, &id_src->val);
  print_asm_template2(mov);
}
```

在`nemu/src/cpu/exec/exec.c`的2字节操作码表中注册：

```c
  /* 0x20 */	IDEX(cR, mov_cr2reg), EMPTY, IDEX(cR, mov_reg2cr), EMPTY,
```

### 3.3 实现page_translate地址翻译

在`nemu/src/memory/memory.c`中实现分页地址翻译。通过CR3获取页目录基址，依次查找页目录项和页表项，最终拼接物理地址：

```c
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
```

修改`vaddr_read`和`vaddr_write`，在CR0的PG位为1时进行地址翻译，并处理跨页访问：

```c
uint32_t vaddr_read(vaddr_t addr, int len) {
  if (cpu.cr0.paging) {
    if (OFF(addr) + len > PAGE_SIZE) {
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
```

### 3.4 实现时钟中断（NEMU侧）

在`nemu/src/cpu/intr.c`中实现`dev_raise_intr()`设置INTR引脚，并在`raise_intr()`中保存eflags后关中断：

```c
void raise_intr(uint8_t NO, vaddr_t ret_addr) {
  // ... 保存eflags, cs, eip到栈 ...
  // 保存eflags后关中断
  cpu.eflags.IF = 0;
}

void dev_raise_intr() {
  cpu.INTR = true;
}
```

在`nemu/src/cpu/exec/exec.c`的`exec_wrapper()`末尾添加INTR轮询：

```c
  update_eip();

#ifdef DIFF_TEST
  void difftest_step(uint32_t);
  difftest_step(eip);
#endif

  /* 检查硬件中断 */
  if (cpu.INTR && cpu.eflags.IF) {
    cpu.INTR = false;
    raise_intr(32, cpu.eip);
    update_eip();
  }
}
```

### 3.5 AM层实现_map和_umake

在`nexus-am/am/arch/x86-nemu/src/pte.c`中实现`_map()`函数，将虚拟地址映射到物理地址：

```c
void _map(_Protect *p, void *va, void *pa) {
  PDE *pdir = (PDE *)p->ptr;
  uint32_t pdx = PDX(va);
  uint32_t ptx = PTX(va);

  if (!(pdir[pdx] & PTE_P)) {
    void *ptab = palloc_f();
    memset(ptab, 0, PGSIZE);
    pdir[pdx] = (uintptr_t)ptab | PTE_P;
  }

  PTE *ptab = (PTE *)PTE_ADDR(pdir[pdx]);
  ptab[ptx] = (uintptr_t)pa | PTE_P;
}
```

实现`_umake()`函数，在用户栈上初始化陷阱帧：

```c
_RegSet *_umake(_Protect *p, _Area ustack, _Area kstack, void *entry,
                char *const argv[], char *const envp[]) {
  _RegSet *tf = (_RegSet *)ustack.end - 1;

  /* 压入 _start() 函数的栈帧 */
  uintptr_t *sp = (uintptr_t *)tf;
  *(--sp) = 0;  // envp
  *(--sp) = 0;  // argv
  *(--sp) = 0;  // argc
  *(--sp) = 0;  // return address

  /* 初始化陷阱帧 */
  tf->edi = 0; tf->esi = 0; tf->ebp = 0;
  tf->esp = (uintptr_t)sp;
  tf->ebx = 0; tf->edx = 0; tf->ecx = 0; tf->eax = 0;
  tf->irq = 0;
  tf->error_code = 0;
  tf->eip = (uintptr_t)entry;
  tf->cs = 8;
  tf->eflags = 2 | FL_IF;  // 开中断

  return tf;
}
```

### 3.6 AM层实现_trap和时钟中断入口

在`nexus-am/am/arch/x86-nemu/src/asye.c`中实现`_trap()`并扩展`irq_handle()`：

```c
void _trap() {
  asm volatile("int $0x81");
}

int _istatus(int enable) {
  if (enable) {
    asm volatile("sti");
  } else {
    asm volatile("cli");
  }
  return 0;
}
```

扩展`irq_handle()`处理多种中断事件：

```c
_RegSet* irq_handle(_RegSet *tf) {
  _RegSet *next = tf;
  if (H) {
    _Event ev;
    switch (tf->irq) {
      case 0x80: ev.event = _EVENT_SYSCALL; break;
      case 0x81: ev.event = _EVENT_TRAP; break;
      case 32:   ev.event = _EVENT_IRQ_TIME; break;
      case -1:   ev.event = _EVENT_ERROR; break;
      default:   ev.event = _EVENT_ERROR; break;
    }
    next = H(ev, tf);
    if (next == NULL) {
      next = tf;
    }
  }
  return next;
}
```

在`_asye_init()`中注册vecself(0x81)和vectime(32)门描述符：

```c
  idt[0x81] = GATE(STS_TG32, KSEL(SEG_KCODE), vecself, DPL_KERN);
  idt[32] = GATE(STS_TG32, KSEL(SEG_KCODE), vectime, DPL_KERN);
```

### 3.7 修改trap.S实现上下文切换

在`nexus-am/am/arch/x86-nemu/src/trap.S`中添加vecself和vectime入口，并修改asm_trap实现上下文切换：

```asm
#----|-------entry-------|-errorcode-|---irq id---|---handler---|
.globl vecsys;    vecsys:  pushl $0;  pushl $0x80; jmp asm_trap
.globl vecnull;  vecnull:  pushl $0;  pushl   $-1; jmp asm_trap
.globl vecself;  vecself:  pushl $0;  pushl $0x81; jmp asm_trap
.globl vectime;  vectime:  pushl $0;  pushl  $32;  jmp asm_trap

asm_trap:
  pushal
  pushl %esp
  call irq_handle
  # 上下文切换: 将栈顶切换到新进程的陷阱帧
  movl %eax, %esp
  popal
  addl $8, %esp
  iret
```

### 3.8 Nanos-lite启用分页与程序加载

在`nanos-lite/src/main.c`中启用HAS_PTE并改用load_prog加载用户程序：

```c
#define HAS_PTE

int main() {
#ifdef HAS_PTE
  init_mm();
#endif
  // ... 初始化 ...
  load_prog("/bin/pal");
  load_prog("/bin/hello");
  _trap();  // 内核自陷触发第一次上下文切换
  panic("Should not reach here");
}
```

### 3.9 实现分页加载器

在`nanos-lite/src/loader.c`中修改DEFAULT_ENTRY并实现按页加载：

```c
#define DEFAULT_ENTRY ((void *)0x8048000)

uintptr_t loader(_Protect *as, const char *filename) {
  int fd = fs_open(filename, 0, 0);
  size_t filesz = fs_filesz(fd);
  size_t nr_page = (filesz + PGSIZE - 1) / PGSIZE;

  uintptr_t va = (uintptr_t)DEFAULT_ENTRY;
  for (size_t i = 0; i < nr_page; i++) {
    void *pa = new_page();
    _map(as, (void *)va, pa);
    size_t read_len = (i == nr_page - 1) ? (filesz - i * PGSIZE) : PGSIZE;
    fs_read(fd, pa, read_len);
    va += PGSIZE;
  }

  fs_close(fd);
  return (uintptr_t)DEFAULT_ENTRY;
}
```

### 3.10 实现mm_brk堆区管理

在`nanos-lite/src/mm.c`中实现`mm_brk()`，只在需要新增映射时调用_map：

```c
int mm_brk(uint32_t new_brk) {
  if (current->cur_brk == 0) {
    current->cur_brk = current->max_brk = new_brk;
  } else {
    if (new_brk > current->max_brk) {
      uint32_t first = PGROUNDUP(current->max_brk);
      uint32_t end = PGROUNDDOWN(new_brk);
      if ((new_brk & 0xfff) == 0) {
        end -= PGSIZE;
      }
      for (uint32_t va = first; va <= end; va += PGSIZE) {
        void *pa = new_page();
        _map(&(current->as), (void *)va, pa);
      }
      current->max_brk = new_brk;
    }
    current->cur_brk = new_brk;
  }
  return 0;
}
```

### 3.11 实现schedule进程调度

在`nanos-lite/src/proc.c`中实现优先级调度：

```c
int current_game = 0;
static int schedule_count = 0;

_RegSet* schedule(_RegSet *prev) {
  if (current != NULL) {
    current->tf = prev;
  }

  /* 优先级调度: current_game运行100次后hello运行1次 */
  if (schedule_count < 100) {
    schedule_count++;
    current = &pcb[current_game];
  } else {
    schedule_count = 0;
    current = &pcb[1];  // hello程序
  }

  _switch(&current->as);
  return current->tf;
}
```

### 3.12 实现事件分发

在`nanos-lite/src/irq.c`中处理三种事件：

```c
static _RegSet* do_event(_Event e, _RegSet* r) {
  switch (e.event) {
    case _EVENT_SYSCALL:
      do_syscall(r);
      return schedule(r);
    case _EVENT_TRAP:
      return schedule(r);
    case _EVENT_IRQ_TIME:
      return schedule(r);
    default: panic("Unhandled event ID = %d", e.event);
  }
  return NULL;
}
```

### 3.13 修改链接地址

在`navy-apps/Makefile.compile`中将用户程序链接地址从0x4000000改为0x8048000：

```makefile
LDFLAGS += -Ttext 0x8048000
```

## 四、必答题

### 4.1 i386分页机制思考题

**问题1：i386不是一个32位的处理器吗，为什么表项中的基地址信息只有20位，而不是32位？**

答：因为页面大小为4KB（2^12），页面首地址的低12位始终为0，所以只需要20位即可表示页面基地址。32位虚拟地址空间 = 2^32 = 4GB，分页后共2^20 = 1M个页面，20位正好可以索引所有页面。

**问题2：手册上提到表项(包括CR3)中的基地址都是物理地址，物理地址是必须的？能否使用虚拟地址？**

答：必须使用物理地址。如果使用虚拟地址，则需要再次通过页表翻译才能得到物理地址，而翻译过程又需要访问页表，形成无限递归，无法终止。硬件MMU直接通过物理地址访问内存中的页表，不经过地址翻译。

**问题3：为什么不采用一级页表？或者说采用一级页表会有什么缺点？**

答：一级页表有两个主要缺点：
1. 内存消耗大：32位地址空间需要1M个页表项，每项4字节，共4MB。即使进程只使用少量内存，也需要完整的4MB页表。
2. 需要连续内存空间：4MB的页表必须存放在连续的物理内存中，这在内存碎片化后难以分配。

二级页表只为进程实际使用的内存区域分配页表项，且可以在内存中离散存储，大大节省内存。

### 4.2 空指针真的是"空"的吗？

**问题：空指针真的是"空"的吗？当程序对空指针解引用时，计算机内部具体都做了些什么？**

答：空指针NULL的值是0，它并不是真正的"空"，而是一个特殊的指针值，表示不引用任何有效对象。当程序对空指针解引用时，计算机会尝试访问虚拟地址0附近的内存。在分页机制下，虚拟地址0所在的页通常未被映射（页目录项或页表项的present位为0），因此page_translate函数中的`assert(pde.present)`或`assert(pte.present)`会失败，触发段错误（page fault），程序异常终止。这就是为什么访问空指针会导致程序崩溃的原因。

### 4.3 内核映射的作用

**问题：在_protect()函数中注释掉拷贝内核映射的代码后，为什么会出现缺页错误？**

答：`_switch()`函数切换到用户进程的虚拟地址空间后，所有虚拟地址都根据用户进程的页目录表来翻译。如果注释掉了拷贝内核映射的代码，进程的页目录表中就没有内核空间的映射。当内核代码执行时（例如中断处理过程中访问内核数据），虚拟地址无法翻译，导致缺页错误。

内核页表内容为所有进程共享：每个进程的页表中既包含用户态地址映射，也包含内核态地址映射。内核态地址映射对所有进程都是相同的，这部分内容来源于内核页表的一个拷贝。这保证了无论当前运行哪个进程，内核代码都能正确访问内核空间。

### 4.4 asm_trap修改对系统调用的影响

**问题：将asm_trap中的`addl $4, %esp`改为`movl %eax, %esp`后，这一操作会对系统调用（int 0x80）产生影响吗？**

答：不会产生影响。对于系统调用，`irq_handle()`将原陷阱帧位置（即`esp+4`，因为push了`%esp`参数）作为返回值存放在`eax`寄存器中。`asm_trap`执行`movl %eax, %esp`后，栈顶指向的就是原来的陷阱帧位置，因此仍然恢复的是系统调用前的现场，效果与原来的`addl $4, %esp`相同。

对于上下文切换的情况，`schedule()`返回新进程的陷阱帧地址，`irq_handle()`将其存入`eax`，`asm_trap`通过`movl %eax, %esp`切换到新进程的栈，实现上下文切换。

### 4.5 中断嵌套固定位置保存现场的灾难性后果

**问题：假设硬件把中断信息固定保存在内存地址0x1000的位置，如果发生了中断嵌套，将会发生什么样的灾难性后果？**

答：如果中断信息固定保存在0x1000位置，当中断嵌套发生时，第二次中断会覆盖第一次中断保存在0x1000处的现场信息。当内层中断处理完毕执行iret返回时，它会使用0x1000处被覆盖的信息来恢复现场，导致恢复的是错误的上下文。具体表现为：

1. EIP被覆盖：返回到错误的地址执行代码，可能导致执行非法指令或进入死循环。
2. ESP被覆盖：栈指针错误，后续的栈操作会破坏内存。
3. EFLAGS被覆盖：标志位错误，影响条件判断和中断使能状态。

最终的表现通常是程序崩溃、系统死机或不可预测的行为。这就是为什么中断现场必须保存在栈上——栈的LIFO特性天然支持嵌套，每次中断使用不同的栈位置，不会互相覆盖。

### 4.6 分页机制和硬件中断如何支撑分时运行

**问题：请结合代码，解释分页机制和硬件中断是如何支撑仙剑奇侠传和hello程序在计算机系统中分时运行的。**

答：分时多任务的实现依赖于分页机制和硬件中断两个方面：

**(1) 分页机制保证不同进程拥有独立的存储空间**

分页机制由Nanos-lite、AM和NEMU配合实现：
- NEMU提供CR0与CR3寄存器：CR0用于开启分页（PG位），CR3记录页目录表基地址
- MMU进行地址翻译，在代码中体现为`vaddr_read()`和`vaddr_write()`中的`page_translate()`调用
- Nanos-lite通过`init_mm()`初始化存储管理器，调用AM的`_pte_init()`准备内核页表
- 用户程序加载时，`load_prog()`通过`_protect()`创建独立虚拟地址空间，`loader()`按页分配物理页并建立映射

两个用户程序的虚拟地址都从0x8048000开始，但通过分页机制映射到不同的物理页，实现了存储空间隔离。`_switch()`函数通过设置CR3寄存器切换页目录，实现地址空间的切换。

**(2) 硬件中断保证调度时机不被程序控制**

- NEMU的`exec_wrapper()`每执行完一条指令，便检查`cpu.INTR`和`cpu.eflags.IF`，若都满足则调用`raise_intr(32)`触发时钟中断
- 时钟中断通过`vectime`入口进入`asm_trap`，被`irq_handle()`封装为`_EVENT_IRQ_TIME`事件
- Nanos-lite收到该事件后调用`schedule()`进行进程调度
- `schedule()`通过`_switch()`切换进程的虚拟地址空间，返回新进程的上下文
- `asm_trap()`通过`movl %eax, %esp`切换到新进程的栈，恢复其现场
- NEMU执行下一条指令时，便开始新进程的运行

时钟中断是硬件机制，不受程序控制。即使程序陷入死循环，时钟中断仍能触发调度，保证系统不会被单个程序永久占据。这正是抢占式多任务的核心：操作系统通过硬件中断夺回控制权，实现公平的处理器分时共享。
