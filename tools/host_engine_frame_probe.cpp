// Exercise the restored production frame scheduler with only the Android
// JNI/window boundary stubbed. This catches list-cursor and frame integration
// regressions that direct process_particle_motion() host calls cannot expose.
#include "../app/src/main/cpp/arm64/libsand_decompiled.cpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

static ANativeWindow fake_window;
alignas(256) static uint16_t framebuffer[128 * 128];
static int lock_count = 0;
static constexpr int scene_lock_limit = 96;
static int water_index = -1;
static int rocket_index = -1;
static int explosion_index = -1;
static int explosion_x = -1;
static int explosion_y = -1;
static bool explosion_motion_dispatched = false;
static int rocket_x = -1;
static int rocket_y = -1;
static bool rocket_motion_dispatched = false;
static int scene_particle_pool_end = 0;
static int scene_fireworks_at_end = 0;
static int scene_gunpowder_at_end = 0;
static int scene_flames_at_end = 0;
static int scene_rockets_at_end = 0;
static int scene_rocket_peak = 0;
static int scene_detonation_rocket_peak = 0;
static std::uint64_t scene_seed = 0x123456789abcdef0ULL;
static std::uint64_t previous_frame_hash = 0;
static int rendered_frame_changes = 0;
static bool have_frame_hash = false;
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

    // Route a real internal rocket through the frame scheduler into its
    // homing/near_point branch, rather than relying only on direct calls.
    rocket_x = g_view_width[0] / 2;
    rocket_y = g_view_height / 2;
    add_type(static_cast<__int16>(rocket_x), static_cast<__int16>(rocket_y), 64, 1);
    add_type(static_cast<__int16>(rocket_x + 1), static_cast<__int16>(rocket_y), 14, 1);
    rocket_index = *reinterpret_cast<int*>(
        g_cell_state_grid + 8LL * (rocket_x + rocket_y * g_view_width[0]) + 4);
    if (rocket_index < 0 || particle_material_id(rocket_index) != 64) {
      std::fprintf(stderr, "add_type did not create the scheduler firework rocket\n");
      return -1;
    }
    particle_record_at(rocket_index).packed_state_word =
        (particle_record_at(rocket_index).packed_state_word & ~7) | 2;
    particle_counter(rocket_index) = 0;

    // Recreate the reported interaction through the real tool path: ignite a
    // fire tile, then place gunpowder and firework powder beside it.
    g_random_state = scene_seed;
    constexpr int fire_x = 14;
    constexpr int fire_y = 14;
    add_grouptype(fire_x, fire_y, 38, 1);
    const unsigned char fire_material = *reinterpret_cast<unsigned char *>(
        g_cell_state_grid + 8LL * (fire_x + fire_y * g_view_width[0]));
    if (static_cast<unsigned char>(g_shared_engine_data[12 * fire_material + 20]) != 5) {
      std::fprintf(stderr, "fire tool did not create an active flame material\n");
      return -1;
    }
    add_type(fire_x - 1, fire_y, 16, 1);
    add_type(fire_x + 1, fire_y, 16, 1);
    add_type(fire_x, fire_y + 1, 16, 1);
    add_grouptype(fire_x, fire_y - 1, 23, 1);
    add_grouptype(fire_x - 1, fire_y - 1, 22, 1);
    add_grouptype(fire_x + 1, fire_y - 1, 22, 1);
    add_grouptype(fire_x, fire_y - 2, 23, 1);

    const unsigned char gunpowder = *reinterpret_cast<unsigned char *>(
        g_cell_state_grid + 8LL * (fire_x + 1 + (fire_y - 1) * g_view_width[0]));
    if (gunpowder < 74 || gunpowder > 75) {
      std::fprintf(stderr, "gunpowder tool did not create material 74/75\n");
      return -1;
    }
    explosion_x = fire_x + 1;
    explosion_y = fire_y - 1;
    g_current_particle_index = *reinterpret_cast<int *>(
        g_cell_state_grid + 8LL * (explosion_x + explosion_y * g_view_width[0]) + 4);
    add_grouptype(fire_x + 1, fire_y - 1, 38, 1);
    explosion_index = *reinterpret_cast<int *>(
        g_cell_state_grid + 8LL * (fire_x + 1 + (fire_y - 1) * g_view_width[0]) + 4);
    if (explosion_index < 0 || particle_material_id(explosion_index) != 66) {
      std::fprintf(stderr, "fire contact did not ignite sprinkled gunpowder into rocket 66\n");
      return -1;
    }
    particle_record_at(explosion_index).packed_state_word =
        (particle_record_at(explosion_index).packed_state_word & ~7) | 6;
    particle_counter(explosion_index) = 0;
  }

  // Run the real frame scheduler long enough for fire, gunpowder, and firework
  // transitions to interact, then request the normal mode-10 shutdown path.
  if (lock_count >= 3 && rocket_index >= 0 && explosion_index >= 0) {
    const unsigned char rocket_cell_material = *reinterpret_cast<unsigned char *>(
        g_cell_state_grid + 8LL * (rocket_x + rocket_y * g_view_width[0]));
    const unsigned char rocket_state = particle_packed_state(rocket_index);
    rocket_motion_dispatched |= rocket_cell_material == 0 || (rocket_state & 7) != 2;
    const unsigned char explosion_cell_material = *reinterpret_cast<unsigned char *>(
        g_cell_state_grid + 8LL * (explosion_x + explosion_y * g_view_width[0]));
    const unsigned char explosion_state = particle_packed_state(explosion_index);
    explosion_motion_dispatched |= explosion_cell_material != 66 ||
        (explosion_state & 7) != 6 || ((explosion_state >> 3) & 7) == 4;
  }
  if (g_cell_state_grid && g_view_width[0] > 0) {
    int rockets_this_frame = 0;
    int detonation_rockets_this_frame = 0;
    for (int y = 0; y < g_view_height; ++y) {
      for (int x = 0; x < g_view_width[0]; ++x) {
        const unsigned char material = *reinterpret_cast<unsigned char *>(
            g_cell_state_grid + 8LL * (x + y * g_view_width[0]));
        rockets_this_frame += material >= 64 && material <= 67;
        detonation_rockets_this_frame += material == 66;
      }
    }
    if (rockets_this_frame > scene_rocket_peak)
      scene_rocket_peak = rockets_this_frame;
    if (detonation_rockets_this_frame > scene_detonation_rocket_peak)
      scene_detonation_rocket_peak = detonation_rockets_this_frame;
  }

  if (lock_count == scene_lock_limit) {
    scene_particle_pool_end = g_next_particle_index;
    for (int y = 0; y < g_view_height; ++y) {
      for (int x = 0; x < g_view_width[0]; ++x) {
        const unsigned char material = *reinterpret_cast<unsigned char *>(
            g_cell_state_grid + 8LL * (x + y * g_view_width[0]));
        scene_fireworks_at_end += material >= 38 && material <= 40;
        scene_gunpowder_at_end += material >= 74 && material <= 75;
        scene_flames_at_end += material >= 59 && material <= 61;
        scene_rockets_at_end += material >= 64 && material <= 67;
      }
    }
    g_engine_mode = 10;
  }
  return 0;
}
int32_t ANativeWindow_unlockAndPost(ANativeWindow*) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (int i = 0; i < 64 * 64; ++i) {
    hash ^= framebuffer[i];
    hash *= 1099511628211ULL;
  }
  if (have_frame_hash && hash != previous_frame_hash)
    ++rendered_frame_changes;
  previous_frame_hash = hash;
  have_frame_hash = true;
  return 0;
}
void ANativeWindow_release(ANativeWindow*) {}

int main(int argc, char **argv) {
  if (argc > 1) {
    char *end = nullptr;
    const unsigned long long parsed_seed = std::strtoull(argv[1], &end, 0);
    if (end == argv[1] || *end != '\0') {
      std::fprintf(stderr, "invalid firework test seed: %s\n", argv[1]);
      return 2;
    }
    scene_seed = parsed_seed;
  }
  static void* function_table[229]{};
  function_table[228] = reinterpret_cast<void*>(&check_exception);
  function_table[22] = reinterpret_cast<void*>(&delete_ref);
  struct FakeEnv { void** functions; };
  static FakeEnv fake_env{function_table};
  g_jni_env = reinterpret_cast<__int64>(&fake_env);
  g_surface_global_ref = 1;

  main_loop();

  if (lock_count < scene_lock_limit || water_index < 0 || rocket_index < 0) {
    std::fprintf(stderr, "main_loop exited before the scheduler test ran (%d locks)\n",
                 lock_count);
    return 1;
  }
  if (!rocket_motion_dispatched || !explosion_motion_dispatched) {
    std::fprintf(stderr, "frame scheduler did not dispatch both rocket paths\n");
    return 1;
  }
  if (rendered_frame_changes == 0) {
    std::fprintf(stderr, "firework scene did not change rendered framebuffer contents\n");
    return 1;
  }
  const int final_x = particle_record_at(water_index).x;
  const int final_y = particle_record_at(water_index).y;
  if (final_x <= 1 || final_x >= 19 || final_y <= 1 || final_y >= 19 ||
      (final_x == water_x && final_y == water_start_y)) {
    std::fprintf(stderr,
                 "frame scheduler failed to move water: start=(%d,%d), final=(%d,%d)\n",
                 water_x, water_start_y, final_x, final_y);
    return 1;
  }
  std::printf("fire/gunpowder/firework ran %d locks, framebuffer changed %d times; pool=%d, powder=%d/%d, flame=%d, rockets=%d (66 peak %d); water (%d,%d)->(%d,%d)\n",
              lock_count, rendered_frame_changes, scene_particle_pool_end, scene_fireworks_at_end,
              scene_gunpowder_at_end, scene_flames_at_end, scene_rocket_peak,
              scene_detonation_rocket_peak, water_x, water_start_y, final_x, final_y);
  return 0;
}
