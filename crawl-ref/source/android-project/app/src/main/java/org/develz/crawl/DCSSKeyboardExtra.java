package org.develz.crawl;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;


public class DCSSKeyboardExtra extends DCSSKeyboardBase implements View.OnClickListener {

    // Keyboards
    private View keyboardExtra;

    // Spaces
    private View spaceTopLeft;
    private View spaceTopRight;
    private View spaceMiddleLeft;
    private View spaceMiddleRight;
    private View spaceBottomLeft;
    private View spaceBottomRight;

    // Constructors
    public DCSSKeyboardExtra(Context context) {
        this(context, null, 0);
    }

    public DCSSKeyboardExtra(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public DCSSKeyboardExtra(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        // Load layout
        LayoutInflater.from(context).inflate(R.layout.keyboard_extra, this, true);

        // Keyboards
        keyboardExtra = findViewById(R.id.keyboard_extra_numeric);

        // Spaces
        spaceTopLeft = findViewById(R.id.space_top_left);
        spaceTopRight = findViewById(R.id.space_top_right);
        spaceMiddleLeft = findViewById(R.id.space_middle_left);
        spaceMiddleRight = findViewById(R.id.space_middle_right);
        spaceBottomLeft = findViewById(R.id.space_bottom_left);
        spaceBottomRight = findViewById(R.id.space_bottom_right);

        // Initialize buttons - numeric keyboard
        initKey(R.id.key_extra_7);
        initKey(R.id.key_extra_8);
        initKey(R.id.key_extra_9);
        initKey(R.id.key_extra_4);
        initKey(R.id.key_extra_5);
        initKey(R.id.key_extra_6);
        initKey(R.id.key_extra_1);
        initKey(R.id.key_extra_2);
        initKey(R.id.key_extra_3);
    }

    // Extra init settings
    @Override
    public void initKeyboard(int keyboardOption, int size) {
        if (keyboardOption == 1 || keyboardOption == 3) {
            moveLeft();
        } else if (keyboardOption == 2 || keyboardOption == 4) {
            moveRight();
        }
    }

    // Grow the space to set the keyboard in position
    private void inflateSpace(View space) {
        LinearLayout.LayoutParams lParams = (LinearLayout.LayoutParams) space.getLayoutParams();
        lParams.weight = 7;
    }

    // Move keyboard to the left
     public void moveLeft() {
        inflateSpace(spaceTopRight);
        inflateSpace(spaceMiddleRight);
        inflateSpace(spaceBottomRight);
     }

     // Move keyboard to the right
    public void moveRight() {
        inflateSpace(spaceTopLeft);
        inflateSpace(spaceMiddleLeft);
        inflateSpace(spaceBottomLeft);
    }

    // Swap keyboards
    @Override
    protected void updateLayout(View v) {}

}
