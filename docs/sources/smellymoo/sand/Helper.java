package smellymoo.sand;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.preference.PreferenceManager;

/* JADX INFO: loaded from: C:\Users\pashansky2005\Downloads\smellymoo.sand\classes.dex */
public class Helper {
    public static void rate_app(Context context) {
        PreferenceManager.getDefaultSharedPreferences(context).edit().putBoolean("rated", true).commit();
        Uri uri = Uri.parse("market://details?id=" + context.getPackageName());
        Intent goToMarket = new Intent("android.intent.action.VIEW", uri);
        goToMarket.addFlags(1208483840);
        try {
            context.startActivity(goToMarket);
        } catch (ActivityNotFoundException e) {
            context.startActivity(new Intent("android.intent.action.VIEW", Uri.parse("http://play.google.com/store/apps/details?id=" + context.getPackageName())));
        }
    }

    public static void email(Context context) {
        email(context, null, "", "", true);
    }

    public static void email(Context context, String subject) {
        email(context, subject, "", "", true);
    }

    public static void email(Context context, String subject, String start_text, String debug, boolean one_line) {
        try {
            Intent emailIntent = new Intent("android.intent.action.SENDTO", Uri.fromParts("mailto", "smellymoo@gmail.com", null));
            emailIntent.putExtra("android.intent.extra.SUBJECT", "sandbox" + (subject == null ? "" : " " + subject));
            emailIntent.putExtra("android.intent.extra.TEXT", String.valueOf(start_text) + "\n\nDEBUG:" + (one_line ? " " : "\n") + "api=" + Build.VERSION.SDK_INT + " v=" + context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionCode + " r=" + Engine.runs(false, false) + debug);
            context.startActivity(Intent.createChooser(emailIntent, "Send email..."));
        } catch (Exception e) {
        }
    }
}
