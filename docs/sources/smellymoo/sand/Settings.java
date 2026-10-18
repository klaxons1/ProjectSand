package smellymoo.sand;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.View;
import android.widget.CheckedTextView;
import android.widget.RadioButton;
import android.widget.RadioGroup;

/* JADX INFO: loaded from: C:\Users\pashansky2005\Downloads\smellymoo.sand\classes.dex */
public class Settings extends Activity {
    @Override // android.app.Activity
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(C0000R.layout.settings);
        if (PreferenceManager.getDefaultSharedPreferences(this).getBoolean("dev", false)) {
            ((RadioButton) findViewById(C0000R.id.option_toolbox_color)).setEnabled(true);
            ((RadioButton) findViewById(C0000R.id.option_toolbox_one)).setEnabled(true);
            ((RadioButton) findViewById(C0000R.id.option_engine_diag)).setVisibility(0);
            ((RadioButton) findViewById(C0000R.id.option_engine_first)).setEnabled(true);
            ((RadioButton) findViewById(C0000R.id.option_engine_debug)).setEnabled(true);
        }
    }

    @Override // android.app.Activity
    public void onBackPressed() {
        finish();
    }

    public void checkbox(View view) {
        CheckedTextView toggle = (CheckedTextView) view;
        toggle.toggle();
    }

    @SuppressLint({"NewApi"})
    public void click(View view) {
        int engine_mode;
        int toolbox_theme;
        int background;
        switch (view.getId()) {
            case C0000R.id.back_button /* 2131034122 */:
                finish();
                break;
            case C0000R.id.save_button /* 2131034237 */:
                SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
                RadioGroup option_engine = (RadioGroup) findViewById(C0000R.id.option_engine);
                RadioGroup option_toolbox = (RadioGroup) findViewById(C0000R.id.option_toolbox);
                RadioGroup option_background = (RadioGroup) findViewById(C0000R.id.option_background);
                CheckedTextView option_show_pro = (CheckedTextView) findViewById(C0000R.id.option_show_pro);
                int last_state = prefs.getInt("diagnose", 3);
                switch (option_engine.getCheckedRadioButtonId()) {
                    case C0000R.id.option_engine_onesim /* 2131034233 */:
                        engine_mode = 4;
                        break;
                    case C0000R.id.option_engine_first /* 2131034234 */:
                        engine_mode = 5;
                        last_state = 0;
                        break;
                    case C0000R.id.option_engine_diag /* 2131034235 */:
                        engine_mode = 2;
                        break;
                    case C0000R.id.option_engine_debug /* 2131034236 */:
                        engine_mode = 3;
                        break;
                    default:
                        engine_mode = 0;
                        break;
                }
                switch (option_toolbox.getCheckedRadioButtonId()) {
                    case C0000R.id.option_toolbox_one /* 2131034229 */:
                        toolbox_theme = 1;
                        break;
                    case C0000R.id.option_toolbox_color /* 2131034230 */:
                        toolbox_theme = 2;
                        break;
                    default:
                        toolbox_theme = 0;
                        break;
                }
                switch (option_background.getCheckedRadioButtonId()) {
                    case C0000R.id.option_background_grey /* 2131034224 */:
                        background = 21130;
                        break;
                    case C0000R.id.option_background_purple /* 2131034225 */:
                        background = 24588;
                        break;
                    case C0000R.id.option_background_blue /* 2131034226 */:
                        background = 12;
                        break;
                    default:
                        background = 0;
                        break;
                }
                SharedPreferences.Editor prefs_edit = prefs.edit();
                prefs_edit.putInt("mode", engine_mode);
                prefs_edit.putInt("diagnose", last_state);
                prefs_edit.putInt("theme", toolbox_theme);
                prefs_edit.putInt("sim_color", background);
                prefs_edit.putBoolean("never_unlock", !option_show_pro.isChecked());
                prefs_edit.commit();
                if (Build.VERSION.SDK_INT > 10) {
                    Intent intent = new Intent(this, (Class<?>) Main.class);
                    startActivity(Intent.makeRestartActivityTask(intent.getComponent()));
                } else {
                    finish();
                }
                break;
        }
    }
}
