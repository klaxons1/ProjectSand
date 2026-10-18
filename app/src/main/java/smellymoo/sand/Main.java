package smellymoo.sand;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

/* JADX INFO: loaded from: C:\Users\pashansky2005\Downloads\smellymoo.sand\classes.dex */
public class Main extends Activity implements SurfaceHolder.Callback {
    static Surface sand_surface = null;
    private static final int surface_num = 1;

    @Override // android.app.Activity
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main_menu);
        SurfaceView sand_view = (SurfaceView) findViewById(R.id.sand_view);
        SurfaceHolder holder = sand_view.getHolder();
        holder.addCallback(this);
    }

    /* JADX WARN: Can't fix incorrect switch cases order, some code will duplicate */
    /* JADX WARN: Code duplicated, block: B:16:0x00a2  */
    /* JADX WARN: Code duplicated, block: B:27:0x003c A[EXC_TOP_SPLITTER, SYNTHETIC] */
    /* JADX WARN: Code duplicated, block: B:31:? A[RETURN, SYNTHETIC] */
    /* JADX WARN: Code duplicated, block: B:7:0x001f  */
    @Override // android.view.SurfaceHolder.Callback
    public void surfaceCreated(SurfaceHolder holder) {
        Canvas canvas;
        int mode = Engine.construct_engine(getApplicationContext());
        switch (mode) {
            case 2:
                startActivity(new Intent(this, (Class<?>) Diagnostic.class));
                canvas = null;
                try {
                    canvas = holder.lockCanvas();
                    synchronized (holder) {
                        try {
                            Bitmap icon = BitmapFactory.decodeResource(getResources(), R.drawable.smellymoo);
                            Bitmap safe = BitmapFactory.decodeResource(getResources(), R.drawable.safe_mode);
                            canvas.drawColor(-7829368);
                            Rect frame = holder.getSurfaceFrame();
                            int margin_x = (frame.right - 288) / 2;
                            int margin_y = (frame.bottom - 288) / 2;
                            Rect dest = new Rect(margin_x, margin_y, frame.right - margin_x, frame.bottom - margin_y);
                            canvas.drawBitmap(icon, (Rect) null, dest, new Paint());
                            int margin_x2 = (frame.right - 232) / 2;
                            Rect dest_safe = new Rect(margin_x2, (frame.bottom - margin_y) + 5, frame.right - margin_x2, (frame.bottom - margin_y) + 32 + 5);
                            canvas.drawBitmap(safe, (Rect) null, dest_safe, new Paint());
                        } catch (Throwable th) {
                            throw th;
                        }
                        break;
                    }
                    if (canvas != null) {
                        holder.unlockCanvasAndPost(canvas);
                    }
                } catch (Exception e) {
                    if (canvas != null) {
                        holder.unlockCanvasAndPost(canvas);
                    }
                } catch (Throwable th2) {
                    if (canvas != null) {
                        holder.unlockCanvasAndPost(canvas);
                    }
                    throw th2;
                }
                if (Engine.runs(true, true) >= 15) {
                    findViewById(R.id.menu_rate).setVisibility(0);
                    return;
                }
                return;
            case 3:
            default:
                sand_surface = holder.getSurface();
                Engine.init(sand_surface, 1);
                if (Engine.runs(true, true) >= 15) {
                    findViewById(R.id.menu_rate).setVisibility(0);
                    return;
                }
                return;
            case Engine.ONE_SIM /* 4 */:
                canvas = null;
                canvas = holder.lockCanvas();
                synchronized (holder) {
                    Bitmap icon2 = BitmapFactory.decodeResource(getResources(), R.drawable.smellymoo);
                    Bitmap safe2 = BitmapFactory.decodeResource(getResources(), R.drawable.safe_mode);
                    canvas.drawColor(-7829368);
                    Rect frame2 = holder.getSurfaceFrame();
                    int margin_x3 = (frame2.right - 288) / 2;
                    int margin_y2 = (frame2.bottom - 288) / 2;
                    Rect dest2 = new Rect(margin_x3, margin_y2, frame2.right - margin_x3, frame2.bottom - margin_y2);
                    canvas.drawBitmap(icon2, (Rect) null, dest2, new Paint());
                    int margin_x4 = (frame2.right - 232) / 2;
                    Rect dest_safe2 = new Rect(margin_x4, (frame2.bottom - margin_y2) + 5, frame2.right - margin_x4, (frame2.bottom - margin_y2) + 32 + 5);
                    canvas.drawBitmap(safe2, (Rect) null, dest_safe2, new Paint());
                    if (canvas != null) {
                        holder.unlockCanvasAndPost(canvas);
                    }
                    if (Engine.runs(true, true) >= 15) {
                        findViewById(R.id.menu_rate).setVisibility(0);
                        return;
                    }
                    return;
                }
        }
    }

    @Override // android.view.SurfaceHolder.Callback
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override // android.view.SurfaceHolder.Callback
    public void surfaceDestroyed(SurfaceHolder holder) {
        Engine.kill(1);
    }

    @Override // android.app.Activity
    protected void onPause() {
        super.onPause();
        Engine.pause(1);
    }

    @Override // android.app.Activity
    protected void onResume() {
        super.onResume();
        Engine.resume(1);
    }

    public void menu_button(View view) {
        switch (view.getId()) {
            case R.id.menu_sandbox /* 2131034207 */:
                Engine.remain_active();
                startActivity(new Intent(this, (Class<?>) Sand.class));
                break;
            case R.id.menu_editor /* 2131034209 */:
                Engine.remain_active();
                startActivity(new Intent(this, (Class<?>) Editor.class));
                break;
            case R.id.menu_settings /* 2131034210 */:
                startActivity(new Intent(this, (Class<?>) Settings.class));
                break;
            case R.id.menu_rate /* 2131034211 */:
                Helper.rate_app(this);
                break;
            case R.id.menu_help /* 2131034212 */:
                startActivity(new Intent(this, (Class<?>) Help.class));
                break;
            case R.id.menu_about /* 2131034213 */:
                startActivity(new Intent(this, (Class<?>) About.class));
                break;
        }
    }
}
