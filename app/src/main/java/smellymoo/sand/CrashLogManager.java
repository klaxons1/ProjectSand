package smellymoo.sand;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.Process;
import android.provider.Settings;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.nio.charset.Charset;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/** Installs Java and native crash logging before the simulation thread starts. */
final class CrashLogManager {
    private static final String TAG = "SandCrashLog";
    private static final String PUBLIC_DIRECTORY = "/storage/emulated/0/sand";
    private static final String PREFERENCES = "crash_logging";
    private static final String PROMPT_SHOWN = "public_storage_prompt_shown";
    private static final long WATCHDOG_INTERVAL_MS = 5_000L;
    static final int WRITE_PERMISSION_REQUEST = 5182;

    private static Context appContext;
    private static String fallbackDirectory;
    private static volatile String activeDirectory;
    private static boolean javaHandlerInstalled;
    private static boolean nativeLoggerConfigured;
    private static boolean publicAccessAtLastConfiguration;
    private static Thread.UncaughtExceptionHandler previousHandler;
    private static Thread nativeWatchdogThread;

    private CrashLogManager() {}

    static synchronized void initialize(Context context) {
        appContext = context.getApplicationContext();
        fallbackDirectory = new File(appContext.getFilesDir(), "crash-logs").getAbsolutePath();
        new File(fallbackDirectory).mkdirs();
        installJavaHandler();
        if (!nativeLoggerConfigured)
            refreshNativeLogger();
        startNativeWatchdog();
    }

    private static void startNativeWatchdog() {
        if (nativeWatchdogThread != null)
            return;

        // The native checker only records a stall; it never kills or resets the engine.
        nativeWatchdogThread = new Thread(() -> {
            while (!Thread.currentThread().isInterrupted()) {
                try {
                    Thread.sleep(WATCHDOG_INTERVAL_MS);
                    Engine.checkNativeEngineProgress();
                } catch (InterruptedException stopped) {
                    Thread.currentThread().interrupt();
                } catch (Throwable error) {
                    Log.e(TAG, "Native simulation watchdog stopped", error);
                    return;
                }
            }
        }, "SandEngineWatchdog");
        nativeWatchdogThread.setDaemon(true);
        nativeWatchdogThread.start();
    }

    static synchronized void refresh(Context context) {
        if (appContext == null) {
            initialize(context);
            return;
        }
        if (!nativeLoggerConfigured) {
            refreshNativeLogger();
            return;
        }
        boolean publicAccessChanged =
                publicAccessAtLastConfiguration != hasPublicDirectoryAccess();
        if (publicAccessChanged)
            refreshNativeLogger();
    }

    private static boolean hasPublicDirectoryAccess() {
        if (Build.VERSION.SDK_INT >= 30)
            return Environment.isExternalStorageManager();
        if (Build.VERSION.SDK_INT >= 23 && Build.VERSION.SDK_INT <= 28)
            return appContext.checkSelfPermission(
                    android.Manifest.permission.WRITE_EXTERNAL_STORAGE)
                    == android.content.pm.PackageManager.PERMISSION_GRANTED;
        return false;
    }

    private static void refreshNativeLogger() {
        try {
            String chosen = Engine.configureCrashLogging(PUBLIC_DIRECTORY, fallbackDirectory);
            if (chosen != null && !chosen.isEmpty()) {
                activeDirectory = chosen;
                if (!PUBLIC_DIRECTORY.equals(chosen)) {
                    Log.w(TAG, "Public crash folder unavailable; "
                            + "using app-private logs: " + chosen);
                } else {
                    Log.i(TAG, "Native crash logs: " + chosen);
                }
            } else {
                activeDirectory = fallbackDirectory;
                Log.e(TAG, "Native logger could not open a crash log; "
                        + "Java logs will use " + fallbackDirectory);
            }
        } catch (Throwable error) {
            activeDirectory = fallbackDirectory;
            Log.e(TAG, "Unable to configure native crash logging", error);
        } finally {
            nativeLoggerConfigured = true;
            publicAccessAtLastConfiguration = hasPublicDirectoryAccess();
        }
    }

    private static void installJavaHandler() {
        if (javaHandlerInstalled)
            return;
        previousHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((thread, error) -> {
            try {
                writeJavaCrash(thread, error);
            } catch (Throwable loggingError) {
                Log.e(TAG, "Failed to write Java crash report", loggingError);
            }
            if (previousHandler != null) {
                previousHandler.uncaughtException(thread, error);
            } else {
                Process.killProcess(Process.myPid());
                System.exit(10);
            }
        });
        javaHandlerInstalled = true;
    }

    private static void writeJavaCrash(Thread thread, Throwable error) throws Exception {
        String directory = activeDirectory != null ? activeDirectory : fallbackDirectory;
        File logDirectory = new File(directory);
        if ((!logDirectory.exists() && !logDirectory.mkdirs()) || !logDirectory.canWrite()) {
            logDirectory = new File(fallbackDirectory);
            if (!logDirectory.exists() && !logDirectory.mkdirs())
                return;
        }
        String timestamp = new SimpleDateFormat("yyyyMMdd_HHmmss_SSS", Locale.US)
                .format(new Date());
        File report = new File(logDirectory,
                "java_crash_" + timestamp + "_" + Process.myPid() + ".log");
        try (FileOutputStream stream = new FileOutputStream(report);
             OutputStreamWriter output = new OutputStreamWriter(
                     stream, Charset.forName("UTF-8"));
             PrintWriter writer = new PrintWriter(output)) {
            writer.println("=== UNCAUGHT JAVA EXCEPTION ===");
            writer.println("time=" + new Date().toString());
            writer.println("pid=" + Process.myPid());
            writer.println("thread=" + thread.getName());
            error.printStackTrace(writer);
            writer.println("=== END UNCAUGHT JAVA EXCEPTION ===");
            writer.flush();
            stream.getFD().sync();
        }
    }

    /** Offer the Android 11+ special access needed for the requested public folder. */
    static void requestPublicDirectoryAccess(Activity activity) {
        if (Build.VERSION.SDK_INT >= 30) {
            if (Environment.isExternalStorageManager())
                return;
            SharedPreferences preferences = activity.getSharedPreferences(
                    PREFERENCES, Context.MODE_PRIVATE);
            if (preferences.getBoolean(PROMPT_SHOWN, false))
                return;
            preferences.edit().putBoolean(PROMPT_SHOWN, true).apply();
            new AlertDialog.Builder(activity)
                    .setTitle("Crash log folder access")
                    .setMessage("Allow all-files access to write crash reports to "
                            + PUBLIC_DIRECTORY
                            + ". If skipped, reports are saved in the app's private files.")
                    .setPositiveButton("Open settings", (dialog, which) -> {
                        Intent intent = new Intent(
                                Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                                Uri.parse("package:" + activity.getPackageName()));
                        try {
                            activity.startActivity(intent);
                        } catch (RuntimeException unavailable) {
                            Intent generalSettings = new Intent(
                                    Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                            try {
                                activity.startActivity(generalSettings);
                            } catch (RuntimeException ignored) {
                                Log.w(TAG, "All-files access settings are unavailable",
                                        unavailable);
                            }
                        }
                    })
                    .setNegativeButton("Use private fallback", (dialog, which) ->
                            Log.i(TAG, "Crash reports will use app-private storage"))
                    .show();
            return;
        }

        if (Build.VERSION.SDK_INT >= 23 && Build.VERSION.SDK_INT <= 28
                && activity.checkSelfPermission(
                        android.Manifest.permission.WRITE_EXTERNAL_STORAGE)
                        != android.content.pm.PackageManager.PERMISSION_GRANTED) {
            SharedPreferences preferences = activity.getSharedPreferences(
                    PREFERENCES, Context.MODE_PRIVATE);
            if (!preferences.getBoolean(PROMPT_SHOWN, false)) {
                preferences.edit().putBoolean(PROMPT_SHOWN, true).apply();
                activity.requestPermissions(
                        new String[]{android.Manifest.permission.WRITE_EXTERNAL_STORAGE},
                        WRITE_PERMISSION_REQUEST);
            }
        }
    }
}
