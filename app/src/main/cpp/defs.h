#pragma once

// Minimal Hex-Rays compatibility layer for the recovered ARM64 C++ output.
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <pthread.h>
#include <cstdint>
#include <cstddef>
#include <array>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdio>
#include <unistd.h>
#include <stdarg.h>
#include <type_traits>
#include <sys/mman.h>
#include <dlfcn.h>

#define __int64 long long
#define __int32 int
#define __int16 short
#define __int8 char
#define __fastcall
#define __noreturn __attribute__((noreturn))
#define _BYTE unsigned char
#define _WORD unsigned short
#define _DWORD unsigned int
#define _QWORD unsigned long long
#define _UNKNOWN unsigned long long
#define _BOOL8 long long
#define _BOOL4 int
#define gcc_va_list va_list
#define JUMPOUT(x) __builtin_unreachable()
#define abs32(x) abs(x)

#define BYTEn(x, n) (*((_BYTE *)&(x) + (n)))
#define WORDn(x, n) (*((_WORD *)&(x) + (n)))
#define DWORDn(x, n) (*((_DWORD *)&(x) + (n)))
#define LOBYTE(x) BYTEn(x, 0)
#define HIBYTE(x) BYTEn(x, 1)
#define LOWORD(x) WORDn(x, 0)
#define HIWORD(x) WORDn(x, 1)
#define LODWORD(x) DWORDn(x, 0)
#define HIDWORD(x) DWORDn(x, 1)
#define SHIDWORD(x) (*((int *)&(x) + 1))
#define BYTE1(x) BYTEn(x, 1)
#define BYTE2(x) BYTEn(x, 2)
#define BYTE3(x) BYTEn(x, 3)
#define WORD1(x) WORDn(x, 1)
#define WORD2(x) WORDn(x, 2)
#define DWORD1(x) DWORDn(x, 1)
#define DWORD2(x) DWORDn(x, 2)
#define SDWORD1(x) (*((int *)&(x) + 1))
#define SDWORD2(x) (*((int *)&(x) + 2))
#define SWORD2(x) (*((short *)&(x) + 2))
#define _OWORD __int128

// The ANativeWindow buffer and subsequent globals share one packed runtime
// region in the recovered BSS image. Its buffer begins four bytes into this
// region, preserving the original native-window ABI layout.
#pragma pack(push, 4)
struct NativeWindowBufferRegion {
  std::array<std::uint8_t, 4> prefix{};
  ANativeWindow_Buffer buffer{};
  std::int8_t restore_simulation_box_on_next_surface{};
  std::array<std::uint8_t, 7> padding_53_to_59{};
  std::uintptr_t engine_class_global_ref{};
  std::uintptr_t init_callback_method{};
  std::int8_t requested_direction{};
  std::int8_t previous_direction{};
  std::int8_t brush_radius{};
  std::array<std::uint8_t, 5> padding_79_to_83{};
  std::int16_t last_touch_pixel_x{};
  std::int16_t last_touch_pixel_y{};
  std::int8_t gas_field_enabled{};
  std::int8_t frame_rate_adaptation_counter{};
  std::int8_t touch_drawing_active{};
  std::int8_t drag_tool_active{};
  std::int8_t command_queue_read_index{};
  std::int8_t command_queue_write_index{};
  std::array<std::uint8_t, 6> padding_94_to_99{};
  std::uint8_t command_queue[300]{};
  std::int32_t simulation_tick_counter{};
  std::int16_t previous_frame_timestamp{};
  std::int8_t particle_state_list_sync_pending{};
  std::array<std::uint8_t, 701> tail_padding{};
};
#pragma pack(pop)
static_assert(sizeof(__int64) == 8);
static_assert(sizeof(std::uintptr_t) == 8);
static_assert(sizeof(ANativeWindow_Buffer) == 48);
static_assert(offsetof(NativeWindowBufferRegion, buffer) == 4);
static_assert(offsetof(NativeWindowBufferRegion, buffer) + offsetof(ANativeWindow_Buffer, stride) == 12);
static_assert(offsetof(NativeWindowBufferRegion, buffer) + offsetof(ANativeWindow_Buffer, bits) == 20);
static_assert(offsetof(NativeWindowBufferRegion, restore_simulation_box_on_next_surface) == 52);
static_assert(offsetof(NativeWindowBufferRegion, engine_class_global_ref) == 60);
static_assert(offsetof(NativeWindowBufferRegion, command_queue) == 100);
static_assert(offsetof(NativeWindowBufferRegion, simulation_tick_counter) == 400);
static_assert(sizeof(NativeWindowBufferRegion) == 1108);
static_assert(offsetof(ANativeWindow_Buffer, bits) == 16);

// The writable ELF data image contains the engine header and material bytes.
// Model its stable fields by their declared widths and keep byte views only for
// the material table, whose records share the same image.
struct NativeDataRegion {
  std::uint64_t base_marker{};
  std::array<std::uint8_t, 4> shared_engine_header{};
  std::int32_t framebuffer_stride{};
  std::int8_t engine_mode{};
  std::array<std::uint8_t, 3> header_padding{};
  std::int32_t current_particle_index{};
  std::array<std::uint8_t, 0x4c0> material_table_prefix{};
  std::int64_t random_state{};
  std::int32_t particle_list_previous_index{};
  std::int8_t zoom_factor{};
  std::int8_t selected_material{};
  std::int8_t selected_material_state{};
  std::array<std::uint8_t, 0x29> trailing_data{};

  std::uint8_t *bytes() { return reinterpret_cast<std::uint8_t *>(this); }
  std::int8_t &data_byte_at(std::size_t offset) {
    return *reinterpret_cast<std::int8_t *>(bytes() + offset);
  }
  std::int16_t &data_word_at(std::size_t offset) {
    return *reinterpret_cast<std::int16_t *>(bytes() + offset);
  }
};
static_assert(offsetof(NativeDataRegion, base_marker) == 0x00);
static_assert(offsetof(NativeDataRegion, shared_engine_header) == 0x08);
static_assert(offsetof(NativeDataRegion, framebuffer_stride) == 0x0c);
static_assert(offsetof(NativeDataRegion, engine_mode) == 0x10);
static_assert(offsetof(NativeDataRegion, current_particle_index) == 0x14);
static_assert(offsetof(NativeDataRegion, material_table_prefix) == 0x18);
static_assert(offsetof(NativeDataRegion, random_state) == 0x4d8);
static_assert(offsetof(NativeDataRegion, particle_list_previous_index) == 0x4e0);
static_assert(offsetof(NativeDataRegion, zoom_factor) == 0x4e4);
static_assert(offsetof(NativeDataRegion, selected_material) == 0x4e5);
static_assert(offsetof(NativeDataRegion, selected_material_state) == 0x4e6);
static_assert(offsetof(NativeDataRegion, trailing_data) == 0x4e7);
static_assert(sizeof(NativeDataRegion) == 0x510);

// The renderer, dirty rings, and their index/pixel aliases share one BSS span.
// Keep its physical storage contiguous while exposing fixed-width typed access.
struct FramebufferMemory {
  std::int16_t width{};
  std::int16_t height{};
  std::int16_t background_color{};
  std::int8_t bank_index{};
  std::uint8_t header_padding{};
  __int64 bank_words[600012]{};
  std::int8_t visualization_material_mode{};
  std::array<std::uint8_t, 7> tail_padding{};

  std::int32_t &dword_at(std::size_t offset) {
    return *reinterpret_cast<std::int32_t *>(
        reinterpret_cast<std::uint8_t *>(bank_words) + offset);
  }
  std::int8_t &byte_at(std::size_t offset) {
    return *reinterpret_cast<std::int8_t *>(
        reinterpret_cast<std::uint8_t *>(bank_words) + offset);
  }
  __int64 &qword_at(std::size_t offset) {
    return *reinterpret_cast<__int64 *>(
        reinterpret_cast<std::uint8_t *>(bank_words) + offset);
  }
};
static_assert(offsetof(FramebufferMemory, width) == 0x00);
static_assert(offsetof(FramebufferMemory, height) == 0x02);
static_assert(offsetof(FramebufferMemory, background_color) == 0x04);
static_assert(offsetof(FramebufferMemory, bank_index) == 0x06);
static_assert(offsetof(FramebufferMemory, bank_words) == 0x08);
static_assert(offsetof(FramebufferMemory, visualization_material_mode) == 0x493e68);
static_assert(sizeof(FramebufferMemory) == 0x493e70);

// A particle occupies ten 16-bit words in the recovered pool. Keep the field
// offsets explicit so the engine no longer depends on host `short`/`int` sizes.
struct ParticleRecord {
  std::int16_t x{};
  std::int16_t y{};
  std::uint16_t next_link_low{};
  std::uint16_t next_link_high{};
  std::uint16_t material_flags_word{};
  std::uint16_t auxiliary_status_word{};
  std::uint16_t packed_state_word{};
  std::uint16_t reserved_word_7{};
  std::uint16_t counter_word{};
  std::uint16_t reserved_word_9{};
};
static_assert(sizeof(ParticleRecord) == 20);
static_assert(offsetof(ParticleRecord, x) == 0);
static_assert(offsetof(ParticleRecord, y) == 2);
static_assert(offsetof(ParticleRecord, next_link_low) == 4);
static_assert(offsetof(ParticleRecord, next_link_high) == 6);
static_assert(offsetof(ParticleRecord, material_flags_word) == 8);
static_assert(offsetof(ParticleRecord, auxiliary_status_word) == 10);
static_assert(offsetof(ParticleRecord, packed_state_word) == 12);
static_assert(offsetof(ParticleRecord, reserved_word_7) == 14);
static_assert(offsetof(ParticleRecord, counter_word) == 16);
static_assert(offsetof(ParticleRecord, reserved_word_9) == 18);

// Particle list headers use 12-byte strides, while several decompiler globals
// viewed overlapping 16-byte blocks over the same 48-byte storage.
struct ParticleListStorage {
  std::uint32_t words[12]{};

  __int128 &block0() { return *reinterpret_cast<__int128 *>(&words[0]); }
  __int128 &block1() { return *reinterpret_cast<__int128 *>(&words[4]); }
  __int128 &block2() { return *reinterpret_cast<__int128 *>(&words[8]); }
  std::uint32_t &head(unsigned index) {
    return words[3 * index];
  }
};
static_assert(sizeof(ParticleListStorage) == 48);

// Exact typed layout of the state block beginning at ELF address 0xE50A00.
struct NativeRuntimeMemory {
  std::int32_t lookup_particle_index{};
  std::uint32_t padding_04{};
  std::uintptr_t framebuffer_pixels{};
  std::uintptr_t cell_state_grid{};
  std::int8_t direction_index{};
  std::array<std::uint8_t, 7> padding_19_to_1f{};
  std::int32_t held_pixel_coordinates{};
  std::int32_t held_particle_index{};
  std::int8_t pixel_is_held{};
  std::int8_t held_material_id{};
  std::array<std::uint8_t, 6> padding_2a_to_2f{};
  std::int8_t search_direction{};
  std::int8_t search_remaining{};
  std::int8_t search_step{};
  std::array<std::uint8_t, 5> padding_33_to_37{};
  std::int16_t found_x{};
  std::int16_t found_y{};
  std::array<std::uint8_t, 4> padding_3c_to_3f{};
  ParticleListStorage particle_lists{};
  std::int8_t preserve_box_on_next_resize{};
  std::uint8_t padding_71{};
  std::int16_t surface_height{};
  std::array<std::uint8_t, 4> padding_74_to_77{};
  std::uintptr_t jni_env{};
  std::uintptr_t surface_global_ref{};
  std::uintptr_t native_window{};
  std::int16_t surface_width{};
  std::int8_t cell_state_allocated{};
  std::int8_t particle_update_phase{};
  std::int8_t particle_counter_reset_value{};
  std::int8_t particle_counter_threshold{};
  std::int8_t gas_motion_enabled{};
  std::int8_t gas_search_radius{};
  std::int32_t next_particle_index{};
  std::array<std::uint8_t, 4> padding_9c_to_9f{};
  std::int16_t drag_tool_origin_x{};
  std::int16_t drag_tool_origin_y{};
  NativeWindowBufferRegion window_region{};

  std::uintptr_t window_pixel_pointer() const {
    return reinterpret_cast<std::uintptr_t>(window_region.buffer.bits);
  }
};
static_assert(offsetof(NativeRuntimeMemory, lookup_particle_index) == 0x00);
static_assert(offsetof(NativeRuntimeMemory, framebuffer_pixels) == 0x08);
static_assert(offsetof(NativeRuntimeMemory, cell_state_grid) == 0x10);
static_assert(offsetof(NativeRuntimeMemory, direction_index) == 0x18);
static_assert(offsetof(NativeRuntimeMemory, held_pixel_coordinates) == 0x20);
static_assert(offsetof(NativeRuntimeMemory, held_particle_index) == 0x24);
static_assert(offsetof(NativeRuntimeMemory, pixel_is_held) == 0x28);
static_assert(offsetof(NativeRuntimeMemory, held_material_id) == 0x29);
static_assert(offsetof(NativeRuntimeMemory, search_direction) == 0x30);
static_assert(offsetof(NativeRuntimeMemory, search_remaining) == 0x31);
static_assert(offsetof(NativeRuntimeMemory, search_step) == 0x32);
static_assert(offsetof(NativeRuntimeMemory, found_x) == 0x38);
static_assert(offsetof(NativeRuntimeMemory, found_y) == 0x3a);
static_assert(offsetof(NativeRuntimeMemory, particle_lists) == 0x40);
static_assert(offsetof(NativeRuntimeMemory, preserve_box_on_next_resize) == 0x70);
static_assert(offsetof(NativeRuntimeMemory, surface_height) == 0x72);
static_assert(offsetof(NativeRuntimeMemory, jni_env) == 0x78);
static_assert(offsetof(NativeRuntimeMemory, surface_global_ref) == 0x80);
static_assert(offsetof(NativeRuntimeMemory, native_window) == 0x88);
static_assert(offsetof(NativeRuntimeMemory, surface_width) == 0x90);
static_assert(offsetof(NativeRuntimeMemory, next_particle_index) == 0x98);
static_assert(offsetof(NativeRuntimeMemory, drag_tool_origin_x) == 0xa0);
static_assert(offsetof(NativeRuntimeMemory, drag_tool_origin_y) == 0xa2);
static_assert(offsetof(NativeRuntimeMemory, window_region) == 0xa4);
static_assert(sizeof(NativeRuntimeMemory) == 0x4f8);

struct ParticlePoolStorage {
  ParticleRecord records[500000]{};

  // The two weak response symbols alias records in the last 166 pool slots;
  // preserve those original addresses instead of allocating duplicate storage.
  unsigned long long &engine_resume_response_token() {
    return *reinterpret_cast<unsigned long long *>(&records[499834].material_flags_word);
  }
  unsigned long long &engine_state_response_token() {
    return *reinterpret_cast<unsigned long long *>(&records[499872].x);
  }
};
static_assert(sizeof(ParticlePoolStorage) == 0x989680);
static_assert(offsetof(ParticlePoolStorage, records) == 0);
static_assert(499834 * sizeof(ParticleRecord) + 8 == 0x988990);
static_assert(499872 * sizeof(ParticleRecord) == 0x988c80);

struct ParticleAndRuntimeMemory {
  ParticlePoolStorage particle_pool{};
  NativeRuntimeMemory runtime{};
};
static_assert(offsetof(ParticleAndRuntimeMemory, runtime) == 0x989680);
static_assert(sizeof(ParticleAndRuntimeMemory) == 0x989b78);

// Preserve the engine's contiguous data/BSS image as named regions. All
// former runtime globals now live at their exact, asserted offsets here.
struct NativeMemoryArena {
  NativeDataRegion data{};
  FramebufferMemory framebuffer{};
  ParticleAndRuntimeMemory particle_and_runtime{};

  char *shared_engine_data() { return reinterpret_cast<char *>(this) + 8; }
  _BYTE *material_properties() { return reinterpret_cast<_BYTE *>(this) + 0x18; }
};
static_assert(offsetof(NativeMemoryArena, data) == 0);
static_assert(offsetof(NativeMemoryArena, framebuffer) == 0x510);
static_assert(offsetof(NativeMemoryArena, particle_and_runtime) == 0x494380);
static_assert(sizeof(NativeMemoryArena) == 0xe1def8);

template <typename To, typename From> static inline To hx_cast(From value) {
  if constexpr (std::is_pointer_v<From> && std::is_integral_v<To>)
    return static_cast<To>(reinterpret_cast<uintptr_t>(value));
  else if constexpr (std::is_integral_v<From> && std::is_pointer_v<To>)
    return reinterpret_cast<To>(static_cast<uintptr_t>(value));
  else if constexpr (std::is_pointer_v<From> && std::is_pointer_v<To>)
    return reinterpret_cast<To>(value);
  else
    return static_cast<To>(value);
}

static inline __int128 hx_i128(uint64_t high, uint64_t low) {
  const unsigned __int128 bits = (static_cast<unsigned __int128>(high) << 64) | low;
  return static_cast<__int128>(bits);
}

#if defined(__aarch64__)
template <typename T> static inline T __ldaxr(T *p) {
  T value;
  if constexpr (sizeof(T) == 8) __asm__ volatile("ldaxr %0, [%1]" : "=r"(value) : "r"(p) : "memory");
  else if constexpr (sizeof(T) == 4) __asm__ volatile("ldaxr %w0, [%1]" : "=r"(value) : "r"(p) : "memory");
  else if constexpr (sizeof(T) == 2) __asm__ volatile("ldaxrh %w0, [%1]" : "=r"(value) : "r"(p) : "memory");
  else __asm__ volatile("ldaxrb %w0, [%1]" : "=r"(value) : "r"(p) : "memory");
  return value;
}
template <typename T> static inline int __stlxr(T value, T *p) {
  unsigned status;
  if constexpr (sizeof(T) == 8) __asm__ volatile("stlxr %w0, %1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  else if constexpr (sizeof(T) == 4) __asm__ volatile("stlxr %w0, %w1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  else if constexpr (sizeof(T) == 2) __asm__ volatile("stlxrh %w0, %w1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  else __asm__ volatile("stlxrb %w0, %w1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  return status;
}
template <typename T> static inline int __stxr(T value, T *p) {
  unsigned status;
  if constexpr (sizeof(T) == 8) __asm__ volatile("stxr %w0, %1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  else if constexpr (sizeof(T) == 4) __asm__ volatile("stxr %w0, %w1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  else if constexpr (sizeof(T) == 2) __asm__ volatile("stxrh %w0, %w1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  else __asm__ volatile("stxrb %w0, %w1, [%2]" : "=&r"(status) : "r"(value), "r"(p) : "memory");
  return status;
}
static inline void __dmb(unsigned imm) { (void)imm; __asm__ volatile("dmb ish" ::: "memory"); }
#else
// Host-only syntax-test fallbacks; Android builds use the AArch64 asm above.
template <typename T> static inline T __ldaxr(T *p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }
template <typename T> static inline int __stlxr(T v, T *p) { __atomic_store_n(p, v, __ATOMIC_RELEASE); return 0; }
template <typename T> static inline int __stxr(T v, T *p) { __atomic_store_n(p, v, __ATOMIC_RELAXED); return 0; }
static inline void __dmb(unsigned) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
#endif

extern "C" void __cxa_finalize(void *);
extern "C" int __cxa_atexit(void (*)(void *), void *, void *);
extern "C" void __assert2(const char *, int, const char *, const char *);
