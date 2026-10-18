package smellymoo.sand;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.webkit.WebView;
import java.util.Locale;

/* JADX INFO: loaded from: C:\Users\pashansky2005\Downloads\smellymoo.sand\classes.dex */
public class Help extends Activity {
    @Override // android.app.Activity
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(C0000R.layout.help);
        WebView web_view = (WebView) findViewById(C0000R.id.help_html);
        if (!Locale.getDefault().getLanguage().equals("ko")) {
            web_view.loadUrl("file:///android_asset/help.html");
        } else {
            web_view.loadUrl("file:///android_asset/help_ko.html");
        }
    }

    @Override // android.app.Activity
    public void onBackPressed() {
        finish();
    }

    public void click(View view) {
        switch (view.getId()) {
            case C0000R.id.help_back /* 2131034205 */:
                finish();
                break;
            case C0000R.id.help_email /* 2131034206 */:
                Intent emailIntent = new Intent("android.intent.action.SENDTO", Uri.fromParts("mailto", "smellymoo@gmail.com", null));
                emailIntent.putExtra("android.intent.extra.SUBJECT", "Sandbox");
                startActivity(Intent.createChooser(emailIntent, "Send email..."));
                break;
        }
    }
}
