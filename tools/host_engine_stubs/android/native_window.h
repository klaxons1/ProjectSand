#pragma once
#include <cstdint>
struct ANativeWindow {};
struct ARect {};
struct ANativeWindow_Buffer {
  int32_t width, height, stride, format;
  void* bits;
  uint32_t reserved[6];
};
int32_t ANativeWindow_getHeight(ANativeWindow*);
int32_t ANativeWindow_getWidth(ANativeWindow*);
int32_t ANativeWindow_lock(ANativeWindow*, ANativeWindow_Buffer*, ARect*);
int32_t ANativeWindow_unlockAndPost(ANativeWindow*);
int32_t ANativeWindow_setBuffersGeometry(ANativeWindow*, int32_t, int32_t, int32_t);
void ANativeWindow_release(ANativeWindow*);
