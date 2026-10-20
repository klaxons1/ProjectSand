package smellymoo.sand;

import android.app.AlarmManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.Surface;

/* JADX INFO: loaded from: C:\Users\pashansky2005\Downloads\smellymoo.sand\classes.dex */
public class Engine {
    public static final int DEBUG = 3;
    public static final int DIAG = 2;
    public static final int FIRST = 1;
    public static final int INIT = 1;
    public static final int NONE = 3;
    public static final int NORM = 0;
    public static final int ONE_SIM = 4;
    public static final int REMAIN = 2;
    public static final int RUN = 0;
    public static final int TEST = 5;
    private static AlarmManager alarm_mgr;
    private static Context app_context;
    private static short background;
    private static boolean dev;
    private static int engine_mode;
    private static Thread native_thread;
    private static SharedPreferences prefs;
    private static boolean spouts;
    private static Surface surface;
    private static int surface_num;
    private static short view_mode;
    private static char zoom;

    private static native void command(char c, short s, short s2, boolean z);

    static native String configureCrashLogging(String preferredDirectory,
                                               String fallbackDirectory);

    static native void prepareCrashSignalStack();

    static native void checkNativeEngineProgress();

    /* JADX INFO: Access modifiers changed from: private */
    private static native void init(Surface surface2, char c, boolean z, boolean z2);

    static {
        System.loadLibrary("sand");
        prefs = null;
        alarm_mgr = null;
        dev = false;
        engine_mode = 0;
        surface_num = 0;
        app_context = null;
        surface = null;
        native_thread = null;
        background = (short) 0;
        spouts = false;
        zoom = (char) 3;
        view_mode = (short) 0;
    }

    public static void enqueue(int c) {
        if (sim_running()) {
            command((char) c, (short) 0, (short) 0, false);
        }
    }

    public static void enqueue(char c) {
        if (sim_running()) {
            command(c, (short) 0, (short) 0, false);
        }
    }

    public static void enqueue(int c, int data) {
        if (sim_running()) {
            command((char) c, (short) data, (short) 0, false);
        }
    }

    public static void enqueue(char c, short x, short y) {
        if (sim_running()) {
            command(c, x, y, false);
        }
    }

    public static void enqueue(int c, int x, int y) {
        if (sim_running()) {
            command((char) c, (short) x, (short) y, false);
        }
    }

    public static void send_kill(int mode) {
        if (sim_running()) {
            command((char) 18, (short) mode, (short) 0, true);
        }
    }

    public static void set_context(Context app_c) {
        app_context = app_c;
    }

    public static int construct_engine(Context app_c) {
        app_context = app_c;
        CrashLogManager.initialize(app_context);
        prefs = PreferenceManager.getDefaultSharedPreferences(app_context);
        engine_mode = prefs.getInt("mode", 1);
        dev = prefs.getBoolean("dev", false);
        if (engine_mode == 1) {
            set_engine_mode(0);
        } else if (engine_mode == 5) {
            if (prefs.getBoolean("dev", false)) {
                set_engine_mode(0);
            } else {
                int last_state = prefs.getInt("diagnose", 3);
                engine_mode = (last_state == 0 || last_state == 3) ? 5 : 2;
                if (engine_mode == 5) {
                    diagnostic("TEST_RUN", 1);
                }
            }
        }
        return engine_mode;
    }

    public static void settings(char set_zoom, short set_background, boolean set_spouts) {
        background = set_background;
        spouts = set_spouts;
        zoom = set_zoom;
    }

    public static void setting_spouts(boolean enabled) {
        spouts = enabled;
        if (sim_running()) {
            command('\r', (short) (spouts ? 1 : 0), (short) 0, true);
        }
    }

    public static void setting_view(short mode) {
        view_mode = mode;
        if (sim_running()) {
            command((char) 19, view_mode, (short) 0, true);
        }
    }

    public static void setting_zoom(int set_zoom) {
        zoom = (char) set_zoom;
        if (sim_running()) {
            enqueue(9, set_zoom);
        }
    }

    public static void setting_brushsize(int size) {
        if (sim_running()) {
            enqueue(20, size);
        }
    }

    public static int runs(boolean inc, boolean menu) {
        if (prefs == null) {
            return 0;
        }
        int runs = prefs.getInt("runs", 0);
        if (inc) {
            prefs.edit().putInt("runs", runs + 1).commit();
        }
        if (menu) {
            if (prefs.getBoolean("diag_shown", false) || prefs.getBoolean("rated", false)) {
                return 0;
            }
            return runs;
        }
        return runs;
    }

    public static void remain_active() {
        surface_num = 0;
    }

    public static void init(Surface sim_surface, int from) {
        surface_num = from;
        surface = sim_surface;
        if (!sim_running()) {
            diagnostic("NEW_THREAD", 1);
            native_thread = new engine_thread(from);
            native_thread.start();
        } else {
            diagnostic("REUSE_THREAD", 2);
            init(surface, from == 1 ? (char) 3 : zoom, true, false);
        }
    }

    public static void resume(int from) {
        if (sim_running() && from == surface_num) {
            diagnostic("RESUME");
            init(surface, (char) 0, true, true);
        }
    }

    public static void pause(int from) {
        if (sim_running() && from == surface_num) {
            diagnostic("PAUSE");
            send_kill(1);
        }
    }

    public static void kill(int from) {
        if (sim_running() && from == surface_num) {
            diagnostic("KILL");
            send_kill(0);
            surface_num = 0;
            return;
        }
        diagnostic("DIDNT_KILL");
    }

    public static void init_callback() {
        diagnostic("INIT_CALLBACK", 0);
        if (surface_num == 1) {
            command((char) 19, (short) 0, (short) 0, true);
            enqueue(16, 0);
            enqueue(13, 1);
            enqueue(6, 0);
            return;
        }
        if (background != 0) {
            enqueue(16, background);
        }
        enqueue(13, spouts ? 1 : 0);
    }

    private static boolean sim_running() {
        if (native_thread == null) {
            return false;
        }
        return native_thread.isAlive();
    }

    public static void set_engine_mode(int mode) {
        if (prefs != null) {
            prefs.edit().putInt("mode", mode).commit();
        }
        engine_mode = mode;
    }

    public static int get_engine_mode() {
        return engine_mode;
    }

    private static void diagnostic(String msg) {
        if (dev) {
            Log.d("java_sand", msg);
        }
        if ((engine_mode == 1 || engine_mode == 5) && prefs != null) {
            String trace = prefs.getString("trace", "");
            prefs.edit().putString("trace", String.valueOf(trace) + " " + msg).commit();
        }
    }

    public static void diagnostic(String msg, int state) {
        if (prefs != null) {
            prefs.edit().putInt("diagnose", state).commit();
        }
        diagnostic(msg);
    }

    static class engine_thread extends Thread {
        private int from;

        public engine_thread(int init_from) {
            this.from = init_from;
        }

        @Override // java.lang.Thread, java.lang.Runnable
        public void run() {
            Engine.prepareCrashSignalStack();
            Engine.init(Engine.surface, this.from == 1 ? (char) 3 : Engine.zoom, false, false);
        }
    }
}
