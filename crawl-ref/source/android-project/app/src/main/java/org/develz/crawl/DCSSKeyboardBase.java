package org.develz.crawl;

import android.content.Context;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.InputConnection;
import android.widget.Button;
import android.widget.RelativeLayout;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;


public abstract class DCSSKeyboardBase extends RelativeLayout implements View.OnClickListener {

    // Our communication link to the EditText
    private InputConnection inputConnection;

    // Action keys
    private HashSet<Integer> actionKeys;

    // All buttons
    protected List<Button> buttonList;

    // Constructors
    public DCSSKeyboardBase(Context context) {
        this(context, null, 0);
    }

    public DCSSKeyboardBase(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public DCSSKeyboardBase(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        buttonList = new ArrayList<>();

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
    }

    // Extra init settings
    public void initKeyboard(int keyboardOption, int size) {
        for (Button button : buttonList) {
            button.getLayoutParams().height = size;
        }
    }

    // Init a single key
    protected void initKey(int id) {
        Button button = findViewById(id);
        button.setOnClickListener(this);
        buttonList.add(button);
    }

    // Swap keyboards
    protected abstract void updateLayout(View v);

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
