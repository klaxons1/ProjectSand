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

// ==== REA-reconstructed original symbols (x86_64/arm64 libsand.so via objdump/nm) ====
void _Z10draw_pointss(); // original demangled via c++filt: _Z10draw_pointss
void _Z10hold_pixelss(); // original demangled via c++filt: _Z10hold_pixelss
void _Z10init_groupv(); // original demangled via c++filt: _Z10init_groupv
void _Z10near_countsshhb(); // original demangled via c++filt: _Z10near_countsshhb
void _Z10near_pointsshhh(); // original demangled via c++filt: _Z10near_pointsshhh
void _Z10pixel_boolssh(); // original demangled via c++filt: _Z10pixel_boolssh
void _Z10shift_dropss(); // original demangled via c++filt: _Z10shift_dropss
void _Z10shift_linessss(); // original demangled via c++filt: _Z10shift_linessss
void _Z11change_zoomhb(); // original demangled via c++filt: _Z11change_zoomhb
void _Z11draw_bordert(); // original demangled via c++filt: _Z11draw_bordert
void _Z11draw_bordertt(); // original demangled via c++filt: _Z11draw_bordertt
void _Z11draw_squaressss(); // original demangled via c++filt: _Z11draw_squaressss
void _Z11draw_squaressssc(); // original demangled via c++filt: _Z11draw_squaressssc
void _Z11near_configccc(); // original demangled via c++filt: _Z11near_configccc
void _Z11restore_boxv(); // original demangled via c++filt: _Z11restore_boxv
void _Z11run_command14command_struct(); // original demangled via c++filt: _Z11run_command14command_struct
void _Z11wake_aroundss(); // original demangled via c++filt: _Z11wake_aroundss
void _Z11wake_borderv(); // original demangled via c++filt: _Z11wake_borderv
void _Z12chain_appendccb(); // original demangled via c++filt: _Z12chain_appendccb
void _Z12chain_updatev(); // original demangled via c++filt: _Z12chain_updatev
void _Z12clear_eraserv(); // original demangled via c++filt: _Z12clear_eraserv
void _Z12clear_screenb(); // original demangled via c++filt: _Z12clear_screenb
void _Z12delete_pixelss(); // original demangled via c++filt: _Z12delete_pixelss
void _Z12new_particleb(); // original demangled via c++filt: _Z12new_particleb
void _Z12process_heath(); // original demangled via c++filt: _Z12process_heath
void _Z12slide_liquidssibh(); // original demangled via c++filt: _Z12slide_liquidssibh
void _Z12slide_powderssibh(); // original demangled via c++filt: _Z12slide_powderssibh
void _Z13add_grouptypesshb(); // original demangled via c++filt: _Z13add_grouptypesshb
void _Z13draw_particlev(); // original demangled via c++filt: _Z13draw_particlev
void _Z13init_particleissh(); // original demangled via c++filt: _Z13init_particleissh
void _Z13release_pixel5point(); // original demangled via c++filt: _Z13release_pixel5point
void _Z13seek_particlev(); // original demangled via c++filt: _Z13seek_particlev
void _Z13wake_particless(); // original demangled via c++filt: _Z13wake_particless
void _Z14around_matchesssh(); // original demangled via c++filt: _Z14around_matchesssh
void _Z14calc_state_allv(); // original demangled via c++filt: _Z14calc_state_allv
void _Z14trans_particleihb(); // original demangled via c++filt: _Z14trans_particleihb
void _Z15drop_interuptedss(); // original demangled via c++filt: _Z15drop_interuptedss
void _Z15get_particle_idss(); // original demangled via c++filt: _Z15get_particle_idss
void _Z15pull_surface_HRib(); // original demangled via c++filt: _Z15pull_surface_HRib
void _Z15resume_particless(); // original demangled via c++filt: _Z15resume_particless
void _Z15return_particlessbb(); // original demangled via c++filt: _Z15return_particlessbb
void _Z16displace_releasessh(); // original demangled via c++filt: _Z16displace_releasessh
void _Z16is_lower_densityssh(); // original demangled via c++filt: _Z16is_lower_densityssh
void _Z16release_and_holdss(); // original demangled via c++filt: _Z16release_and_holdss
void _Z16release_and_holdss(); // original demangled via c++filt: _Z16release_and_holdss
void _Z16suspend_particlei(); // original demangled via c++filt: _Z16suspend_particlei
void _Z17before_first_lockv(); // original demangled via c++filt: _Z17before_first_lockv
void _Z17current_can_sleepv(); // original demangled via c++filt: _Z17current_can_sleepv
void _Z17displace_make_gapss(); // original demangled via c++filt: _Z17displace_make_gapss
void _Z17pixel_custom_testssh(); // original demangled via c++filt: _Z17pixel_custom_testssh
void _Z19process_transistionhh(); // original demangled via c++filt: _Z19process_transistionhh
void _Z20set_rocket_directioniss(); // original demangled via c++filt: _Z20set_rocket_directioniss
void _Z3taph(); // original demangled via c++filt: _Z3taph
void _Z5flamev(); // original demangled via c++filt: _Z5flamev
void _Z7draw_toss(); // original demangled via c++filt: _Z7draw_toss
void _Z7texturesshb(); // original demangled via c++filt: _Z7texturesshb
void _Z8add_typesshb(); // original demangled via c++filt: _Z8add_typesshb
void _Z8init_allb(); // original demangled via c++filt: _Z8init_allb
void _Z8is_solidss(); // original demangled via c++filt: _Z8is_solidss
void _Z8pixel_issshh(); // original demangled via c++filt: _Z8pixel_issshh
void _Z8pressuress(); // original demangled via c++filt: _Z8pressuress
void _Z8pressuressh(); // original demangled via c++filt: _Z8pressuressh
void _Z8save_boxss(); // original demangled via c++filt: _Z8save_boxss
void _Z8seek_gasv(); // original demangled via c++filt: _Z8seek_gasv
void _Z8seek_rayv(); // original demangled via c++filt: _Z8seek_rayv
void _Z8set_firess(); // original demangled via c++filt: _Z8set_firess
void _Z9can_sleepi(); // original demangled via c++filt: _Z9can_sleepi
void _Z9can_sleepss(); // original demangled via c++filt: _Z9can_sleepss
void _Z9kill_firessb(); // original demangled via c++filt: _Z9kill_firessb
void _Z9main_loopv(); // original demangled via c++filt: _Z9main_loopv
void _Z9scan_linesssshh(); // original demangled via c++filt: _Z9scan_linesssshh
void _Z9seek_boomv(); // original demangled via c++filt: _Z9seek_boomv
void _Z9set_framev(); // original demangled via c++filt: _Z9set_framev

// High-level REA wrappers
void main_loop();
void init_all(bool first);
void texture(int x, int y, uint8_t mat);
void change_zoom(char z, bool b);
