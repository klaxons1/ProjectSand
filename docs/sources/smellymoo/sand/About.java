package smellymoo.sand;

import android.app.Activity;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.View;

/* JADX INFO: loaded from: C:\Users\pashansky2005\Downloads\smellymoo.sand\classes.dex */
public class About extends Activity {
    boolean enable_dev = false;

    @Override // android.app.Activity
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(C0000R.layout.about);
    }

    @Override // android.app.Activity
    public void onBackPressed() {
        finish();
    }

    public void click(View view) {
        switch (view.getId()) {
            case C0000R.id.enable_dev /* 2131034113 */:
                this.enable_dev = true;
                break;
            case C0000R.id.about_back /* 2131034118 */:
                finish();
                break;
            case C0000R.id.about_rate /* 2131034119 */:
                if (this.enable_dev) {
                    PreferenceManager.getDefaultSharedPreferences(this).edit().putBoolean("dev", true).commit();
                } else {
                    Helper.rate_app(this);
                }
                finish();
                break;
            case C0000R.id.about_email /* 2131034120 */:
                Helper.email(this);
                finish();
                break;
        }
    }
}
