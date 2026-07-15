#include "common.h"
#include "fs.h"
#include "memory.h"

#define DEFAULT_ENTRY ((void *)0x8048000)

extern uint8_t ramdisk_start;
extern uint8_t ramdisk_end;
#define RAMDISK_SIZE ((&ramdisk_end)-(&ramdisk_start))
extern void ramdisk_read(void *buf, off_t offset, size_t len);

uintptr_t loader(_Protect *as, const char *filename) {
  int fd = fs_open(filename, 0, 0);
  Log("filename=%s, fd=%d", filename, fd);

  size_t filesz = fs_filesz(fd);
  size_t nr_page = (filesz + PGSIZE - 1) / PGSIZE;  // 向上取整

  /* 按页加载用户程序 */
  uintptr_t va = (uintptr_t)DEFAULT_ENTRY;
  for (size_t i = 0; i < nr_page; i++) {
    void *pa = new_page();
    _map(as, (void *)va, pa);
    // 读取一页数据到物理页
    size_t read_len = (i == nr_page - 1) ? (filesz - i * PGSIZE) : PGSIZE;
    fs_read(fd, pa, read_len);
    va += PGSIZE;
  }

  fs_close(fd);
  return (uintptr_t)DEFAULT_ENTRY;
}
