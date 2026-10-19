// Exercise the restored production frame scheduler with only the Android
// JNI/window boundary stubbed. This catches list-cursor and frame integration
// regressions that direct process_particle_motion() host calls cannot expose.
#include "../app/src/main/cpp/arm64/libsand_decompiled.cpp"

#include <cstdint>
#include <cstdio>

static ANativeWindow fake_window;
alignas(256) static uint16_t framebuffer[128 * 128];
static int lock_count = 0;
static int water_index = -1;
static int water_x = -1;
static int water_start_y = -1;

static unsigned char check_exception(__int64) { return 0; }
static __int64 delete_ref(__int64) { return 0; }

void JNIEnv::CallStaticVoidMethod(jclass, jmethodID, ...) {}
ANativeWindow* ANativeWindow_fromSurface(JNIEnv*, jobject) { return &fake_window; }
int32_t ANativeWindow_getHeight(ANativeWindow*) { return 64; }
int32_t ANativeWindow_getWidth(ANativeWindow*) { return 64; }
int32_t ANativeWindow_setBuffersGeometry(ANativeWindow*, int32_t, int32_t, int32_t) {
  return 0;
}
int32_t ANativeWindow_lock(ANativeWindow*, ANativeWindow_Buffer* buffer, ARect*) {
  ++lock_count;
  buffer->width = 64;
  buffer->height = 64;
  buffer->stride = 64;
  buffer->format = 4;
  buffer->bits = framebuffer;

  // The real setup path maps this fake 64x64 surface to a 21x21 simulation
  // grid. Add water safely inside that grid on the first active frame.
  if (lock_count == 2) {
    if (g_view_width[0] <= 4 || g_view_height <= 8) {
      std::fprintf(stderr, "unexpected simulation grid dimensions %d x %d\n",
                   g_view_width[0], g_view_height);
      return -1;
    }
    water_x = g_view_width[0] / 2;
    water_start_y = 5;
    if (water_x <= 1 || water_x >= g_view_width[0] - 2 ||
        water_start_y >= g_view_height - 2) {
      std::fprintf(stderr, "scheduler test particle is outside safe grid bounds\n");
      return -1;
    }
    add_type(static_cast<__int16>(water_x), static_cast<__int16>(water_start_y), 5, 0);
    water_index = *reinterpret_cast<int*>(
        g_cell_state_grid + 8LL * (water_x + water_start_y * g_view_width[0]) + 4);
    if (water_index < 0) {
      std::fprintf(stderr, "add_type did not create the scheduler test particle\n");
      return -1;
    }
  }

  // Let several real main_loop() frames run, then request the engine's normal
  // mode-10 shutdown path so this integration probe remains deterministic.
  if (lock_count == 6)
    g_engine_mode = 10;
  return 0;
}
int32_t ANativeWindow_unlockAndPost(ANativeWindow*) { return 0; }
void ANativeWindow_release(ANativeWindow*) {}

int main() {
  static void* function_table[229]{};
  function_table[228] = reinterpret_cast<void*>(&check_exception);
  function_table[22] = reinterpret_cast<void*>(&delete_ref);
  struct FakeEnv { void** functions; };
  static FakeEnv fake_env{function_table};
  g_jni_env = reinterpret_cast<__int64>(&fake_env);
  g_surface_global_ref = 1;

  main_loop();

  if (lock_count < 6 || water_index < 0) {
    std::fprintf(stderr, "main_loop exited before the scheduler test ran (%d locks)\n",
                 lock_count);
    return 1;
  }
  const int final_x = g_particle_records[10 * water_index];
  const int final_y = g_particle_records[10 * water_index + 1];
  if (final_x != water_x || final_y <= water_start_y) {
    std::fprintf(stderr,
                 "frame scheduler failed to move water: start=(%d,%d), final=(%d,%d)\n",
                 water_x, water_start_y, final_x, final_y);
    return 1;
  }
  std::printf("frame scheduler moved water from (%d,%d) to (%d,%d) in %d locks\n",
              water_x, water_start_y, final_x, final_y, lock_count);
  return 0;
}
