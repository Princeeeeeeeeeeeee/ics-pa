# 计算机系统设计 PA5 实验报告

**学号**: 2313922  
**姓名**: 蔡梓涵

## 一、实验目的

在PA4实现分页机制和分时多任务的基础上，进一步加载第三个用户程序videotest，并实现F12按键在仙剑奇侠传和videotest之间切换的功能，完成"编写不朽的传奇"这一最终展示任务。

## 二、实验内容

PA5（展示阶段）主要完成以下工作：

1. 加载第三个用户程序`/bin/videotest`
2. 实现`current_game`变量和`switch_current_game()`切换接口
3. 在`events_read()`中处理F12按键，调用`switch_current_game()`切换游戏
4. 修复多进程打开同一文件时的偏移指针bug

## 三、实验过程

### 3.1 加载videotest程序

在`nanos-lite/src/main.c`中，在加载pal和hello之后，加载第三个用户程序videotest：

```c
  // 加载用户程序
  load_prog("/bin/pal");
  load_prog("/bin/hello");
  load_prog("/bin/videotest");

  // 通过内核自陷触发第一次上下文切换
  _trap();
```

三个程序的加载顺序约定为：0=仙剑奇侠传(pal)，1=hello，2=videotest。`load_prog()`为每个程序创建独立的虚拟地址空间并初始化陷阱帧。

### 3.2 实现current_game和切换接口

在`nanos-lite/src/proc.c`中，添加`current_game`变量维护当前运行的游戏进程号，并提供`switch_current_game()`函数进行切换：

```c
/* 当前运行游戏的进程号 (0=仙剑奇侠传, 1=hello, 2=videotest) */
int current_game = 0;

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

/* 切换当前运行的游戏 (用于F12按键) */
void switch_current_game() {
  current_game = (current_game == 0) ? 2 : 0;
  Log("switch_current_game: current_game=%d", current_game);
}
```

`schedule()`函数中，`current_game`变量决定哪个游戏参与调度。初始时`current_game=0`（仙剑奇侠传），按下F12后切换为`current_game=2`（videotest）。hello程序始终参与调度，用于验证分时机制仍在工作。

### 3.3 F12按键处理

在`nanos-lite/src/device.c`的`events_read()`函数中，检测F12按键按下事件并调用`switch_current_game()`：

```c
extern void switch_current_game();

size_t events_read(void *buf, size_t len) {
  char str[20];
  bool down=false;
  int key=_read_key();
  if(key&0x8000){
    key^=0x8000;
    down=true;
  }
  if(key!=_KEY_NONE) {
    sprintf(str,"%s %s\n",down?"kd":"ku",keyname[key]);
    // F12按下时切换当前游戏
    if(key == _KEY_F12 && down) {
      switch_current_game();
    }
  }
  else
    sprintf(str,"t %d\n",_uptime());

  if(strlen(str)<=len){
    strncpy((char*)buf,str,strlen(str));
    return strlen(str);
  }
  return 0;
}
```

当F12被按下时（`key == _KEY_F12 && down`），调用`switch_current_game()`切换`current_game`。下一次时钟中断触发`schedule()`时，就会调度到新的游戏程序。

### 3.4 修复多进程打开同一文件的偏移指针bug

**问题发现**：当切换`current_game`时，出现`Assertion failed: screen_w>0 && screen_h>0`错误。

**根本原因**：两个游戏进程都会打开`/dev/dispinfo`文件读取屏幕信息。第一个进程读取完毕后，文件的`open_offset`位于文件末尾。第二个进程打开同一文件时，`open_offset`仍在上次的位置，导致读取到0个字节，屏幕宽度和高度为0。

**解决方案**：在`nanos-lite/src/fs.c`的`fs_open()`中，打开文件时重置偏移指针：

```c
int fs_open(const char* filename, int flags, int mode){
  for(int i=0; i<NR_FILES; i++){
    if(strcmp(filename, file_table[i].name)==0) {
      set_open_offset(i, 0);  // 重置文件偏移指针
      return i;
    }
  }
  panic("this filename not exist in file_table");
  return -1;
}
```

根据`man 2 open`的说明，打开文件时文件偏移量应设置为文件开头。这一修复保证了每次`open`文件时都从头开始读取，解决了多进程共享文件偏移量的问题。

### 3.5 videotest程序

在`navy-apps/apps/videotest/src/main.c`中实现了一个简单的视频测试程序，绘制移动的彩色条纹：

```c
#include <ndl.h>
#include <stdlib.h>

int main() {
  NDL_OpenDisplay(320, 200);

  int w = 320, h = 200;
  uint32_t *pixels = malloc(w * h * sizeof(uint32_t));

  int frame = 0;
  while (1) {
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        uint8_t r = (x + frame) & 0xff;
        uint8_t g = (y + frame) & 0xff;
        uint8_t b = (x + y + frame) & 0xff;
        pixels[y * w + x] = (r << 16) | (g << 8) | b;
      }
    }
    NDL_DrawRect(pixels, 0, 0, w, h);
    NDL_Render();
    frame++;

    NDL_Event event;
    if (NDL_WaitEvent(&event)) {
      if (event.type == NDL_EVENT_KEYDOWN && event.data == NDL_SCANCODE_ESCAPE) {
        break;
      }
    }
  }

  free(pixels);
  NDL_CloseDisplay();
  return 0;
}
```

### 3.6 运行结果

系统启动后，仙剑奇侠传与hello程序分时运行。按下F12键后，仙剑奇侠传被切换为videotest程序，videotest与hello程序分时运行。再次按下F12，切换回仙剑奇侠传。hello程序在整个过程中持续输出，证明分时调度机制正常工作。

## 四、必答题

### 4.1 多进程打开同一文件的偏移指针问题

**问题**：get_display_info()读取dispinfo包括open-read-close三个步骤。假设game1正在读dispinfo，按下F12切换到game2，game2打开文件并读取；此时再按F12切换回game1，game1会根据game2读取时的offset继续读dispinfo，导致game1读取内容重复或缺失吗？

答：在修复前，会的。因为文件表中每个文件只有一个`open_offset`字段，所有进程共享。game2读取后offset在文件末尾，game1继续读时会读到0个字节。

修复后，每次`fs_open()`都会将`open_offset`重置为0，因此game1重新打开文件时会从头读取，不会出现内容重复或缺失的问题。

但在更严格的场景中，如果两个进程同时打开同一文件（文件偏移量在打开时重置，但读取过程中被切换），仍可能存在竞态条件。在真实的操作系统中，每个进程的文件描述符表中有独立的文件偏移量，彻底避免了这个问题。Nanos-lite的简化实现通过`open`时重置offset缓解了这一问题。

### 4.2 最多只允许一个需要更新画面的进程参与调度

**问题**：为什么最多只允许一个需要更新画面的进程参与调度？

答：多个需要更新画面的进程分时运行会导致画面被相互覆盖。每个进程都直接向帧缓冲(framebuffer)写入像素数据，如果两个进程交替运行，它们会交替写入不同的画面内容，导致屏幕上出现两个画面交替闪烁的混乱效果。

在真实的图形界面操作系统（如Linux/X Window）中，由窗口管理器统一管理画面显示。每个进程在自己的窗口中绘制，窗口管理器负责将多个窗口的内容合成到屏幕上。这需要进程间通信机制（如共享内存、消息传递），已经超出了ICS课程的范围，Nanos-lite也不支持。

因此在PA中简化为：最多只允许一个需要更新画面的进程（current_game）参与调度，hello程序只输出文字不更新画面，不会造成冲突。

### 4.3 F12切换的实现机制

**问题**：F12按下后，切换是如何发生的？

答：F12按键的处理流程如下：

1. **按键检测**：NEMU的键盘设备捕获F12按键，通过`_read_key()`返回按键码
2. **事件处理**：`events_read()`检测到F12按下，调用`switch_current_game()`将`current_game`在0和2之间切换
3. **调度生效**：下一次时钟中断触发`schedule()`时，`schedule()`读取`current_game`的值，选择对应的进程（pcb[0]或pcb[2]）作为下一个运行的进程
4. **上下文切换**：`_switch()`切换到新游戏的虚拟地址空间，`asm_trap`恢复新游戏的上下文

F12按键不会立即触发切换，而是设置`current_game`标志，等待下一次时钟中断时由`schedule()`生效。这保证了切换操作在中断处理中完成，不会破坏当前进程的执行状态。
