#include "common.h"

#define DEFAULT_ENTRY ((void *)0x4000000)

extern uint8_t ramdisk_start;
extern uint8_t ramdisk_end;
#define RAMDISK_SIZE ((&ramdisk_end) - (&ramdisk_start))
extern void ramdisk_read(void *buf, off_t offset, size_t len);

intptr_t loader(_Protect *as, const char *filename) {
  TODO();
  //if (RAMDISK_SIZE) {
  //  ramdisk_read(DEFAULT_ENTRY, 0, RAMDISK_SIZE);
  //}
  return (intptr_t)DEFAULT_ENTRY;
}
