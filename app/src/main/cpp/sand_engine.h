#pragma once
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <pthread.h>
#include <queue>
#include <mutex>
#include <atomic>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_smellymoo_sand_Engine_init(JNIEnv *env, jclass clazz, jobject surface, jchar zoom, jboolean isResume1, jboolean isResume2);
JNIEXPORT void JNICALL Java_smellymoo_sand_Engine_command(JNIEnv *env, jclass clazz, jchar cmd, jshort x, jshort y, jboolean isKill);

#ifdef __cplusplus
}
#endif

struct Command {
    char id;
    short x;
    short y;
    bool kill;
};

enum Material : uint8_t {
    MAT_EMPTY = 0,
    MAT_SAND = 1,
    MAT_SALT = 2,
    MAT_WATER = 3,
    MAT_OIL = 4,
    MAT_MERCURY = 5,
    MAT_WALL = 6,
    MAT_STONE = 7,
    MAT_GLASS = 8,
    MAT_METAL = 9,
    MAT_GAS = 10,
    MAT_ALGAE = 11,
    MAT_ACID = 12,
    MAT_WAX = 13,
    MAT_LAVA = 14,
    MAT_CEMENT = 15,
    MAT_WOOD = 16,
    MAT_COAL = 17,
    MAT_SAWDUST = 31,
    MAT_GUNPOWDER = 22,
    MAT_FIREWORK = 23,
    MAT_PETROL = 24,
    MAT_PARAFFIN = 25,
    MAT_NITRO = 26,
    MAT_C4 = 27,
    MAT_BLACK = 28,
    MAT_NUKE = 29,
    MAT_PLANT = 20,
    MAT_ICE = 21,
    MAT_WICK = 19,
    MAT_LIFE = 42,
    MAT_FILTER = 39,
    MAT_PERM = 34,
    MAT_TAP = 37,
    MAT_PUMP = 40,
    MAT_PORTAL = 41,
    MAT_THERMITE = 6, // alias
    MAT_FIRE = 38
};

void push_command(char c, short x, short y, bool kill);
void process_command(const Command& cmd);
void* engine_thread_func(void* arg);
void render_frame();
void init_simulation(int w, int h);
void clear_simulation();
