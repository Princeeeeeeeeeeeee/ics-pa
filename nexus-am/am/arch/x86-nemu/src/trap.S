#----|-------entry-------|-errorcode-|---irq id---|---handler---|
.globl vecsys;    vecsys:  pushl $0;  pushl $0x80; jmp asm_trap
.globl vecnull;  vecnull:  pushl $0;  pushl   $-1; jmp asm_trap
.globl vecself;  vecself:  pushl $0;  pushl $0x81; jmp asm_trap
.globl vectime;  vectime:  pushl $0;  pushl  $32;  jmp asm_trap

asm_trap:
  pushal

  pushl %esp
  call irq_handle

  # 上下文切换: 将栈顶指针切换到新进程的陷阱帧
  movl %eax, %esp

  popal
  addl $8, %esp

  iret
