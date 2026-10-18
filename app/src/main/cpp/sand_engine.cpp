#include "sand_engine.h"
#include <android/log.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <mutex>
#include <queue>
#include <thread>
#include <chrono>

#define LOG_TAG "native_sand"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// ============================================================================
// Global engine state – mirrors original decompiled globals (byte_2204A zoom,
// byte_22049 selected material, byte_2204B mode, word_22582 cursor, etc.)
// ============================================================================

static JavaVM* g_vm = nullptr;
static jclass g_engineClazz = nullptr;
static jmethodID g_initCallback = nullptr;

static ANativeWindow* g_window = nullptr;
static pthread_t g_thread = 0;
static std::atomic<bool> g_running{false};
static std::atomic<bool> g_paused{false};
static std::atomic<bool> g_threadStarted{false};

// Mirrors of original globals extracted via REA analysis (inspect-artifact + manual
// decompilation of libsand.so for arm64-v8a/x86_64/mips64).
static std::mutex g_cmdMutex;
static std::queue<Command> g_cmdQueue;

static int g_zoom = 3;                 // byte_2204A
static int g_selectedMat = 1;          // byte_22049 (default sand)
static int g_background = 0;           // word_22586
static bool g_spouts = false;          // byte_22594
static int g_viewMode = 0;             // byte_2258B
static int g_brushSize = 5;
static bool g_borders = true;          // byte_22592
static bool g_slowMo = false;          // derived from command 7

// Simulation grid – simplified from original 200k particle system.
// Original uses per-particle linked lists (byte_22640[20 * idx] etc.) and
// separate heat/pressure buffers. We provide a dense grid with material + state.
static int g_gridW = 160;
static int g_gridH = 240;
static std::vector<uint8_t> g_grid;        // material per cell
static std::vector<int8_t>  g_heat;        // temperature offset
static std::vector<uint8_t> g_life;        // life/conway state

static short g_cursorX = 0, g_cursorY = 0; // word_22582/84
static short g_lastDrawX = -1, g_lastDrawY = -1;

// Color table – extracted from original texture() / draw_particle() analysis.
// Each material maps to an ARGB color. Values approximated from
// drawable resources (sand.jpg, water.png, etc.) and decompiled texture().
static uint32_t materialColor(uint8_t m) {
    switch (m) {
        case MAT_SAND:      return 0xFFD2B48C;
        case MAT_SALT:      return 0xFFF5F5DC;
        case MAT_WATER:     return 0xFF3A7DFF;
        case MAT_OIL:       return 0xFF3B2F1A;
        case MAT_MERCURY:   return 0xFFB0B0B0;
        case MAT_WALL:      return 0xFF8B4513;
        case MAT_STONE:     return 0xFF808080;
        case MAT_GLASS:     return 0xFFB0E0E6;
        case MAT_METAL:     return 0xFFA9A9A9;
        case MAT_GAS:       return 0xFFC8FFC8;
        case MAT_ALGAE:     return 0xFF2E8B57;
        case MAT_ACID:      return 0xFF9ACD32;
        case MAT_WAX:       return 0xFFFFE4B5;
        case MAT_LAVA:      return 0xFFFF4500;
        case MAT_CEMENT:    return 0xFFB8B8B8;
        case MAT_WOOD:      return 0xFF8B5A2B;
        case MAT_COAL:      return 0xFF2F2F2F;
        case MAT_GUNPOWDER: return 0xFF404040;
        case MAT_FIREWORK:  return 0xFFFF1493;
        case MAT_PETROL:    return 0xFF6B4423;
        case MAT_PARAFFIN:  return 0xFFEEE8AA;
        case MAT_NITRO:     return 0xFFFFFF00;
        case MAT_C4:        return 0xFF556B2F;
        case MAT_BLACK:     return 0xFF000000;
        case MAT_NUKE:      return 0xFF00FF00;
        case MAT_PLANT:     return 0xFF32CD32;
        case MAT_ICE:       return 0xFFADD8E6;
        case MAT_WICK:      return 0xFFFFA500;
        case MAT_LIFE:      return 0xFF00FFFF;
        case MAT_FILTER:    return 0xFFDAA520;
        case MAT_PERM:      return 0xFF4B0082;
        case MAT_TAP:       return 0xFF4682B4;
        case MAT_PUMP:      return 0xFF708090;
        case MAT_PORTAL:    return 0xFF9932CC;
        case MAT_FIRE:      return 0xFFFF0000;
        default:            return 0xFF000000;
    }
}

static inline int gridIndex(int x, int y) { return y * g_gridW + x; }
static inline bool inBounds(int x, int y) { return x >= 0 && x < g_gridW && y >= 0 && y < g_gridH; }

// ---------------------------------------------------------------------------
// Simulation primitives – faithful to original scan_line, shift_line,
// slide_powder, slide_liquid, seek_* helpers.
// ---------------------------------------------------------------------------

void init_simulation(int w, int h) {
    g_gridW = w;
    g_gridH = h;
    g_grid.assign(w * h, MAT_EMPTY);
    g_heat.assign(w * h, 0);
    g_life.assign(w * h, 0);
    LOGI("init_simulation %dx%d zoom=%d", w, h, g_zoom);
}

void clear_simulation() {
    std::fill(g_grid.begin(), g_grid.end(), MAT_EMPTY);
    std::fill(g_heat.begin(), g_heat.end(), 0);
}

static void draw_brush(int cx, int cy, uint8_t mat) {
    int r = g_brushSize;
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx) {
            if (dx*dx + dy*dy > r*r + r) continue;
            int x = cx + dx, y = cy + dy;
            if (!inBounds(x,y)) continue;
            // Original eraser (mat 0) clears, others paint
            if (mat == 0) g_grid[gridIndex(x,y)] = MAT_EMPTY;
            else if (g_grid[gridIndex(x,y)] == MAT_EMPTY || mat == MAT_WALL || mat == MAT_PERM)
                g_grid[gridIndex(x,y)] = mat;
        }
}

static void draw_line(int x0, int y0, int x1, int y1, uint8_t mat) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (true) {
        draw_brush(x0, y0, mat);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Simplified physics step – captures essence of original main_loop's
// particle scanning (seek_particle, seek_gas, seek_boom, seek_ray, flame).
static void simulate_step() {
    // heat diffusion (process_heat)
    for (int y = 1; y < g_gridH - 1; ++y)
        for (int x = 1; x < g_gridW - 1; ++x) {
            int idx = gridIndex(x,y);
            if (g_grid[idx] == MAT_LAVA || g_grid[idx] == MAT_FIRE) g_heat[idx] = 20;
            else if (g_heat[idx] > 0) g_heat[idx]--;
        }

    // falling pass – bottom to top for powder/liquid
    for (int y = g_gridH - 2; y >= 0; --y) {
        for (int x = 0; x < g_gridW; ++x) {
            uint8_t m = g_grid[gridIndex(x,y)];
            if (m == MAT_EMPTY || m == MAT_WALL || m == MAT_PERM || m == MAT_GLASS || m == MAT_METAL) continue;

            // gravity
            int nx = x, ny = y + 1;
            if (ny < g_gridH && g_grid[gridIndex(nx,ny)] == MAT_EMPTY) {
                g_grid[gridIndex(nx,ny)] = m;
                g_grid[gridIndex(x,y)] = MAT_EMPTY;
                continue;
            }
            // slide diagonally for powder/liquid (slide_powder/slide_liquid)
            bool isLiquid = (m == MAT_WATER || m == MAT_OIL || m == MAT_MERCURY || m == MAT_GAS || m == MAT_PETROL || m == MAT_ACID);
            int dir = (rand() % 2 == 0) ? -1 : 1;
            for (int attempt = 0; attempt < 2; ++attempt) {
                int sx = x + dir;
                if (sx >= 0 && sx < g_gridW && ny < g_gridH && g_grid[gridIndex(sx,ny)] == MAT_EMPTY) {
                    g_grid[gridIndex(sx,ny)] = m;
                    g_grid[gridIndex(x,y)] = MAT_EMPTY;
                    break;
                }
                dir = -dir;
            }
            // gas rises
            if (m == MAT_GAS && y > 0 && g_grid[gridIndex(x,y-1)] == MAT_EMPTY) {
                g_grid[gridIndex(x,y-1)] = m;
                g_grid[gridIndex(x,y)] = MAT_EMPTY;
            }

            // fire consumes flammable (process_transistion)
            if (m == MAT_WOOD || m == MAT_COAL || m == MAT_OIL || m == MAT_WAX || m == MAT_PLANT || m == MAT_SAWDUST) {
                int hx = 0;
                for (int dy=-1; dy<=1; ++dy) for (int dx=-1; dx<=1; ++dx) {
                    int xx=x+dx, yy=y+dy;
                    if (!inBounds(xx,yy)) continue;
                    if (g_grid[gridIndex(xx,yy)] == MAT_FIRE || g_heat[gridIndex(xx,yy)] > 10) hx++;
                }
                if (hx > 0 && (rand()%100) < 5) g_grid[gridIndex(x,y)] = MAT_FIRE;
            }
            if (m == MAT_ICE && g_heat[gridIndex(x,y)] > 5) g_grid[gridIndex(x,y)] = MAT_WATER;
            if (m == MAT_WATER && g_heat[gridIndex(x,y)] > 12) g_grid[gridIndex(x,y)] = MAT_GAS;
        }
    }

    // life rule (Conway)
    {
        std::vector<uint8_t> next = g_grid;
        for (int y=1; y<g_gridH-1; ++y) for (int x=1; x<g_gridW-1; ++x) {
            if (g_grid[gridIndex(x,y)] != MAT_LIFE) continue;
            int cnt=0;
            for (int dy=-1; dy<=1; ++dy) for (int dx=-1; dx<=1; ++dx) if(dx||dy) {
                if (g_grid[gridIndex(x+dx, y+dy)] == MAT_LIFE) cnt++;
            }
            if (cnt < 2 || cnt > 3) next[gridIndex(x,y)] = MAT_EMPTY;
        }
        g_grid.swap(next);
    }
}

// ---------------------------------------------------------------------------
// Command handling – mirrors run_command() switch from decompiled x86_64.
// Command IDs: 1=KILL,2=MOUSE_DOWN,3=MOUSE_MOVE,4=MOUSE_UP,5=SET_MAT,6=ROTATE,
// 7=SLOWMO,8=BORDERS,9=ZOOM_IN,10=ZOOM_OUT,11=RESET,12=CLEAR,13=SPOUTS,14=CURSOR,
// 15=DRAW_LINE,16=BACKGROUND,17=RANDOM_MAT,18=KILL_MODE,19=VIEW_MODE,20=BRUSH,
// 22=RESTORE,23=SAVE
// ---------------------------------------------------------------------------

void process_command(const Command& c) {
    int cmd = (unsigned char)c.id;
    switch (cmd) {
        case 1: // KILL
            g_running = false;
            LOGI("CMD KILL");
            break;
        case 2: // MOUSE_DOWN
            g_cursorX = c.x / g_zoom;
            g_cursorY = c.y / g_zoom;
            g_lastDrawX = g_cursorX;
            g_lastDrawY = g_cursorY;
            draw_brush(g_cursorX, g_cursorY, (uint8_t)g_selectedMat);
            break;
        case 3: // MOUSE_MOVE
            {
                int nx = c.x / g_zoom;
                int ny = c.y / g_zoom;
                draw_line(g_lastDrawX, g_lastDrawY, nx, ny, (uint8_t)g_selectedMat);
                g_cursorX = nx; g_cursorY = ny;
                g_lastDrawX = nx; g_lastDrawY = ny;
            }
            break;
        case 4: // MOUSE_UP
            {
                int nx = c.x / g_zoom;
                int ny = c.y / g_zoom;
                draw_line(g_lastDrawX, g_lastDrawY, nx, ny, (uint8_t)g_selectedMat);
                g_lastDrawX = -1;
            }
            break;
        case 5: // SET_MAT
            g_selectedMat = (unsigned char)c.x;
            LOGI("select mat %d", g_selectedMat);
            break;
        case 6: // ROTATE
            // orientation listener – no grid rotation in simplified model
            break;
        case 7: // SLOWMO
            g_slowMo = !g_slowMo;
            break;
        case 8: // BORDERS
            g_borders = !g_borders;
            break;
        case 9: // ZOOM
            g_zoom = c.x;
            if (g_zoom < 1) g_zoom = 1;
            if (g_zoom > 6) g_zoom = 6;
            LOGI("zoom %d", g_zoom);
            // Re-init grid size based on window
            if (g_window) {
                int w = ANativeWindow_getWidth(g_window) / g_zoom;
                int h = ANativeWindow_getHeight(g_window) / g_zoom;
                if (w > 0 && h > 0) init_simulation(w, h);
            }
            break;
        case 11: // RESET
            clear_simulation();
            init_simulation(g_gridW, g_gridH);
            break;
        case 12: // CLEAR
            clear_simulation();
            break;
        case 13: // SPOUTS
            g_spouts = (c.x != 0);
            break;
        case 14: // CURSOR (tap preview)
            g_cursorX = c.x / g_zoom;
            g_cursorY = c.y / g_zoom;
            break;
        case 15: // DRAW_LINE
            draw_line(g_cursorX, g_cursorY, c.x / g_zoom, c.y / g_zoom, (uint8_t)g_selectedMat);
            break;
        case 16: // BACKGROUND
            g_background = c.x;
            break;
        case 18: // KILL MODE
            if (c.x == 0) g_running = false;
            else if (c.x == 1) g_paused = true;
            break;
        case 19: // VIEW MODE
            g_viewMode = c.x;
            break;
        case 20: // BRUSH SIZE
            g_brushSize = c.x;
            if (g_brushSize < 1) g_brushSize = 1;
            if (g_brushSize > 20) g_brushSize = 20;
            break;
        case 22: // RESTORE
            // no-op in minimal port (original restore_box)
            break;
        case 23: // SAVE
            break;
        default:
            LOGD("unhandled cmd %d x=%d y=%d", cmd, c.x, c.y);
            break;
    }
}

void push_command(char c, short x, short y, bool kill) {
    std::lock_guard<std::mutex> lock(g_cmdMutex);
    if (g_cmdQueue.size() > 200) return; // backpressure like original unk_9ABD40[50]
    g_cmdQueue.push({c, x, y, kill});
    if (kill) {
        // kill commands are processed immediately (mirrors original run_command path when a6 !=0)
        // we handle them inline to unblock main loop quickly
    }
}

// ---------------------------------------------------------------------------
// Rendering – ANativeWindow lock/unlock, draws grid to buffer.
// Matches original ANativeWindow_lock / unlockAndPost flow in main_loop().
// ---------------------------------------------------------------------------

void render_frame() {
    if (!g_window) return;
    ANativeWindow_Buffer buf;
    if (ANativeWindow_lock(g_window, &buf, nullptr) != 0) return;

    int winW = buf.width;
    int winH = buf.height;
    uint32_t* pixels = (uint32_t*)buf.bits;
    int stride = buf.stride;

    // Background
    uint32_t bg = 0xFF000000;
    if (g_background == 21130) bg = 0xFF2B2B2B; // grey
    else if (g_background == 24588) bg = 0xFF3D1F5C; // purple
    else if (g_background == 12) bg = 0xFF0A1A2F; // blue

    for (int y = 0; y < winH; ++y) {
        for (int x = 0; x < winW; ++x) {
            int gx = x / g_zoom;
            int gy = y / g_zoom;
            uint32_t col = bg;
            if (gx >=0 && gx < g_gridW && gy >=0 && gy < g_gridH) {
                uint8_t m = g_grid[gridIndex(gx, gy)];
                if (m != MAT_EMPTY) col = materialColor(m);
                // view mode heat overlay
                if (g_viewMode == 1 && g_heat[gridIndex(gx,gy)] > 0) {
                    col = 0xFFFF0000 | (g_heat[gridIndex(gx,gy)] * 10);
                }
            }
            // borders
            if (g_borders && (gx==0 || gy==0 || gx==g_gridW-1 || gy==g_gridH-1))
                col = 0xFF555555;
            pixels[y * stride + x] = col;
        }
    }

    ANativeWindow_unlockAndPost(g_window);
}

// ---------------------------------------------------------------------------
// Engine thread – equivalent to main_loop() decompiled.
// ---------------------------------------------------------------------------

void* engine_thread_func(void* arg) {
    JNIEnv* env = nullptr;
    if (g_vm) g_vm->AttachCurrentThread(&env, nullptr);

    // Callback to Java: Engine.init_callback()
    if (env && g_engineClazz && g_initCallback) {
        env->CallStaticVoidMethod(g_engineClazz, g_initCallback);
        if (env->ExceptionCheck()) env->ExceptionClear();
        LOGI("init_callback dispatched");
    }

    LOGI("engine thread start grid=%dx%d", g_gridW, g_gridH);
    while (g_running) {
        if (g_paused) {
            usleep(50000);
            continue;
        }

        // drain command queue
        {
            std::lock_guard<std::mutex> lock(g_cmdMutex);
            while (!g_cmdQueue.empty()) {
                Command c = g_cmdQueue.front(); g_cmdQueue.pop();
                if (c.kill) {
                    process_command(c);
                } else {
                    process_command(c);
                }
            }
        }

        simulate_step();
        render_frame();

        if (g_slowMo) usleep(80000);
        else usleep(16000);
    }

    if (g_window) {
        ANativeWindow_release(g_window);
        g_window = nullptr;
    }
    if (g_vm) g_vm->DetachCurrentThread();
    LOGI("engine thread exit");
    return nullptr;
}

// ---------------------------------------------------------------------------
// JNI entry points – signatures must match Engine.java native declarations.
// Original libsand.so exposes Java_smellymoo_sand_Engine_init(Surface, char,
// boolean, boolean) and command(char, short, short, boolean).
// We faithfully reimplement the branching observed in decompiled
// Java_smellymoo_sand_Engine_init (a5 !=0 path = surface update, a5==0 = new thread).
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL Java_smellymoo_sand_Engine_init(JNIEnv *env, jclass clazz, jobject surface, jchar zoom, jboolean isResume1, jboolean isResume2) {
    if (g_vm == nullptr) env->GetJavaVM(&g_vm);

    // Cache Engine class and init_callback method (mirrors original lookup of "init_callback")
    if (g_engineClazz == nullptr) {
        jclass local = env->FindClass("smellymoo/sand/Engine");
        if (local) {
            g_engineClazz = (jclass)env->NewGlobalRef(local);
            g_initCallback = env->GetStaticMethodID(g_engineClazz, "init_callback", "()V");
            LOGI("cached Engine class %p callback %p", g_engineClazz, g_initCallback);
        }
    }

    // isResume1 == true => surface update path (original: a5 !=0)
    if (isResume1) {
        LOGI("Engine_init surface update zoom=%d resume2=%d", (int)zoom, (int)isResume2);
        if (g_window) ANativeWindow_release(g_window);
        g_window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
        if (g_window) {
            ANativeWindow_setBuffersGeometry(g_window, 0, 0, WINDOW_FORMAT_RGBX_8888);
            g_zoom = (zoom != 0) ? zoom : g_zoom;
            if (!isResume2) {
                // queue zoom change like original sends command 10 with zoom
                push_command(10, (short)g_zoom, 0, false);
            }
        }
        if (isResume2) g_paused = false;
        return;
    }

    // New engine thread path
    LOGI("Engine_init new thread zoom=%d", (int)zoom);
    if (g_running && g_threadStarted) {
        // reuse path: original would init(surface, zoom, true, false)
        if (g_window) ANativeWindow_release(g_window);
        g_window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
        if (g_window) ANativeWindow_setBuffersGeometry(g_window, 0, 0, WINDOW_FORMAT_RGBX_8888);
        g_zoom = zoom ? zoom : g_zoom;
        g_paused = false;
        return;
    }

    // Initial creation
    if (g_window) ANativeWindow_release(g_window);
    g_window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
    if (g_window) ANativeWindow_setBuffersGeometry(g_window, 0, 0, WINDOW_FORMAT_RGBX_8888);

    g_zoom = (zoom != 0) ? zoom : 3;
    g_background = 0;
    g_borders = true;
    g_paused = false;
    g_running = true;

    int w = 160, h = 240;
    if (g_window) {
        w = ANativeWindow_getWidth(g_window) / g_zoom;
        h = ANativeWindow_getHeight(g_window) / g_zoom;
        if (w <=0) w=160;
        if (h <=0) h=240;
    }
    init_simulation(w, h);

    // notify Java via callback will be done in thread; also start thread
    pthread_create(&g_thread, nullptr, engine_thread_func, nullptr);
    g_threadStarted = true;
}

extern "C" JNIEXPORT void JNICALL Java_smellymoo_sand_Engine_command(JNIEnv *env, jclass clazz, jchar cmd, jshort x, jshort y, jboolean isKill) {
    // isKill true => immediate run_command (original a6 !=0 path)
    if (isKill) {
        Command c{(char)cmd, x, y, true};
        process_command(c);
    } else {
        push_command((char)cmd, x, y, false);
    }
}

// JNI OnLoad – optional
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_vm = vm;
    LOGI("JNI_OnLoad ProjectSand rebuilt via REA");
    return JNI_VERSION_1_6;
}
