package smellymoo.sand;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.View;
import android.widget.CheckedTextView;
import android.widget.SeekBar;

/* JADX INFO: loaded from: C:\Users\pashansky2005\Downloads\smellymoo.sand\classes.dex */
public class Settings_popup extends Activity {
    @Override // android.app.Activity
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(1);
        setContentView(R.layout.settings_popup);
    }

    @Override // android.app.Activity
    public void onBackPressed() {
        finish();
    }

    public void checkbox(View view) {
        CheckedTextView toggle = (CheckedTextView) view;
        toggle.toggle();
    }

    public void click(View view) {
        switch (view.getId()) {
            case R.id.popup_back /* 2131034240 */:
                finish();
                break;
            case R.id.popup_save /* 2131034241 */:
                SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
                CheckedTextView show_pro = (CheckedTextView) findViewById(R.id.show_premium);
                SeekBar brush = (SeekBar) findViewById(R.id.brush_size);
                SharedPreferences.Editor prefs_edit = prefs.edit();
                prefs_edit.putBoolean("never_unlock", !show_pro.isChecked());
                prefs_edit.putInt("brush_size", brush.getProgress() + 1);
                prefs_edit.commit();
                setResult(888);
                finish();
                break;
        }
    }
}
