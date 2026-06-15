#include "cpu/exec.h"

make_EHelper(jmp) {
  // the target address is calculated at the decode stage
  decoding.is_jmp = 1;

  print_asm("jmp %x", decoding.jmp_eip);
}

make_EHelper(jcc) {
  // the target address is calculated at the decode stage
  uint8_t subcode = decoding.opcode & 0xf;
  rtl_setcc(&t2, subcode);

#ifdef DEBUG
  /* 打印用于诊断：subcode, rtl_setcc 返回, 各标志位 */
  rtlreg_t zf, cf, of;
  rtl_get_ZF(&zf);
  rtl_get_CF(&cf);
  rtl_get_OF(&of);
  fprintf(stderr, "DBG jcc: subcode=0x%x setcc=%d ZF=%d CF=%d OF=%d jmp_eip=0x%x\n",
          subcode, (int)t2, (int)zf, (int)cf, (int)of, decoding.jmp_eip);
#endif

  decoding.is_jmp = t2;

  print_asm("j%s %x", get_cc_name(subcode), decoding.jmp_eip);
}

make_EHelper(jmp_rm) {
  decoding.jmp_eip = id_dest->val;
  decoding.is_jmp = 1;

  print_asm("jmp *%s", id_dest->str);
}

make_EHelper(call) {
  // the target address is calculated at the decode stage
  //TODO();
  rtl_li(&t2, decoding.seq_eip);
  rtl_push(&t2);
  decoding.is_jmp=1;
  print_asm("call %x", decoding.jmp_eip);
}

make_EHelper(ret) {
  //TODO();
  rtl_pop(&t2);
  decoding.jmp_eip = t2;
  decoding.is_jmp = 1;
  print_asm("ret");
}

make_EHelper(call_rm) {
  //TODO();
  rtl_li(&t2, decoding.seq_eip);
  rtl_push(&t2);
  decoding.jmp_eip = id_dest->val;
  decoding.is_jmp=1;
  print_asm("call *%s", id_dest->str);
}
