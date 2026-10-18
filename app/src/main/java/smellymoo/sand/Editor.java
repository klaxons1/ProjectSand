package smellymoo.sand;

import android.app.Activity;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.HorizontalScrollView;
import android.widget.ImageButton;
import android.widget.Toast;

/* JADX INFO: loaded from: C:\Users\pashansky2005\Downloads\smellymoo.sand\classes.dex */
public class Editor extends Activity implements SurfaceHolder.Callback {
    static final char BUTTON_ENABLED = 1;
    static final char BUTTON_GREY = 3;
    static final char BUTTON_INVISIBLE = 4;
    static final char BUTTON_LOCKED = 2;
    static final char BUTTON_VISIBLE = 0;
    static final char KILL = 1;
    static final char MOUSE_DOWN = 2;
    static final char MOUSE_MOVE = 3;
    static final char MOUSE_UP = 4;
    static final char ROTATE = 6;
    static Toast toasty;
    static final short[] zoom_lookup = {1, 2, 3, 6};
    static boolean ui_loaded = false;
    static Surface sand_surface = null;
    static short orientation = 0;
    static boolean enable_rotate = true;
    static boolean mouse_down = false;
    static char zoom_factor = 2;
    static int toast_y = 100;

    @Override // android.app.Activity
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.editor);
        SurfaceView sand_view = (SurfaceView) findViewById(R.id.sand_view);
        SurfaceHolder holder = sand_view.getHolder();
        holder.addCallback(this);
    }

    @Override // android.app.Activity
    protected void onPause() {
        super.onPause();
        Engine.pause(3);
    }

    @Override // android.view.SurfaceHolder.Callback
    public void surfaceDestroyed(SurfaceHolder holder) {
        Engine.kill(3);
    }

    @Override // android.view.SurfaceHolder.Callback
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override // android.app.Activity
    public void onBackPressed() {
        Engine.remain_active();
        super.onBackPressed();
    }

    @Override // android.app.Activity
    protected void onDestroy() {
        super.onDestroy();
    }

    @Override // android.view.SurfaceHolder.Callback
    public void surfaceCreated(SurfaceHolder holder) {
        ui_loaded = true;
        setRequestedOrientation(5);
        HorizontalScrollView tool_box = (HorizontalScrollView) findViewById(R.id.scroll_tools);
        toast_y = tool_box.getHeight() + 20;
        premium_lock(false);
        sand_surface = holder.getSurface();
        Engine.init(sand_surface, 3);
    }

    /* JADX WARN: Can't fix incorrect switch cases order, some code will duplicate */
    @Override // android.app.Activity
    public boolean onTouchEvent(MotionEvent event) {
        super.onTouchEvent(event);
        short x = (short) event.getX();
        short y = (short) event.getY();
        switch (event.getAction()) {
            case 0:
                mouse_down = true;
                Engine.enqueue((char) 2, x, y);
                return true;
            case 1:
                mouse_down = false;
                Engine.enqueue((char) 4, x, y);
                return true;
            case 2:
                if (mouse_down) {
                    Engine.enqueue((char) 3, x, y);
                }
                return true;
            default:
                return true;
        }
    }

    public char id_to_tool(int id) {
        switch (id) {
            case R.id.tool_eraser /* 2131034140 */:
                return BUTTON_VISIBLE;
            case R.id.tool_fix /* 2131034141 */:
            case R.id.tableRow2 /* 2131034142 */:
            case R.id.tool_clear /* 2131034143 */:
            case R.id.tool_borders /* 2131034144 */:
            case R.id.tool_slomo /* 2131034145 */:
            case R.id.tool_settings /* 2131034146 */:
            case R.id.tool_debug /* 2131034147 */:
            case R.id.tool_thermite /* 2131034164 */:
            case R.id.tool_portal /* 2131034181 */:
            default:
                return BUTTON_VISIBLE;
            case R.id.tool_wall /* 2131034148 */:
                return ROTATE;
            case R.id.tool_glass /* 2131034149 */:
                return '\b';
            case R.id.tool_ice /* 2131034150 */:
                return (char) 21;
            case R.id.tool_pump /* 2131034151 */:
                return '(';
            case R.id.tool_wood /* 2131034152 */:
                return (char) 16;
            case R.id.tool_oil /* 2131034153 */:
                return (char) 4;
            case R.id.tool_wax /* 2131034154 */:
                return '\r';
            case R.id.tool_gas /* 2131034155 */:
                return '\n';
            case R.id.tool_fire /* 2131034156 */:
                return '&';
            case R.id.tool_lava /* 2131034157 */:
                return (char) 14;
            case R.id.tool_sand /* 2131034158 */:
                return (char) 1;
            case R.id.tool_salt /* 2131034159 */:
                return (char) 2;
            case R.id.tool_filter /* 2131034160 */:
                return '\'';
            case R.id.tool_mercury /* 2131034161 */:
                return (char) 5;
            case R.id.tool_gunpowder /* 2131034162 */:
                return (char) 22;
            case R.id.tool_c4 /* 2131034163 */:
                return (char) 27;
            case R.id.tool_nuke /* 2131034165 */:
                return (char) 29;
            case R.id.tool_stone /* 2131034166 */:
                return (char) 7;
            case R.id.tool_perm /* 2131034167 */:
                return '\"';
            case R.id.tool_metal /* 2131034168 */:
                return '\t';
            case R.id.tool_water /* 2131034169 */:
                return (char) 3;
            case R.id.tool_tap /* 2131034170 */:
                return '%';
            case R.id.tool_coal /* 2131034171 */:
                return (char) 17;
            case R.id.tool_paraffin /* 2131034172 */:
                return (char) 25;
            case R.id.tool_wick /* 2131034173 */:
                return (char) 19;
            case R.id.tool_plant /* 2131034174 */:
                return (char) 20;
            case R.id.tool_algae /* 2131034175 */:
                return (char) 11;
            case R.id.tool_acid /* 2131034176 */:
                return '\f';
            case R.id.tool_sawdust /* 2131034177 */:
                return (char) 31;
            case R.id.tool_cement /* 2131034178 */:
                return (char) 15;
            case R.id.tool_petrol /* 2131034179 */:
                return (char) 24;
            case R.id.tool_black /* 2131034180 */:
                return (char) 28;
            case R.id.tool_nitro /* 2131034182 */:
                return (char) 26;
            case R.id.tool_firework /* 2131034183 */:
                return (char) 23;
            case R.id.tool_life /* 2131034184 */:
                return '*';
        }
    }

    private void debug_unlock() {
        set_button_state("debug", 0);
        set_button_state("save", 0);
        set_button_state("settings", 0);
    }

    private void premium_lock(boolean lock) {
        int lock_state = lock ? 2 : 1;
        if (ui_loaded) {
            set_button_state("firework", lock_state);
            set_button_state("filter", lock_state);
            set_button_state("gunpowder", lock_state);
            set_button_state("nuke", lock_state);
            set_button_state("nitro", lock_state);
            set_button_state("petrol", lock_state);
            set_button_state("c4", lock_state);
            set_button_state("perm", lock_state);
        }
    }

    public void set_button_state(String name, int state) {
        int button_id = getResources().getIdentifier("tool_" + name, "id", getPackageName());
        String append = "";
        View button = findViewById(button_id);
        button.setTag(R.id.button_state, Integer.valueOf(state));
        switch (state) {
            case 0:
                button.setVisibility(0);
                return;
            case 2:
                append = "_locked";
                break;
            case 3:
                append = "_grey";
                break;
            case Engine.ONE_SIM /* 4 */:
                button.setVisibility(8);
                return;
        }
        int res_id = getResources().getIdentifier(String.valueOf(name) + append, "drawable", getPackageName());
        ((ImageButton) button).setImageResource(res_id);
    }

    public void tool(View view) {
        Object state = view.getTag(R.id.button_state);
        if (state == null || ((Integer) state).intValue() != 3) {
            switch (view.getId()) {
                case R.id.tool_rotate /* 2131034136 */:
                    enable_rotate = !enable_rotate;
                    break;
                case R.id.tool_save /* 2131034137 */:
                    Engine.enqueue(23);
                    break;
                case R.id.tool_zoom /* 2131034138 */:
                    char c = (char) (zoom_factor + 1);
                    zoom_factor = c;
                    if (c == zoom_lookup.length) {
                        zoom_factor = BUTTON_VISIBLE;
                    }
                    Engine.enqueue('\t', zoom_lookup[zoom_factor], (short) 0);
                    break;
                case R.id.tool_spouts /* 2131034139 */:
                    Engine.enqueue(13);
                    break;
                case R.id.tool_eraser /* 2131034140 */:
                case R.id.tableRow2 /* 2131034142 */:
                default:
                    Engine.enqueue((char) 5, (short) id_to_tool(view.getId()), (short) 0);
                    break;
                case R.id.tool_fix /* 2131034141 */:
                    Engine.enqueue(22);
                    break;
                case R.id.tool_clear /* 2131034143 */:
                    Engine.enqueue(12);
                    break;
                case R.id.tool_borders /* 2131034144 */:
                    Engine.enqueue(8);
                    break;
                case R.id.tool_slomo /* 2131034145 */:
                    Engine.enqueue(7);
                    break;
                case R.id.tool_settings /* 2131034146 */:
                    Engine.enqueue(21);
                    break;
                case R.id.tool_debug /* 2131034147 */:
                    Engine.enqueue(20);
                    break;
            }
            if (toasty != null) {
                toasty.cancel();
            }
            toasty = Toast.makeText(getApplicationContext(), view.getContentDescription(), 0);
            toasty.setGravity(81, 0, toast_y);
            toasty.show();
        }
    }
}
