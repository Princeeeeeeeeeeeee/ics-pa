#include "common.h"

extern void switch_current_game();

#define NAME(key) \
  [_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [_KEY_NONE] = "NONE",
  _KEYS(NAME)
};

size_t events_read(void *buf, size_t len) {
  //Log("enter events_read,len=%d",len);
  char str[20];
  bool down=false;
  int key=_read_key();
  if(key&0x8000){
    key^=0x8000;//获得该通码表示的按键位置
    down=true;
  }
  if(key!=_KEY_NONE) {
    sprintf(str,"%s %s\n",down?"kd":"ku",keyname[key]);//按键事件
    // F12按下时切换当前游戏
    if(key == _KEY_F12 && down) {
      switch_current_game();
    }
  }
  else
    sprintf(str,"t %d\n",_uptime());//时钟事件
    //Log("%s",str);

  if(strlen(str)<=len){
    strncpy((char*)buf,str,strlen(str));
    return strlen(str);
  }
  Log("strlen(event)>len,return 0");
  return 0;
}

static char dispinfo[128] __attribute__((used));

void dispinfo_read(void *buf, off_t offset, size_t len) {
  strncpy(buf, dispinfo+offset, len);
}

extern void getScreen(int* p_width, int* p_height);
void fb_write(const void *buf, off_t offset, size_t len) {
  assert(offset%4==0 && len%4==0);
  int index,screen_x,screen_y;
  int width=0,height=0;
  getScreen(&width, &height);
  /*
  index=offset/4;
  screen_y1=index/width;
  screen_x1=index%width;
  index=(offset+len)/4;
  screen_y2=index/width;

  assert(screen_y2>=screen_y1);
  if(screen_y2==screen_y1){
    _draw_rect(buf, screen_x1, screen_y1, len/4, 1);
    return;
  }
  int tempw=width-screen_x1;
  if(screen_y2-screen_y1==1){
    _draw_rect(buf, screen_x1, screen_y1, tempw, 1);
    _draw_rect(buf+tempw*4, 0, screen_y2, len/4-tempw, 1);
    return;
  }
  _draw_rect(buf, screen_x1, screen_y1, tempw, 1);
  int tempy=screen_y2-screen_y1-1;
  _draw_rect(buf+tempw*4, 0, screen_y1+1, width, tempy);
  _draw_rect(buf+tempw*4+tempy*width*4, 0, screen_y2, len/4-tempw-tempy*width, 1);
  */
  for(int i=0; i<len/4; i++){
    index=offset/4+i;
    screen_x=index%width;
    screen_y=index/width;
    _draw_rect(buf+i*4, screen_x, screen_y, 1, 1);
  }
}

void init_device() {
  _ioe_init();

  // TODO: print the string to array `dispinfo` with the format
  // described in the Navy-apps convention
  int width=0, height=0;
  getScreen(&width, &height);
  sprintf(dispinfo, "WIDTH: %d\nHEIGHT: %d\n", width, height);
}
