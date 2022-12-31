package org.develz.crawl;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.InputConnection;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import java.util.HashSet;
import java.util.ArrayList;


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

        // Initialize buttons - lower keyboard
        findViewById(R.id.key_q).setOnClickListener(this);
        findViewById(R.id.key_w).setOnClickListener(this);
        findViewById(R.id.key_e).setOnClickListener(this);
        findViewById(R.id.key_r).setOnClickListener(this);
        findViewById(R.id.key_t).setOnClickListener(this);
        findViewById(R.id.key_y).setOnClickListener(this);
        findViewById(R.id.key_u).setOnClickListener(this);
        findViewById(R.id.key_i).setOnClickListener(this);
        findViewById(R.id.key_o).setOnClickListener(this);
        findViewById(R.id.key_p).setOnClickListener(this);

        findViewById(R.id.key_a).setOnClickListener(this);
        findViewById(R.id.key_s).setOnClickListener(this);
        findViewById(R.id.key_d).setOnClickListener(this);
        findViewById(R.id.key_f).setOnClickListener(this);
        findViewById(R.id.key_g).setOnClickListener(this);
        findViewById(R.id.key_h).setOnClickListener(this);
        findViewById(R.id.key_j).setOnClickListener(this);
        findViewById(R.id.key_k).setOnClickListener(this);
        findViewById(R.id.key_l).setOnClickListener(this);
        findViewById(R.id.key_bspace).setOnClickListener(this);

        findViewById(R.id.key_tab_lower).setOnClickListener(this);
        findViewById(R.id.key_z).setOnClickListener(this);
        findViewById(R.id.key_x).setOnClickListener(this);
        findViewById(R.id.key_c).setOnClickListener(this);
        findViewById(R.id.key_v).setOnClickListener(this);
        findViewById(R.id.key_b).setOnClickListener(this);
        findViewById(R.id.key_n).setOnClickListener(this);
        findViewById(R.id.key_m).setOnClickListener(this);
        findViewById(R.id.key_semicol).setOnClickListener(this);
        findViewById(R.id.key_apos).setOnClickListener(this);

        findViewById(R.id.key_shift_lower).setOnClickListener(this);
        findViewById(R.id.key_ctrl_lower).setOnClickListener(this);
        findViewById(R.id.key_grave).setOnClickListener(this);
        findViewById(R.id.key_5).setOnClickListener(this);
        findViewById(R.id.key_minus).setOnClickListener(this);
        findViewById(R.id.key_plus).setOnClickListener(this);
        findViewById(R.id.key_enter).setOnClickListener(this);
        findViewById(R.id.key_123_lower).setOnClickListener(this);

        // Initialize buttons - upper keyboard
        findViewById(R.id.key_Q).setOnClickListener(this);
        findViewById(R.id.key_W).setOnClickListener(this);
        findViewById(R.id.key_E).setOnClickListener(this);
        findViewById(R.id.key_R).setOnClickListener(this);
        findViewById(R.id.key_T).setOnClickListener(this);
        findViewById(R.id.key_Y).setOnClickListener(this);
        findViewById(R.id.key_U).setOnClickListener(this);
        findViewById(R.id.key_I).setOnClickListener(this);
        findViewById(R.id.key_O).setOnClickListener(this);
        findViewById(R.id.key_P).setOnClickListener(this);

        findViewById(R.id.key_A).setOnClickListener(this);
        findViewById(R.id.key_S).setOnClickListener(this);
        findViewById(R.id.key_D).setOnClickListener(this);
        findViewById(R.id.key_F).setOnClickListener(this);
        findViewById(R.id.key_G).setOnClickListener(this);
        findViewById(R.id.key_H).setOnClickListener(this);
        findViewById(R.id.key_J).setOnClickListener(this);
        findViewById(R.id.key_K).setOnClickListener(this);
        findViewById(R.id.key_L).setOnClickListener(this);
        findViewById(R.id.key_equal).setOnClickListener(this);

        findViewById(R.id.key_tab_upper).setOnClickListener(this);
        findViewById(R.id.key_Z).setOnClickListener(this);
        findViewById(R.id.key_X).setOnClickListener(this);
        findViewById(R.id.key_C).setOnClickListener(this);
        findViewById(R.id.key_V).setOnClickListener(this);
        findViewById(R.id.key_B).setOnClickListener(this);
        findViewById(R.id.key_N).setOnClickListener(this);
        findViewById(R.id.key_M).setOnClickListener(this);
        findViewById(R.id.key_colon).setOnClickListener(this);
        findViewById(R.id.key_quot).setOnClickListener(this);

        findViewById(R.id.key_shift_upper).setOnClickListener(this);
        findViewById(R.id.key_ctrl_upper).setOnClickListener(this);
        findViewById(R.id.key_lt).setOnClickListener(this);
        findViewById(R.id.key_gt).setOnClickListener(this);
        findViewById(R.id.key_comma).setOnClickListener(this);
        findViewById(R.id.key_dot).setOnClickListener(this);
        findViewById(R.id.key_space).setOnClickListener(this);
        findViewById(R.id.key_123_upper).setOnClickListener(this);

        // Initialize buttons - ctrl keyboard
        findViewById(R.id.key_Cq).setOnClickListener(this);
        findViewById(R.id.key_Cw).setOnClickListener(this);
        findViewById(R.id.key_Ce).setOnClickListener(this);
        findViewById(R.id.key_Cr).setOnClickListener(this);
        findViewById(R.id.key_Ct).setOnClickListener(this);
        findViewById(R.id.key_Cy).setOnClickListener(this);
        findViewById(R.id.key_Cu).setOnClickListener(this);
        findViewById(R.id.key_Ci).setOnClickListener(this);
        findViewById(R.id.key_Co).setOnClickListener(this);
        findViewById(R.id.key_Cp).setOnClickListener(this);

        findViewById(R.id.key_Ca).setOnClickListener(this);
        findViewById(R.id.key_Cs).setOnClickListener(this);
        findViewById(R.id.key_Cd).setOnClickListener(this);
        findViewById(R.id.key_Cf).setOnClickListener(this);
        findViewById(R.id.key_Cg).setOnClickListener(this);
        findViewById(R.id.key_Ch).setOnClickListener(this);
        findViewById(R.id.key_Cj).setOnClickListener(this);
        findViewById(R.id.key_Ck).setOnClickListener(this);
        findViewById(R.id.key_Cl).setOnClickListener(this);
        findViewById(R.id.key_pipe).setOnClickListener(this);

        findViewById(R.id.key_quest).setOnClickListener(this);
        findViewById(R.id.key_Cz).setOnClickListener(this);
        findViewById(R.id.key_Cx).setOnClickListener(this);
        findViewById(R.id.key_Cc).setOnClickListener(this);
        findViewById(R.id.key_Cv).setOnClickListener(this);
        findViewById(R.id.key_Cb).setOnClickListener(this);
        findViewById(R.id.key_Cn).setOnClickListener(this);
        findViewById(R.id.key_Cm).setOnClickListener(this);
        findViewById(R.id.key_slash).setOnClickListener(this);
        findViewById(R.id.key_bslash).setOnClickListener(this);

        findViewById(R.id.key_shift_ctrl).setOnClickListener(this);
        findViewById(R.id.key_ctrl_ctrl).setOnClickListener(this);
        findViewById(R.id.key_lcurly).setOnClickListener(this);
        findViewById(R.id.key_rcurly).setOnClickListener(this);
        findViewById(R.id.key_lbracket).setOnClickListener(this);
        findViewById(R.id.key_rbracket).setOnClickListener(this);
        findViewById(R.id.key_escape).setOnClickListener(this);
        findViewById(R.id.key_123_ctrl).setOnClickListener(this);

        // Initialize buttons - numeric keyboard
        findViewById(R.id.key_num_F1).setOnClickListener(this);
        findViewById(R.id.key_num_F2).setOnClickListener(this);
        findViewById(R.id.key_num_F3).setOnClickListener(this);
        findViewById(R.id.key_num_tilde).setOnClickListener(this);
        findViewById(R.id.key_num_exclam).setOnClickListener(this);
        findViewById(R.id.key_num_at).setOnClickListener(this);
        findViewById(R.id.key_num_hash).setOnClickListener(this);
        findViewById(R.id.key_num_7).setOnClickListener(this);
        findViewById(R.id.key_num_8).setOnClickListener(this);
        findViewById(R.id.key_num_9).setOnClickListener(this);

        findViewById(R.id.key_num_F4).setOnClickListener(this);
        findViewById(R.id.key_num_F5).setOnClickListener(this);
        findViewById(R.id.key_num_F6).setOnClickListener(this);
        findViewById(R.id.key_num_dollar).setOnClickListener(this);
        findViewById(R.id.key_num_percent).setOnClickListener(this);
        findViewById(R.id.key_num_circum).setOnClickListener(this);
        findViewById(R.id.key_num_amper).setOnClickListener(this);
        findViewById(R.id.key_num_4).setOnClickListener(this);
        findViewById(R.id.key_num_5).setOnClickListener(this);
        findViewById(R.id.key_num_6).setOnClickListener(this);

        findViewById(R.id.key_num_F7).setOnClickListener(this);
        findViewById(R.id.key_num_F8).setOnClickListener(this);
        findViewById(R.id.key_num_F9).setOnClickListener(this);
        findViewById(R.id.key_num_aster).setOnClickListener(this);
        findViewById(R.id.key_num_lparen).setOnClickListener(this);
        findViewById(R.id.key_num_rparen).setOnClickListener(this);
        findViewById(R.id.key_num_lowline).setOnClickListener(this);
        findViewById(R.id.key_num_1).setOnClickListener(this);
        findViewById(R.id.key_num_2).setOnClickListener(this);
        findViewById(R.id.key_num_3).setOnClickListener(this);

        findViewById(R.id.key_num_F10).setOnClickListener(this);
        findViewById(R.id.key_num_F11).setOnClickListener(this);
        findViewById(R.id.key_num_F12).setOnClickListener(this);
        findViewById(R.id.key_num_lt).setOnClickListener(this);
        findViewById(R.id.key_num_gt).setOnClickListener(this);
        findViewById(R.id.key_num_equal).setOnClickListener(this);
        findViewById(R.id.key_num_quest).setOnClickListener(this);
        findViewById(R.id.key_num_0).setOnClickListener(this);
        findViewById(R.id.key_abc).setOnClickListener(this);
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

}
