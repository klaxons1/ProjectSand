package smellymoo.sand;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;

/* JADX INFO: loaded from: C:\Users\pashansky2005\Downloads\smellymoo.sand\classes.dex */
public class Freemium extends Activity {
    short cheat = 0;

    @Override // android.app.Activity
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(1);
        setContentView(C0000R.layout.freemium);
    }

    @Override // android.app.Activity
    public void onBackPressed() {
        finish();
    }

    /* JADX WARN: Can't fix incorrect switch cases order, some code will duplicate */
    public void cheat(View view) {
        switch (view.getId()) {
            case C0000R.id.ex1 /* 2131034188 */:
                this.cheat = (short) (this.cheat ^ 1);
                this.cheat = (short) (this.cheat ^ 2);
                this.cheat = (short) (this.cheat ^ 4);
                this.cheat = (short) (this.cheat ^ 8);
                this.cheat = (short) (this.cheat ^ 16);
                this.cheat = (short) (this.cheat ^ 32);
                this.cheat = (short) (this.cheat ^ 64);
                this.cheat = (short) (this.cheat ^ 128);
                break;
            case C0000R.id.ex2 /* 2131034189 */:
                this.cheat = (short) (this.cheat ^ 2);
                this.cheat = (short) (this.cheat ^ 4);
                this.cheat = (short) (this.cheat ^ 8);
                this.cheat = (short) (this.cheat ^ 16);
                this.cheat = (short) (this.cheat ^ 32);
                this.cheat = (short) (this.cheat ^ 64);
                this.cheat = (short) (this.cheat ^ 128);
                break;
            case C0000R.id.ex3 /* 2131034190 */:
                this.cheat = (short) (this.cheat ^ 4);
                this.cheat = (short) (this.cheat ^ 8);
                this.cheat = (short) (this.cheat ^ 16);
                this.cheat = (short) (this.cheat ^ 32);
                this.cheat = (short) (this.cheat ^ 64);
                this.cheat = (short) (this.cheat ^ 128);
                break;
            case C0000R.id.ex4 /* 2131034191 */:
                this.cheat = (short) (this.cheat ^ 8);
                this.cheat = (short) (this.cheat ^ 16);
                this.cheat = (short) (this.cheat ^ 32);
                this.cheat = (short) (this.cheat ^ 64);
                this.cheat = (short) (this.cheat ^ 128);
                break;
            case C0000R.id.ex5 /* 2131034193 */:
                this.cheat = (short) (this.cheat ^ 16);
                this.cheat = (short) (this.cheat ^ 32);
                this.cheat = (short) (this.cheat ^ 64);
                this.cheat = (short) (this.cheat ^ 128);
                break;
            case C0000R.id.ex6 /* 2131034194 */:
                this.cheat = (short) (this.cheat ^ 32);
                this.cheat = (short) (this.cheat ^ 64);
                this.cheat = (short) (this.cheat ^ 128);
                break;
            case C0000R.id.ex7 /* 2131034195 */:
                this.cheat = (short) (this.cheat ^ 64);
                this.cheat = (short) (this.cheat ^ 128);
                break;
            case C0000R.id.ex8 /* 2131034196 */:
                this.cheat = (short) (this.cheat ^ 128);
                break;
        }
    }

    /* JADX WARN: Can't fix incorrect switch cases order, some code will duplicate */
    public void unlock(View view) {
        switch (view.getId()) {
            case C0000R.id.freemium_back /* 2131034198 */:
                setResult(0);
                finish();
                setResult(999);
                finish();
                break;
            case C0000R.id.unlock /* 2131034199 */:
                int code = -1;
                if (this.cheat == 99 || this.cheat == 54) {
                    code = 4269;
                } else if (this.cheat == 34) {
                    code = 7337;
                }
                setResult(code);
                finish();
                setResult(0);
                finish();
                setResult(999);
                finish();
                break;
            case C0000R.id.never /* 2131034200 */:
                setResult(999);
                finish();
                break;
        }
    }
}
