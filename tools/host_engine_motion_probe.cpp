// Host-side probe that executes the restored production engine, not a substitute
// simulator. It exercises liquid movement and diagonal draw_to() placement; it
// does not replace an Android/ARM64 runtime test.
#include "../app/src/main/cpp/arm64/libsand_decompiled.cpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void send_touch_command(unsigned char action, short x, short y) {
  const std::uint64_t packed = static_cast<std::uint64_t>(action) |
      (static_cast<std::uint64_t>(static_cast<std::uint16_t>(x)) << 16) |
      (static_cast<std::uint64_t>(static_cast<std::uint16_t>(y)) << 32);
  run_command(static_cast<__int64>(packed));
}

int main() {
  constexpr int width = 64;
  constexpr int height = 64;
  constexpr int start_x = 32;
  constexpr int start_y = 32;
  constexpr int expected_directions[8][2] = {
      {0, 1}, {1, 1}, {1, 0}, {1, -1},
      {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}};
  // Engine material IDs reached by the water (tool 3), oil (tool 4), and
  // mercury (tool 5) buttons. Water has two randomized internal variants.
  constexpr unsigned char fluid_material_ids[] = {5, 6, 7, 8};

  for (const unsigned char material_id : fluid_material_ids) {
    for (int direction = 0; direction < 8; ++direction) {
      init_all(1);
      g_view_width[0] = width;
      g_view_height = height;
      g_framebuffer_stride = width;
      g_cell_state_grid = reinterpret_cast<__int64>(
          std::calloc(static_cast<size_t>(width) * height, 8));
      if (!g_cell_state_grid) {
        std::fprintf(stderr, "could not allocate test cell grid\n");
        return 1;
      }
      g_cell_state_allocated = 1;
      g_engine_mode = 7;
      g_direction_index = static_cast<char>(direction);
      if (static_cast<unsigned char>(g_shared_engine_data[12 * material_id + 20]) != 4) {
        std::fprintf(stderr, "material %u is not routed through liquid motion\n", material_id);
        return 1;
      }

      add_type(start_x, start_y, material_id, 0);
      const int particle_index = *reinterpret_cast<int*>(
          g_cell_state_grid + 8LL * (start_x + start_y * width) + 4);
      if (particle_index < 0) {
        std::fprintf(stderr,
                     "material %u, direction %d: particle was not created\n",
                     material_id, direction);
        return 1;
      }

      g_current_particle_index = particle_index;
      seek_particle();

      const int dx = static_cast<__int16>(g_direction_lut[16 * direction + 4]);
      const int dy = static_cast<__int16>(g_direction_lut[16 * direction + 5]);
      if (dx != expected_directions[direction][0] ||
          dy != expected_directions[direction][1]) {
        std::fprintf(stderr, "direction LUT entry %d is (%d,%d), expected (%d,%d)\n",
                     direction, dx, dy, expected_directions[direction][0],
                     expected_directions[direction][1]);
        return 1;
      }
      const int expected_x = start_x + byte_E50A97 * dx;
      const int expected_y = start_y + byte_E50A97 * dy;
      const int actual_x = g_particle_records[10 * particle_index];
      const int actual_y = g_particle_records[10 * particle_index + 1];
      if (actual_x != expected_x || actual_y != expected_y) {
        std::fprintf(stderr,
                     "material %u, direction %d: expected (%d,%d), got (%d,%d)\n",
                     material_id, direction, expected_x, expected_y, actual_x,
                     actual_y);
        return 1;
      }
      std::printf("material %u, direction %d: moved to (%d,%d)\n",
                  material_id, direction, actual_x, actual_y);
    }
  }

  // Exercise the real pointer-stroke path at opposing diagonal slopes with
  // several tool mappings. Every created particle must stay in bounds, belong
  // to the selected material family, and remain near the requested stroke.
  constexpr int stroke_cases[][5] = {
      {1, 20, 20, 42, 38}, {1, 20, 42, 42, 20},
      {3, 20, 20, 42, 38}, {3, 20, 42, 42, 20},
      {4, 20, 20, 42, 38}, {4, 20, 42, 42, 20},
      {5, 20, 20, 42, 38}, {5, 20, 42, 42, 20},
  };
  for (const auto& stroke : stroke_cases) {
    init_all(1);
    g_view_width[0] = width;
    g_view_height = height;
    g_framebuffer_stride = width;
    g_cell_state_grid = reinterpret_cast<__int64>(
        std::calloc(static_cast<size_t>(width) * height, 8));
    if (!g_cell_state_grid) {
      std::fprintf(stderr, "could not allocate diagonal-stroke test grid\n");
      return 1;
    }
    g_cell_state_allocated = 1;
    g_engine_mode = 7;
    g_direction_index = 0;
    g_selected_material = static_cast<char>(stroke[0]);
    byte_E50AF2 = 1;
    word_E50AF8 = static_cast<__int16>(stroke[1]);
    word_E50AFA = static_cast<__int16>(stroke[2]);
    draw_to(static_cast<__int16>(stroke[3]), static_cast<__int16>(stroke[4]));

    const int particle_count = dword_E50A98;
    const int material_family_start = g_material_properties_lut[224 + 4 * stroke[0]];
    const int material_family_size = g_material_properties_lut[225 + 4 * stroke[0]];
    if (particle_count == 0 || material_family_size == 0) {
      std::fprintf(stderr, "tool %d produced no diagonal stroke particles\n", stroke[0]);
      return 1;
    }
    const double dx = stroke[3] - stroke[1];
    const double dy = stroke[4] - stroke[2];
    const double line_length_squared = dx * dx + dy * dy;
    for (int particle = 0; particle < particle_count; ++particle) {
      const int x = g_particle_records[10 * particle];
      const int y = g_particle_records[10 * particle + 1];
      const int material_id = reinterpret_cast<unsigned char*>(
          g_particle_records + 10LL * particle)[8];
      double t = ((x - stroke[1]) * dx + (y - stroke[2]) * dy) /
                 line_length_squared;
      if (t < 0.0) t = 0.0;
      if (t > 1.0) t = 1.0;
      const double offset_x = x - (stroke[1] + t * dx);
      const double offset_y = y - (stroke[2] + t * dy);
      if (x <= 1 || x >= width - 2 || y <= 1 || y >= height - 2 ||
          material_id < material_family_start ||
          material_id >= material_family_start + material_family_size ||
          offset_x * offset_x + offset_y * offset_y > 9.0) {
        std::fprintf(stderr,
                     "tool %d diagonal stroke produced an out-of-family or off-line particle\n",
                     stroke[0]);
        return 1;
      }
    }
    std::printf("tool %d diagonal stroke: %d particles stayed on the stroke\n",
                stroke[0], particle_count);
  }

  // Regression: flowing and granular brush strokes must attempt every
  // interpolated cell, rather than dropping seven of every eight placements.
  constexpr unsigned char dense_stroke_tools[] = {1, 3, 4, 5};
  for (const unsigned char tool_id : dense_stroke_tools) {
    init_all(1);
    g_view_width[0] = width;
    g_view_height = height;
    g_framebuffer_stride = width;
    g_cell_state_grid = reinterpret_cast<__int64>(
        std::calloc(static_cast<size_t>(width) * height, 8));
    if (!g_cell_state_grid) {
      std::fprintf(stderr, "could not allocate dense-stroke test grid\n");
      return 1;
    }
    g_cell_state_allocated = 1;
    g_engine_mode = 7;
    g_zoom_factor = 1;
    g_selected_material = static_cast<char>(tool_id);
    byte_E50AF2 = 1;
    send_touch_command(2, 10, 20);
    for (short x = 11; x <= 50; ++x) {
      send_touch_command(3, x, 20);
    }
    send_touch_command(4, 50, 20);

    int filled_cells = 0;
    for (int x = 10; x <= 50; ++x) {
      const unsigned char material = *reinterpret_cast<unsigned char*>(
          g_cell_state_grid + 8LL * (x + 20 * width));
      if (material == 0) {
        std::fprintf(stderr,
                     "tool %u left a hole at (%d,20) in a native touch stroke\n",
                     tool_id, x);
        return 1;
      }
      ++filled_cells;
    }
    std::printf("tool %u native touch stroke: %d/41 cells filled\n",
                tool_id, filled_cells);
  }

  // A non-particle wall cell blocks direct gravity; the production liquid
  // scheduler should find a route around it rather than overwrite the obstacle.
  init_all(1);
  g_view_width[0] = width;
  g_view_height = height;
  g_framebuffer_stride = width;
  g_cell_state_grid = reinterpret_cast<__int64>(
      std::calloc(static_cast<size_t>(width) * height, 8));
  if (!g_cell_state_grid) {
    std::fprintf(stderr, "could not allocate obstacle test grid\n");
    return 1;
  }
  g_cell_state_allocated = 1;
  g_engine_mode = 7;
  g_direction_index = 0;
  const __int64 wall_cell = g_cell_state_grid + 8LL * (start_x + (start_y + 1) * width);
  *reinterpret_cast<unsigned char*>(wall_cell) = 10;
  *reinterpret_cast<int*>(wall_cell + 4) = -1;
  add_type(start_x, start_y, 5, 0);
  const int water_index = *reinterpret_cast<int*>(
      g_cell_state_grid + 8LL * (start_x + start_y * width) + 4);
  for (int frame = 0; frame < 4; ++frame) {
    g_current_particle_index = water_index;
    seek_particle();
  }
  const int water_x = g_particle_records[10 * water_index];
  const int water_y = g_particle_records[10 * water_index + 1];
  if ((water_x == start_x && water_y == start_y) ||
      (water_x == start_x && water_y == start_y + 1) ||
      *reinterpret_cast<unsigned char*>(wall_cell) != 10) {
    std::fprintf(stderr, "liquid did not route around the wall safely\n");
    return 1;
  }
  std::printf("blocked flow: water moved around wall to (%d,%d)\n", water_x, water_y);
}
