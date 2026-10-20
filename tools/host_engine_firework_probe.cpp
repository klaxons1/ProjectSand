// Host regression for the production UI-tool, fire, and rocket paths. This is
// the real restored engine, not a replacement simulator or ARM64 runtime test.
#include "../app/src/main/cpp/arm64/libsand_decompiled.cpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned char cell_material(int x, int y, int width) {
  return *reinterpret_cast<unsigned char *>(g_cell_state_grid + 8LL * (x + y * width));
}

int main() {
  constexpr int width = 64;
  constexpr int height = 64;
  constexpr int center_x = 32;
  constexpr int center_y = 32;
  constexpr unsigned char firework_tool = 23;
  constexpr unsigned char gunpowder_tool = 22;
  constexpr unsigned char fire_tool = 38;
  constexpr unsigned char rocket_materials[] = {64, 65, 66};
  constexpr int edge_positions[][2] = {
      {2, 2}, {3, 3}, {2, 32}, {3, 32},
      {61, 32}, {60, 32}, {32, 61}, {32, 60},
  };

  const auto *firework_group = &g_material_properties_lut[224 + 4 * firework_tool];
  const auto *gunpowder_group = &g_material_properties_lut[224 + 4 * gunpowder_tool];
  if (firework_group[0] != 38 || firework_group[1] != 3 ||
      gunpowder_group[0] != 74 || gunpowder_group[1] != 2) {
    std::fprintf(stderr, "UI material-group metadata does not match firework/gunpowder\n");
    return 1;
  }

  int firework_transitions = 0;
  int gunpowder_transitions = 0;
  int flame_transitions = 0;
  int rockets_started_by_gunpowder = 0;

  // Exercise the actual UI group mappings, including flame creation, then
  // execute the recovered movement and transition functions for each particle.
  for (int direction = 0; direction < 8; ++direction) {
    for (int seed = 1; seed <= 64; ++seed) {
      initialize_simulation_state(1);
      g_view_width[0] = width;
      g_view_height = height;
      g_framebuffer_stride = width;
      if (!g_cell_state_grid)
        g_cell_state_grid = reinterpret_cast<__int64>(std::calloc(width * height, 8));
      if (!g_cell_state_grid) {
        std::fprintf(stderr, "could not allocate firework interaction test grid\n");
        return 1;
      }
      std::memset(reinterpret_cast<void *>(g_cell_state_grid), 0, width * height * 8);
      g_cell_state_allocated = 1;
      g_engine_mode = 7;
      g_random_state = static_cast<std::uint64_t>(seed) * 0x9e3779b97f4a7c15ULL;
      g_direction_index = static_cast<char>(direction);

      add_grouptype(center_x - 2, center_y, fire_tool, 1);
      add_grouptype(center_x, center_y, firework_tool, 1);
      add_grouptype(center_x + 1, center_y, gunpowder_tool, 1);
      add_grouptype(center_x, center_y + 1, firework_tool, 1);
      add_grouptype(center_x + 1, center_y + 1, gunpowder_tool, 1);
      const unsigned char fire_material = cell_material(center_x - 2, center_y, width);
      const unsigned char firework_material = cell_material(center_x, center_y, width);
      const unsigned char gunpowder_material = cell_material(center_x + 1, center_y, width);
      const unsigned char rocket_fuel_material = cell_material(center_x + 1, center_y + 1, width);
      const unsigned char rocket_target_material = cell_material(center_x, center_y + 1, width);
      if (fire_material < 59 || fire_material > 61 ||
          firework_material < firework_group[0] ||
          firework_material >= firework_group[0] + firework_group[1] ||
          gunpowder_material < gunpowder_group[0] ||
          gunpowder_material >= gunpowder_group[0] + gunpowder_group[1] ||
          rocket_fuel_material < gunpowder_group[0] ||
          rocket_fuel_material >= gunpowder_group[0] + gunpowder_group[1] ||
          rocket_target_material < firework_group[0] ||
          rocket_target_material >= firework_group[0] + firework_group[1]) {
        std::fprintf(stderr, "UI fire/firework/gunpowder tools created unexpected materials\n");
        return 1;
      }

      if (static_cast<unsigned char>(g_shared_engine_data[12 * firework_material + 20]) != 3 ||
          static_cast<unsigned char>(g_shared_engine_data[12 * firework_material + 21]) != 13 ||
          static_cast<unsigned char>(g_shared_engine_data[12 * firework_material + 22]) != 35 ||
          static_cast<unsigned char>(g_shared_engine_data[12 * gunpowder_material + 20]) != 3 ||
          static_cast<unsigned char>(g_shared_engine_data[12 * gunpowder_material + 21]) != 13 ||
          static_cast<unsigned char>(g_shared_engine_data[12 * gunpowder_material + 22]) != 36) {
        std::fprintf(stderr, "UI explosive material transition metadata changed\n");
        return 1;
      }

      int firework_index = *reinterpret_cast<int *>(
          g_cell_state_grid + 8LL * (center_x + center_y * width) + 4);
      int gunpowder_index = *reinterpret_cast<int *>(
          g_cell_state_grid + 8LL * (center_x + 1 + center_y * width) + 4);
      int rocket_fuel_index = *reinterpret_cast<int *>(
          g_cell_state_grid + 8LL * (center_x + 1 + (center_y + 1) * width) + 4);
      int flame_index = *reinterpret_cast<int *>(
          g_cell_state_grid + 8LL * (center_x - 2 + center_y * width) + 4);
      if (firework_index < 0 || gunpowder_index < 0 || rocket_fuel_index < 0 ||
          flame_index < 0) {
        std::fprintf(stderr, "UI fire/firework/gunpowder particle was not created\n");
        return 1;
      }

      // The fire-tool contact handler is the engine's actual ignition event.
      // Trigger it before movement so the test starts with a firework/gunpowder
      // cluster exactly like a freshly sprinkled scene.
      if (cell_material(center_x + 1, center_y + 1, width) != rocket_fuel_material) {
        std::fprintf(stderr, "gunpowder contact cell changed before ignition\n");
        return 1;
      }
      g_current_particle_index = rocket_fuel_index;
      ignite_particle_cell(center_x + 1, center_y + 1);
      if (particle_material_id(rocket_fuel_index) != 66) {
        std::fprintf(stderr, "fire contact did not create rocket material 66 from UI gunpowder\n");
        return 1;
      }
      particle_record_at(rocket_fuel_index).packed_state_word =
          (particle_record_at(rocket_fuel_index).packed_state_word & ~7) | 6;
      particle_counter(rocket_fuel_index) = 0;
      g_current_particle_index = rocket_fuel_index;
      process_rocket_motion();
      ++rockets_started_by_gunpowder;

      g_current_particle_index = firework_index;
      process_particle_motion();
      if (particle_material_id(firework_index) >= firework_group[0] &&
          particle_material_id(firework_index) < firework_group[0] + firework_group[1]) {
        g_current_particle_index = firework_index;
        process_material_transition(13, 35);
        ++firework_transitions;
      }

      g_current_particle_index = gunpowder_index;
      process_particle_motion();
      if (particle_material_id(gunpowder_index) >= gunpowder_group[0] &&
          particle_material_id(gunpowder_index) < gunpowder_group[0] + gunpowder_group[1]) {
        g_current_particle_index = gunpowder_index;
        process_material_transition(13, 36);
        ++gunpowder_transitions;
      }

      g_current_particle_index = flame_index;
      process_gas_motion();
      if (static_cast<unsigned char>(g_shared_engine_data[12 * particle_material_id(flame_index) + 20]) == 5) {
        g_current_particle_index = flame_index;
        process_material_transition(24, 25);
        ++flame_transitions;
      }
    }
  }

  // Internal rocket IDs exercise homing (64/65), detonation (66), near_point,
  // delete_pixel(), and edge-adjacent grid accesses under ASan.
  for (unsigned char rocket_material : rocket_materials) {
    if (static_cast<unsigned char>(g_shared_engine_data[12 * rocket_material + 20]) != 6) {
      std::fprintf(stderr, "material %u is not routed to rocket motion\n", rocket_material);
      return 1;
    }
    for (int direction = 0; direction < 8; ++direction) {
      for (const auto &position : edge_positions) {
        for (int seed = 1; seed <= 12; ++seed) {
          initialize_simulation_state(1);
          g_view_width[0] = width;
          g_view_height = height;
          g_framebuffer_stride = width;
          if (!g_cell_state_grid)
            g_cell_state_grid = reinterpret_cast<__int64>(std::calloc(width * height, 8));
          if (!g_cell_state_grid) {
            std::fprintf(stderr, "could not allocate rocket test grid\n");
            return 1;
          }
          std::memset(reinterpret_cast<void *>(g_cell_state_grid), 0, width * height * 8);
          g_cell_state_allocated = 1;
          g_engine_mode = 7;
          g_random_state = static_cast<std::uint64_t>(seed) * 0x9e3779b97f4a7c15ULL;

          const int rocket_x = position[0];
          const int rocket_y = position[1];
          const int dx = static_cast<__int16>(g_direction_lut[16 * direction]);
          const int dy = static_cast<__int16>(g_direction_lut[16 * direction + 1]);
          add_type(rocket_x, rocket_y, rocket_material, 1);
          const int target_x = rocket_x + dx;
          const int target_y = rocket_y + dy;
          if (target_x > 1 && target_y > 1 && target_x < width - 2 &&
              target_y < height - 2)
            add_type(target_x, target_y, rocket_material == 66 ? 38 : 14, 1);

          const int rocket_index = *reinterpret_cast<int *>(
              g_cell_state_grid + 8LL * (rocket_x + rocket_y * width) + 4);
          if (rocket_index < 0 || particle_material_id(rocket_index) != rocket_material) {
            std::fprintf(stderr,
                         "rocket %u was not created at direction %d, position (%d,%d)\n",
                         rocket_material, direction, rocket_x, rocket_y);
            return 1;
          }
          particle_record_at(rocket_index).packed_state_word =
              (particle_record_at(rocket_index).packed_state_word & ~7) | direction;
          particle_counter(rocket_index) = 0;
          g_current_particle_index = rocket_index;
          process_rocket_motion();
        }
      }
    }
  }

  if (firework_transitions < 256 || gunpowder_transitions < 256 ||
      flame_transitions < 256 || rockets_started_by_gunpowder < 512) {
    std::fprintf(stderr,
                 "interaction paths under-covered: firework=%d gunpowder=%d flame=%d rocket=%d\n",
                 firework_transitions, gunpowder_transitions, flame_transitions,
                 rockets_started_by_gunpowder);
    return 1;
  }
  std::printf("UI fire/firework/gunpowder transitions passed (%d/%d/%d); ignition created %d rockets; rocket stress passed\n",
              flame_transitions, firework_transitions, gunpowder_transitions,
              rockets_started_by_gunpowder);
  return 0;
}
