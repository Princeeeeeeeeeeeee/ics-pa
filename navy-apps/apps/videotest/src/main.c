#include <ndl.h>
#include <stdlib.h>

int main() {
  /* 打开画布 */
  NDL_OpenDisplay(320, 200);

  /* 分配像素缓冲区 */
  int w = 320, h = 200;
  uint32_t *pixels = malloc(w * h * sizeof(uint32_t));

  /* 绘制彩色条纹动画 */
  int frame = 0;
  while (1) {
    /* 填充像素: 生成移动的彩色条纹 */
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        uint8_t r = (x + frame) & 0xff;
        uint8_t g = (y + frame) & 0xff;
        uint8_t b = (x + y + frame) & 0xff;
        pixels[y * w + x] = (r << 16) | (g << 8) | b;
      }
    }

    /* 绘制到屏幕 */
    NDL_DrawRect(pixels, 0, 0, w, h);
    NDL_Render();

    frame++;

    /* 检查是否有按键事件 */
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
