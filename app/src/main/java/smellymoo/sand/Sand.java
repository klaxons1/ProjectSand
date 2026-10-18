package smellymoo.sand;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.preference.PreferenceManager;
import android.view.MotionEvent;
import android.view.OrientationEventListener;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.HorizontalScrollView;
import android.widget.ImageButton;
import android.widget.Toast;
import com.android.vending.billing.IInAppBillingService;
import java.util.ArrayList;
import org.json.JSONObject;

/* JADX INFO: loaded from: C:\Users\pashansky2005\Downloads\smellymoo.sand\classes.dex */
public class Sand extends Activity implements SurfaceHolder.Callback {
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
    static final String pro_sku = "premium";
    private static final int surface_num = 2;
    static Toast toasty;
    public IInAppBillingService mService;
    static short orientation = 0;
    static boolean mouse_down = false;
    static int toast_y = 100;
    private static SharedPreferences prefs = null;
    static boolean heat_view = false;
    static boolean enable_spouts = false;
    static boolean enable_rotate = true;
    static char zoom_factor = 1;
    static short selected_tool = -1;
    static Surface sand_surface = null;
    static final short[] zoom_lookup = {2, 3, 6};
    static boolean premium_unlocked = false;
    static boolean beta = false;
    static boolean billing_enabled = false;
    static boolean surface_loaded = false;
    OrientationEventListener orientation_listener = null;
    ServiceConnection mServiceConn = new ServiceConnection() { // from class: smellymoo.sand.Sand.1
        @Override // android.content.ServiceConnection
        public void onServiceDisconnected(ComponentName name) {
            Sand.this.mService = null;
        }

        @Override // android.content.ServiceConnection
        public void onServiceConnected(ComponentName name, IBinder service) {
            try {
                Sand.this.mService = IInAppBillingService.Stub.asInterface(service);
                if (Sand.this.mService.isBillingSupported(3, Sand.this.getPackageName(), "inapp") == 0) {
                    Sand.billing_enabled = true;
                }
            } catch (RemoteException e) {
            }
            Sand.this.update_purchases();
        }
    };

    @Override // android.app.Activity
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        prefs = PreferenceManager.getDefaultSharedPreferences(this);
        if (prefs.getInt("theme", 0) == 0) {
            setContentView(R.layout.sand);
        } else {
            setContentView(R.layout.onerow_sand);
        }
        SurfaceView sand_view = (SurfaceView) findViewById(R.id.sand_view);
        SurfaceHolder holder = sand_view.getHolder();
        holder.addCallback(this);
        Intent serviceIntent = new Intent("com.android.vending.billing.InAppBillingService.BIND");
        serviceIntent.setPackage("com.android.vending");
        bindService(serviceIntent, this.mServiceConn, 1);
        setRequestedOrientation(5);
        this.orientation_listener = new orientation_event_listener(this);
    }

    @Override // android.view.SurfaceHolder.Callback
    public void surfaceCreated(SurfaceHolder holder) {
        surface_loaded = true;
        HorizontalScrollView tool_box = (HorizontalScrollView) findViewById(R.id.scroll_tools);
        toast_y = tool_box.getHeight() + 20;
        if (prefs.getBoolean("dev", false)) {
            debug_unlock();
        }
        if (beta) {
            beta();
        } else if (!premium_unlocked) {
            if (prefs.getBoolean("never_unlock", false)) {
                premium_buttons(3);
            } else {
                premium_buttons(2);
            }
        }
        sand_surface = holder.getSurface();
        Engine.settings((char) zoom_lookup[zoom_factor], (short) prefs.getInt("sim_color", 0), enable_spouts);
        Engine.init(sand_surface, 2);
        this.orientation_listener.enable();
    }

    @Override // android.view.SurfaceHolder.Callback
    public void surfaceDestroyed(SurfaceHolder holder) {
        Engine.kill(2);
    }

    @Override // android.app.Activity
    protected void onPause() {
        super.onPause();
        Engine.pause(2);
    }

    @Override // android.app.Activity
    protected void onResume() {
        super.onResume();
        Engine.resume(2);
    }

    @Override // android.app.Activity
    public void onBackPressed() {
        Engine.remain_active();
        super.onBackPressed();
    }

    @Override // android.view.SurfaceHolder.Callback
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override // android.app.Activity
    protected void onDestroy() {
        this.orientation_listener.disable();
        super.onDestroy();
        if (this.mService != null) {
            unbindService(this.mServiceConn);
        }
    }

    /* JADX INFO: Access modifiers changed from: private */
    public void update_purchases() {
        if (!billing_enabled) {
            if (prefs.getBoolean("recover_pro", false)) {
                premium_unlocked = true;
                premium_buttons(1);
                return;
            }
            return;
        }
        try {
            Bundle ownedItems = this.mService.getPurchases(3, getPackageName(), "inapp", null);
            if (ownedItems != null) {
                int response = ownedItems.getInt("RESPONSE_CODE");
                if (response == 0) {
                    ArrayList<String> ownedSkus = ownedItems.getStringArrayList("INAPP_PURCHASE_ITEM_LIST");
                    for (int i = 0; i < ownedSkus.size(); i++) {
                        String sku = ownedSkus.get(i);
                        if (sku.equals(pro_sku)) {
                            premium_buttons(1);
                        }
                    }
                    prefs.edit().putBoolean("recover_pro", premium_unlocked).commit();
                }
            }
        } catch (RemoteException e) {
        }
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
            case R.id.tool_thermite /* 2131034164 */:
                return '3';
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
            case R.id.tool_portal /* 2131034181 */:
                return '1';
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
        set_button_state("fix", 0);
    }

    private void beta() {
        beta = true;
        set_button_state("firework", 1);
        set_button_state("pump", 1);
        set_button_state("portal", 1);
        set_button_state("nuke", 1);
        set_button_state("view", 1);
        set_button_state("thermite", 1);
        set_button_state("c4", 2);
        set_button_state("nitro", 2);
        set_button_state("ice", 3);
        set_button_state("coal", 3);
        set_button_state("plant", 3);
        set_button_state("sawdust", 3);
        set_button_state("salt", 3);
        set_button_state("gunpowder", 3);
        set_button_state("mercury", 3);
        set_button_state("blackh", 3);
        set_button_state("petrol", 3);
        set_button_state("algae", 3);
        set_button_state("paraffin", 3);
    }

    private void premium_buttons(int state) {
        if (state == 1) {
            premium_unlocked = true;
        }
        if (surface_loaded) {
            set_button_state("firework", state);
            set_button_state("pump", state);
            set_button_state("portal", state);
            set_button_state("nuke", state);
            set_button_state("nitro", state);
            set_button_state("view", state);
            set_button_state("c4", state);
            set_button_state("thermite", state);
        }
    }

    public void set_button_state(String name, int state) {
        int button_id = getResources().getIdentifier("tool_" + name, "id", getPackageName());
        if (button_id >= 0) {
            String append = "";
            View button = findViewById(button_id);
            if (button != null) {
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
                if (res_id != 0) {
                    ((ImageButton) button).setImageResource(res_id);
                }
            }
        }
    }

    public void tool(View view) {
        Object tag = view.getTag(R.id.button_state);
        if (tag != null) {
            switch (((Integer) tag).intValue()) {
                case 2:
                    if (billing_enabled) {
                        startActivityForResult(new Intent(this, (Class<?>) Freemium.class), 666);
                    } else {
                        Toast toastMakeText = Toast.makeText(getApplicationContext(), "in-app billing failed.", 0);
                        toastMakeText.setGravity(81, 0, toast_y);
                        toastMakeText.show();
                    }
                    break;
            }
        }
        switch (view.getId()) {
            case R.id.tool_rotate /* 2131034136 */:
                enable_rotate = enable_rotate ? false : true;
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
                Engine.setting_zoom(zoom_lookup[zoom_factor]);
                break;
            case R.id.tool_spouts /* 2131034139 */:
                enable_spouts = enable_spouts ? false : true;
                Engine.setting_spouts(enable_spouts);
                break;
            case R.id.tool_fix /* 2131034141 */:
                Engine.enqueue(22);
                break;
            case R.id.tool_clear /* 2131034143 */:
                Engine.enqueue(12);
                enable_spouts = false;
                Engine.setting_spouts(false);
                Engine.enqueue(13, 0);
                break;
            case R.id.tool_borders /* 2131034144 */:
                Engine.enqueue(8);
                break;
            case R.id.tool_slomo /* 2131034145 */:
                Engine.enqueue(7);
                break;
            case R.id.tool_settings /* 2131034146 */:
                startActivityForResult(new Intent(this, (Class<?>) Settings_popup.class), 888);
                break;
            case R.id.tool_debug /* 2131034147 */:
                Engine.enqueue(19, 2);
                break;
            case R.id.tool_view /* 2131034215 */:
                heat_view = !heat_view;
                Engine.setting_view((short) (heat_view ? 1 : 0));
                break;
            case R.id.tool_segfault /* 2131034218 */:
                Engine.enqueue(24);
                break;
            default:
                short sId_to_tool = (short) id_to_tool(view.getId());
                if (sId_to_tool == selected_tool) {
                    Engine.enqueue((char) 17, selected_tool, (short) 0);
                } else {
                    selected_tool = sId_to_tool;
                    Engine.enqueue((char) 5, selected_tool, (short) 0);
                }
                break;
        }
        if (toasty != null) {
            toasty.cancel();
        }
        toasty = Toast.makeText(getApplicationContext(), view.getContentDescription(), 0);
        toasty.setGravity(81, 0, toast_y);
        toasty.show();
    }

    private class orientation_event_listener extends OrientationEventListener {
        public orientation_event_listener(Context context) {
            super(context);
        }

        @Override // android.view.OrientationEventListener
        public void onOrientationChanged(int deg) {
            short o;
            if (deg != -1 && Sand.enable_rotate) {
                if (deg >= 30 && deg < 60) {
                    o = 1;
                } else if (deg >= 75 && deg < 105) {
                    o = 2;
                } else if (deg >= 120 && deg < 150) {
                    o = 3;
                } else if (deg >= 165 && deg < 195) {
                    o = 4;
                } else if (deg >= 210 && deg < 240) {
                    o = 5;
                } else if (deg >= 255 && deg < 285) {
                    o = 6;
                } else if (deg >= 300 && deg < 330) {
                    o = 7;
                } else if (deg < 345 && deg >= 15) {
                    return;
                } else {
                    o = 0;
                }
                if (o != Sand.orientation) {
                    Sand.orientation = o;
                    Engine.enqueue(Sand.ROTATE, o, (short) 0);
                }
            }
        }
    }

    @Override // android.app.Activity
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (resultCode) {
            case -1:
                switch (requestCode) {
                    case 666:
                        if (billing_enabled) {
                            try {
                                Bundle buyIntentBundle = this.mService.getBuyIntent(3, getPackageName(), pro_sku, "inapp", "dev-code");
                                switch (buyIntentBundle.getInt("RESPONSE_CODE")) {
                                    case 0:
                                        PendingIntent pendingIntent = (PendingIntent) buyIntentBundle.getParcelable("BUY_INTENT");
                                        startIntentSenderForResult(pendingIntent.getIntentSender(), 80085, new Intent(), 0, 0, 0);
                                        break;
                                    case 7:
                                        premium_buttons(1);
                                        break;
                                }
                            } catch (Exception e) {
                                return;
                            }
                        }
                        break;
                    case 80085:
                        String purchaseData = data.getStringExtra("INAPP_PURCHASE_DATA");
                        try {
                            JSONObject jo = new JSONObject(purchaseData);
                            String sku = jo.getString("productId");
                            if (sku.equals(pro_sku)) {
                                premium_buttons(1);
                            }
                        } catch (Exception e2) {
                            return;
                        }
                        break;
                }
                break;
            case 888:
                Engine.setting_brushsize(prefs.getInt("brush_size", 5));
                if (!premium_unlocked) {
                    if (!prefs.getBoolean("never_unlock", false)) {
                        premium_buttons(2);
                    } else {
                        premium_buttons(3);
                    }
                }
                break;
            case 999:
                prefs.edit().putBoolean("never_unlock", true).commit();
                premium_buttons(3);
                break;
            case 4269:
                premium_buttons(1);
                break;
            case 7337:
                beta();
                break;
        }
    }
}
