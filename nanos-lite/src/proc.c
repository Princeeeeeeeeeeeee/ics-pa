#include "proc.h"

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC];
static int nr_proc = 0;
PCB *current = NULL;

uintptr_t loader(_Protect *as, const char *filename);

void load_prog(const char *filename) {
  int i = nr_proc ++;
  _protect(&pcb[i].as);

  uintptr_t entry = loader(&pcb[i].as, filename);

  _Area stack;
  stack.start = pcb[i].stack;
  stack.end = stack.start + sizeof(pcb[i].stack);

  pcb[i].tf = _umake(&pcb[i].as, stack, stack, (void *)entry, NULL, NULL);
}

/* 当前运行游戏的进程号 (0=仙剑奇侠传, 1=hello, 2=videotest) */
int current_game = 0;

/* 调度计数, 用于优先级调度 */
static int schedule_count = 0;

_RegSet* schedule(_RegSet *prev) {
  /* 保存当前进程的上下文 */
  if (current != NULL) {
    current->tf = prev;
  }

  /* 优先级调度: current_game 运行多次后才让 hello 运行1次 */
  if (schedule_count < 100) {
    schedule_count++;
    current = &pcb[current_game];
  } else {
    schedule_count = 0;
    current = &pcb[1];  // hello 程序
  }

  /* 切换到新进程的虚拟地址空间 */
  _switch(&current->as);

  return current->tf;
}

/* 切换当前运行的游戏 (用于F12按键) */
void switch_current_game() {
  current_game = (current_game == 0) ? 2 : 0;
  Log("switch_current_game: current_game=%d", current_game);
}
