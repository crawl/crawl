package org.develz.crawl;

import android.content.Context;
import android.graphics.Color;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.InputConnection;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import java.util.HashSet;
import java.util.ArrayList;
import java.util.List;


public class DCSSKeyboard extends RelativeLayout implements View.OnClickListener {

    // Keyboards
    private final View keyboardLower;
    private final View keyboardUpper;
    private final View keyboardCtrl;
    private final View keyboardNumeric;

    // Our communication link to the EditText
    private InputConnection inputConnection;

    // Action keys
    private HashSet<Integer> actionKeys;

    // All buttons
    private List<Button> buttonList;

    // Constructors
    public DCSSKeyboard(Context context) {
        this(context, null, 0);
    }

    public DCSSKeyboard(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public DCSSKeyboard(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        // Define action keys
        actionKeys = new HashSet<>();
        actionKeys.add(KeyEvent.KEYCODE_TAB);
        actionKeys.add(KeyEvent.KEYCODE_ENTER);
        actionKeys.add(KeyEvent.KEYCODE_ESCAPE);
        actionKeys.add(KeyEvent.KEYCODE_DEL);
        actionKeys.add(KeyEvent.KEYCODE_FORWARD_DEL);
        actionKeys.add(KeyEvent.KEYCODE_SHIFT_LEFT);
        actionKeys.add(KeyEvent.KEYCODE_SHIFT_RIGHT);
        actionKeys.add(KeyEvent.KEYCODE_CTRL_LEFT);
        actionKeys.add(KeyEvent.KEYCODE_CTRL_RIGHT);
        actionKeys.add(KeyEvent.KEYCODE_F1);
        actionKeys.add(KeyEvent.KEYCODE_F2);
        actionKeys.add(KeyEvent.KEYCODE_F3);
        actionKeys.add(KeyEvent.KEYCODE_F4);
        actionKeys.add(KeyEvent.KEYCODE_F5);
        actionKeys.add(KeyEvent.KEYCODE_F6);
        actionKeys.add(KeyEvent.KEYCODE_F7);
        actionKeys.add(KeyEvent.KEYCODE_F8);
        actionKeys.add(KeyEvent.KEYCODE_F9);
        actionKeys.add(KeyEvent.KEYCODE_F10);
        actionKeys.add(KeyEvent.KEYCODE_F11);
        actionKeys.add(KeyEvent.KEYCODE_F12);

        // Load layout
        LayoutInflater.from(context).inflate(R.layout.keyboard, this, true);

        // Keyboards
        keyboardLower = findViewById(R.id.keyboard_lower);
        keyboardUpper = findViewById(R.id.keyboard_upper);
        keyboardCtrl = findViewById(R.id.keyboard_ctrl);
        keyboardNumeric = findViewById(R.id.keyboard_numeric);

        // Initialize key buttons - lower keyboard
        buttonList = new ArrayList<>();
        initKey(R.id.key_q);
        initKey(R.id.key_w);
        initKey(R.id.key_e);
        initKey(R.id.key_r);
        initKey(R.id.key_t);
        initKey(R.id.key_y);
        initKey(R.id.key_u);
        initKey(R.id.key_i);
        initKey(R.id.key_o);
        initKey(R.id.key_p);

        initKey(R.id.key_a);
        initKey(R.id.key_s);
        initKey(R.id.key_d);
        initKey(R.id.key_f);
        initKey(R.id.key_g);
        initKey(R.id.key_h);
        initKey(R.id.key_j);
        initKey(R.id.key_k);
        initKey(R.id.key_l);
        initKey(R.id.key_bspace);

        initKey(R.id.key_tab_lower);
        initKey(R.id.key_z);
        initKey(R.id.key_x);
        initKey(R.id.key_c);
        initKey(R.id.key_v);
        initKey(R.id.key_b);
        initKey(R.id.key_n);
        initKey(R.id.key_m);
        initKey(R.id.key_semicol);
        initKey(R.id.key_apos);

        initKey(R.id.key_shift_lower);
        initKey(R.id.key_ctrl_lower);
        initKey(R.id.key_grave);
        initKey(R.id.key_5);
        initKey(R.id.key_minus);
        initKey(R.id.key_plus);
        initKey(R.id.key_enter);
        initKey(R.id.key_123_lower);

        // Initialize buttons - upper keyboard
        initKey(R.id.key_Q);
        initKey(R.id.key_W);
        initKey(R.id.key_E);
        initKey(R.id.key_R);
        initKey(R.id.key_T);
        initKey(R.id.key_Y);
        initKey(R.id.key_U);
        initKey(R.id.key_I);
        initKey(R.id.key_O);
        initKey(R.id.key_P);

        initKey(R.id.key_A);
        initKey(R.id.key_S);
        initKey(R.id.key_D);
        initKey(R.id.key_F);
        initKey(R.id.key_G);
        initKey(R.id.key_H);
        initKey(R.id.key_J);
        initKey(R.id.key_K);
        initKey(R.id.key_L);
        initKey(R.id.key_equal);

        initKey(R.id.key_tab_upper);
        initKey(R.id.key_Z);
        initKey(R.id.key_X);
        initKey(R.id.key_C);
        initKey(R.id.key_V);
        initKey(R.id.key_B);
        initKey(R.id.key_N);
        initKey(R.id.key_M);
        initKey(R.id.key_colon);
        initKey(R.id.key_quot);

        initKey(R.id.key_shift_upper);
        initKey(R.id.key_ctrl_upper);
        initKey(R.id.key_lt);
        initKey(R.id.key_gt);
        initKey(R.id.key_comma);
        initKey(R.id.key_dot);
        initKey(R.id.key_space);
        initKey(R.id.key_123_upper);

        // Initialize buttons - ctrl keyboard
        initKey(R.id.key_Cq);
        initKey(R.id.key_Cw);
        initKey(R.id.key_Ce);
        initKey(R.id.key_Cr);
        initKey(R.id.key_Ct);
        initKey(R.id.key_Cy);
        initKey(R.id.key_Cu);
        initKey(R.id.key_Ci);
        initKey(R.id.key_Co);
        initKey(R.id.key_Cp);

        initKey(R.id.key_Ca);
        initKey(R.id.key_Cs);
        initKey(R.id.key_Cd);
        initKey(R.id.key_Cf);
        initKey(R.id.key_Cg);
        initKey(R.id.key_Ch);
        initKey(R.id.key_Cj);
        initKey(R.id.key_Ck);
        initKey(R.id.key_Cl);
        initKey(R.id.key_pipe);

        initKey(R.id.key_quest);
        initKey(R.id.key_Cz);
        initKey(R.id.key_Cx);
        initKey(R.id.key_Cc);
        initKey(R.id.key_Cv);
        initKey(R.id.key_Cb);
        initKey(R.id.key_Cn);
        initKey(R.id.key_Cm);
        initKey(R.id.key_slash);
        initKey(R.id.key_bslash);

        initKey(R.id.key_shift_ctrl);
        initKey(R.id.key_ctrl_ctrl);
        initKey(R.id.key_lcurly);
        initKey(R.id.key_rcurly);
        initKey(R.id.key_lbracket);
        initKey(R.id.key_rbracket);
        initKey(R.id.key_escape);
        initKey(R.id.key_123_ctrl);

        // Initialize buttons - numeric keyboard
        initKey(R.id.key_num_F1);
        initKey(R.id.key_num_F2);
        initKey(R.id.key_num_F3);
        initKey(R.id.key_num_tilde);
        initKey(R.id.key_num_exclam);
        initKey(R.id.key_num_at);
        initKey(R.id.key_num_hash);
        initKey(R.id.key_num_7);
        initKey(R.id.key_num_8);
        initKey(R.id.key_num_9);

        initKey(R.id.key_num_F4);
        initKey(R.id.key_num_F5);
        initKey(R.id.key_num_F6);
        initKey(R.id.key_num_dollar);
        initKey(R.id.key_num_percent);
        initKey(R.id.key_num_circum);
        initKey(R.id.key_num_amper);
        initKey(R.id.key_num_4);
        initKey(R.id.key_num_5);
        initKey(R.id.key_num_6);

        initKey(R.id.key_num_F7);
        initKey(R.id.key_num_F8);
        initKey(R.id.key_num_F9);
        initKey(R.id.key_num_aster);
        initKey(R.id.key_num_lparen);
        initKey(R.id.key_num_rparen);
        initKey(R.id.key_num_lowline);
        initKey(R.id.key_num_1);
        initKey(R.id.key_num_2);
        initKey(R.id.key_num_3);

        initKey(R.id.key_num_F10);
        initKey(R.id.key_num_F11);
        initKey(R.id.key_num_F12);
        initKey(R.id.key_num_lt);
        initKey(R.id.key_num_gt);
        initKey(R.id.key_num_equal);
        initKey(R.id.key_num_quest);
        initKey(R.id.key_num_0);
        initKey(R.id.key_abc);
    }

    // Swap keyboards
    private void updateLayout(View v) {
        if ((v.getId() == R.id.key_shift_lower) ||
                (v.getId() == R.id.key_shift_ctrl)) {
            keyboardLower.setVisibility(View.INVISIBLE);
            keyboardCtrl.setVisibility(View.INVISIBLE);
            keyboardNumeric.setVisibility(View.INVISIBLE);
            keyboardUpper.setVisibility(View.VISIBLE);
        } else if (v.getId() == R.id.key_ctrl_lower ||
                v.getId() == R.id.key_ctrl_upper) {
            keyboardLower.setVisibility(View.INVISIBLE);
            keyboardUpper.setVisibility(View.INVISIBLE);
            keyboardNumeric.setVisibility(View.INVISIBLE);
            keyboardCtrl.setVisibility(View.VISIBLE);
        } else if ((v.getId() == R.id.key_123_lower) ||
                (v.getId() == R.id.key_123_upper) ||
                (v.getId() == R.id.key_123_ctrl)) {
            keyboardLower.setVisibility(View.INVISIBLE);
            keyboardUpper.setVisibility(View.INVISIBLE);
            keyboardCtrl.setVisibility(View.INVISIBLE);
            keyboardNumeric.setVisibility(View.VISIBLE);
        } else if ((v.getId() == R.id.key_abc) ||
                (((LinearLayout)v.getParent().getParent()).getId() == R.id.keyboard_upper) ||
                (((LinearLayout)v.getParent().getParent()).getId() == R.id.keyboard_ctrl)) {
            keyboardUpper.setVisibility(View.INVISIBLE);
            keyboardCtrl.setVisibility(View.INVISIBLE);
            keyboardNumeric.setVisibility(View.INVISIBLE);
            keyboardLower.setVisibility(View.VISIBLE);
        }
    }

    // Init a single key
    private void initKey(int id) {
        Button button = findViewById(id);
        button.setOnClickListener(this);
        buttonList.add(button);
    }

    // Identify action events
    private boolean isActionEvent(KeyEvent event) {
        return (actionKeys.contains(event.getKeyCode()) ||
                ((event.getMetaState() & KeyEvent.META_CTRL_ON) == KeyEvent.META_CTRL_ON));
    }

    // Process software keyboard events
    @Override
    public void onClick(View v) {
        String tag = v.getTag().toString();
        if (!"layout".equals(tag)) {
            String[] keyList = tag.split(",");
            ArrayList<Integer> keycodes = new ArrayList<>();
            for (int i = 0; i < keyList.length; i++) {
                try {
                    int keycode = Integer.parseInt(keyList[i]);
                    keycodes.add(keycode);
                } catch (NumberFormatException e) {
                    Log.e("KEY", "Invalid key code: " + keyList[i]);
                }
            }
            // Press keys
            int metaState = 0;
            for (int i = 0; i < keycodes.size(); i++) {
                if (keycodes.get(i) == KeyEvent.KEYCODE_SHIFT_LEFT ||
                        keycodes.get(i) == KeyEvent.KEYCODE_SHIFT_RIGHT) {
                    metaState |= KeyEvent.META_SHIFT_ON | KeyEvent.META_SHIFT_LEFT_ON;
                }
                if (keycodes.get(i) == KeyEvent.KEYCODE_CTRL_LEFT ||
                        keycodes.get(i) == KeyEvent.KEYCODE_CTRL_RIGHT) {
                    metaState |= KeyEvent.META_CTRL_ON | KeyEvent.META_CTRL_LEFT_ON;
                }
                KeyEvent eventDown = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN,
                        keycodes.get(i), 0, metaState);
                dispatchKeyEvent(eventDown);
            }
            // Release keys
            for (int i = keycodes.size() - 1; i >= 0; i--) {
                if (keycodes.get(i) == KeyEvent.KEYCODE_SHIFT_LEFT ||
                        keycodes.get(i) == KeyEvent.KEYCODE_SHIFT_RIGHT) {
                    metaState &= ~KeyEvent.META_SHIFT_ON & ~KeyEvent.META_SHIFT_LEFT_ON;
                }
                if (keycodes.get(i) == KeyEvent.KEYCODE_CTRL_LEFT ||
                        keycodes.get(i) == KeyEvent.KEYCODE_CTRL_RIGHT) {
                    metaState &= ~KeyEvent.META_CTRL_ON & ~KeyEvent.META_CTRL_LEFT_ON;
                }
                KeyEvent eventUp = new KeyEvent(0, 0, KeyEvent.ACTION_UP,
                        keycodes.get(i), 0, metaState);
                dispatchKeyEvent(eventUp);
            }
        }
        // Update keyboard
        updateLayout(v);
    }

    // Process all keyboard events
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        Log.v("KEY", "KeyEvent: " + event.toString());
        if (isActionEvent(event)) {
            inputConnection.sendKeyEvent(event);
        } else if (event.getAction() == KeyEvent.ACTION_DOWN) {
            inputConnection.commitText(String.valueOf((char) event.getUnicodeChar()), 1);
        }
        return true;
    }

    // The activity must connect with the DummyEdit's InputConnection
    public void setInputConnection(InputConnection ic) {
        this.inputConnection = ic;
    }

    // Turn keyboard transparent
    public void transparentKeyboard() {
        RelativeLayout mainLayout = findViewById(R.id.main_layout);
        mainLayout.setBackgroundColor(Color.TRANSPARENT);
        for (Button button : buttonList) {
            button.setBackgroundResource(R.drawable.transparent_button);
        }
    }

}
