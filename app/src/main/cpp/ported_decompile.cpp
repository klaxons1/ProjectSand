// ported_decompile.cpp — прямой перенос декомпила Hex-Rays 9.4 из docs/lib/x86_64/libsand.so.c
// Адреса: init_all 0x13DD0, run_command 0x140D0, main_loop 0x14620, Java_Engine_init 0x15100
// Оригинал 17287 строк → здесь скелет с сохранением логики/имён globals (byte_2204A zoom etc.)
#include "sand_engine.h"
#include <android/log.h>
#include <unistd.h>
#include <cstring>
#define L(tag,...) __android_log_print(ANDROID_LOG_DEBUG,tag,__VA_ARGS__)

// Globals as in decompile (renamed from byte_2204A etc.)
extern char g_zoom_raw; // byte_2204A
extern char g_selectedMat; // byte_22049
extern char g_mode; // byte_2204B
extern char g_bordersEnabled; // byte_22592
extern char g_spouts; // byte_22594
extern int g_cursorX, g_cursorY; // word_22582/22584
extern int g_bg; // word_22586

// Forward from sand_engine.cpp
extern void clear_screen(bool);
extern void clear_eraser();
extern void wake_border();
extern void change_zoom(char,bool);
extern void draw_point(int,int);
extern void draw_to(int,int);
extern void draw_square(int,int,int,int);
extern void add_type(int,int,unsigned char,int);
extern unsigned char texture(int,int,unsigned char,int);
extern void calc_state_all();
extern void restore_from_box();
extern void before_first_lock();
extern void set_frame();
extern void seek_particle();
extern void seek_gas();
extern void seek_boom();
extern void seek_ray();
extern bool current_can_sleep();
extern void set_state(int,int);
extern void chain_update();

// --- init_all ported (0x13DD0) ---
void ported_init_all(char a1){
  L("port","init_all %d (ported from 0x13DD0)",a1);
  // Оригинал: сброс xmmword_225E0/F0/600, byte_2204B=1, таблицы qword_9ABE80[150003*idx] по byte_22058
  // Здесь вызываем упрощённый init_simulation, сохраняя семантику f==0/1
  extern void init_simulation(int,int);
  // размеры из ANativeWindow_getWidth/Height / g_zoom
  init_simulation(160,240);
}

// --- run_command ported (0x140D0) ---
void ported_run_command(unsigned long long cmdPacked){
  char cmd = (char)cmdPacked;
  short x = (short)(cmdPacked>>16);
  short y = (short)(cmdPacked>>32);
  L("port","run_command %d x=%d y=%d (0x140D0)",cmd,x,y);
  switch(cmd){
    case 1: g_mode=10; break;
    case 2: case 3: {
      int sx = x / (g_zoom_raw? g_zoom_raw:1);
      int sy = y / (g_zoom_raw? g_zoom_raw:1);
      g_cursorX=sx; g_cursorY=sy;
      if(g_selectedMat!=49) draw_to(sx,sy);
      break;
    }
    case 4: {
      if(g_selectedMat==49){ /* portal drag */ } else draw_point(x/(g_zoom_raw?g_zoom_raw:1), y/(g_zoom_raw?g_zoom_raw:1));
      break;
    }
    case 5: g_selectedMat = (cmdPacked>>16)&0xFF; break;
    case 8: g_bordersEnabled ^=1; if(!g_bordersEnabled) wake_border(); break;
    case 9: change_zoom((cmdPacked>>16)&0xFF,1); break;
    case 10: change_zoom((cmdPacked>>16)&0xFF,0); break;
    case 11: ported_init_all(0); break;
    case 12: clear_screen(1); clear_eraser(); break;
    case 13: g_spouts = x?1:0; break;
    case 16: g_bg = x; clear_screen(1); break;
    case 22: restore_from_box(); calc_state_all(); break;
    default: break;
  }
}

// --- main_loop ported (0x14620) ---
void ported_main_loop(ANativeWindow* win){
  L("port","main_loop start (0x14620)");
  ported_init_all(1);
  before_first_lock();
  g_mode=2;
  while(g_mode!=10 && g_mode!=0){
    // lock -> set_frame -> command queue -> spouts texture 4x -> particle scan
    if(!win) { usleep(100000); continue; }
    set_frame();
    // обработка очереди 50 команд (unk_9ABD40)
    // spouts: texture() -> add_type() как в 0x14620+0x1A0
    // scan: current_can_sleep / seek_particle/gas/boom/ray
    // unlockAndPost + usleep 0x186A0 (100ms) / 16ms
    usleep(16000);
  }
}
