package smellymoo.sand;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.View;
import android.widget.CheckBox;
import android.widget.RadioGroup;

/* JADX INFO: loaded from: C:\Users\pashansky2005\Downloads\smellymoo.sand\classes.dex */
public class Diagnostic extends Activity {
    boolean option_clicked = false;

    @Override // android.app.Activity
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(1);
        setContentView(R.layout.diagnostic);
    }

    @Override // android.app.Activity
    protected void onDestroy() {
        if (!this.option_clicked) {
            SharedPreferences.Editor prefs_edit = PreferenceManager.getDefaultSharedPreferences(this).edit();
            prefs_edit.putInt("diagnose", 3);
            prefs_edit.putBoolean("diag_shown", true);
            prefs_edit.commit();
        }
        super.onDestroy();
    }

    @Override // android.app.Activity
    public void onBackPressed() {
    }

    public void checkbox(View view) {
        CheckBox toggle = (CheckBox) view;
        toggle.toggle();
    }

    @SuppressLint({"NewApi"})
    public void click(View view) {
        String report_text;
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
        RadioGroup report_option = (RadioGroup) findViewById(R.id.report_option);
        this.option_clicked = true;
        int engine_mode = 4;
        int last_state = prefs.getInt("diagnose", 3);
        switch (report_option.getCheckedRadioButtonId()) {
            case R.id.report_none /* 2131034127 */:
                report_text = "none";
                engine_mode = 0;
                break;
            case R.id.report_menu /* 2131034128 */:
                report_text = "menu";
                break;
            case R.id.report_sim /* 2131034129 */:
                report_text = "init_sim";
                break;
            case R.id.report_late /* 2131034130 */:
                report_text = "use_sim";
                break;
            default:
                report_text = "?";
                break;
        }
        SharedPreferences.Editor prefs_edit = prefs.edit();
        prefs_edit.putInt("mode", engine_mode);
        prefs_edit.putInt("diagnose", 0);
        prefs_edit.putBoolean("diag_shown", true);
        prefs_edit.commit();
        switch (view.getId()) {
            case R.id.fix_button /* 2131034131 */:
                if (engine_mode != 4) {
                    Intent intent = new Intent(this, (Class<?>) Main.class);
                    startActivity(Intent.makeRestartActivityTask(intent.getComponent()));
                } else {
                    finish();
                }
                break;
            case R.id.email_button /* 2131034132 */:
                String debug = " report=" + report_text + " state=" + last_state + "\ntrace='" + prefs.getString("trace", "?") + "'";
                Helper.email(this, "fault", "<" + getResources().getString(R.string.add_info) + ">", debug, false);
                break;
        }
    }
}
