package org.develz.crawl;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.Spinner;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SwitchCompat;

public class DCSSLauncher extends AppCompatActivity implements AdapterView.OnItemSelectedListener, TextWatcher, CompoundButton.OnCheckedChangeListener {

    public final static String TAG = "LAUNCHER";

    private static final String INIT_FILE = "/init.txt";

    private static final int DEFAULT_KEYBOARD = 0;

    private static final boolean DEFAULT_FULL_SCREEN = true;

    // Crawl's init file
    private File initFile;

    // Android options
    private SharedPreferences preferences;

    // Current keyboard type
    private int keyboardOption;

    // Extra keyboard options
    private int extraKeyboardOption;

    // Keyboard size input
    EditText ksizeEditText;

    // Keyboard size in pixels
    private float keyboardSizePx;

    // Default keyboard size in dp
    private int defaultKbSizeDp;

    // Screen density
    private float density;

    // Full screen input
    SwitchCompat fullScreenSwitch;

    // Full screen
    private boolean fullScreen;

    @Override
    protected void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);
        setContentView(R.layout.launcher);

        findViewById(R.id.startButton).setOnClickListener(this::startGame);
        findViewById(R.id.editInitFile).setOnClickListener(this::editInitFile);
        findViewById(R.id.morgueButton).setOnClickListener(this::openMorgue);
        findViewById(R.id.modsButton).setOnClickListener(this::openMods);

        preferences = getPreferences(Context.MODE_PRIVATE);
        keyboardOption = preferences.getInt("keyboard", DEFAULT_KEYBOARD);
        extraKeyboardOption = preferences.getInt("extra_keyboard", DEFAULT_KEYBOARD);
        fullScreen = preferences.getBoolean("full_screen", DEFAULT_FULL_SCREEN);

        // Density is the relationship between px and dp
        density = getResources().getDisplayMetrics().density;
        defaultKbSizeDp = Math.round(getResources().getDimension(R.dimen.key_height) / density);
        int keyboardSizeDp = preferences.getInt("keyboard_size", defaultKbSizeDp);
        keyboardSizePx = keyboardSizeDp * density;

        // Keyboard spinner
        Spinner keyboardSpinner = findViewById(R.id.keyboardSpinner);
        ArrayAdapter<CharSequence> arrayAdapter = ArrayAdapter.createFromResource(
                this, R.array.keyboard_options, android.R.layout.simple_spinner_item);
        arrayAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        keyboardSpinner.setAdapter(arrayAdapter);
        keyboardSpinner.setOnItemSelectedListener(this);
        keyboardSpinner.setSelection(keyboardOption);

        // Extra keyboard spinner
        Spinner extraKeyboardSpinner = findViewById(R.id.extraKeyboardSpinner);
        ArrayAdapter<CharSequence> extraArrayAdapter = ArrayAdapter.createFromResource(
                this, R.array.extra_keyboard_options, android.R.layout.simple_spinner_item);
        extraArrayAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        extraKeyboardSpinner.setAdapter(extraArrayAdapter);
        extraKeyboardSpinner.setOnItemSelectedListener(this);
        extraKeyboardSpinner.setSelection(extraKeyboardOption);

        // Keyboard size input
        ksizeEditText = findViewById(R.id.keyboardSize);
        ksizeEditText.setText(Integer.toString(keyboardSizeDp));
        ksizeEditText.addTextChangedListener(this);

        // Full screen switch
        fullScreenSwitch = findViewById(R.id.fullScreen);
        fullScreenSwitch.setChecked(fullScreen);
        fullScreenSwitch.setOnCheckedChangeListener(this);

        initFile = new File(getExternalFilesDir(null)+INIT_FILE);
        resetInitFile(false);
    }

    // Start game
    private void startGame(View v) {
        Intent intent = new Intent(getBaseContext(), DungeonCrawlStoneSoup.class);
        intent.putExtra("keyboard", keyboardOption);
        intent.putExtra("extra_keyboard", extraKeyboardOption);
        intent.putExtra("keyboard_size", Math.round(keyboardSizePx));
        intent.putExtra("full_screen", fullScreen);
        startActivity(intent);
    }

    // Reset the init file
    private void resetInitFile(boolean force) {
        if (!initFile.exists() || force) {
            try {
                FileWriter writer = new FileWriter(initFile);
                writer.close();
            } catch (IOException e) {
                Log.e(TAG, "Can't write init file: " + e.getMessage());
            }
        }
    }

    // Edit init file
    private void editInitFile(View v) {
        Intent intent = new Intent(getBaseContext(), DCSSTextEditor.class);
        intent.putExtra("file", initFile);
        startActivity(intent);
    }

    // Open morgue
    private void openMorgue(View v) {
        Intent intent = new Intent(getBaseContext(), DCSSMorgue.class);
        startActivity(intent);
    }

    // Open mods
    private void openMods(View v) {
        Intent intent = new Intent(getBaseContext(), DCSSMods.class);
        startActivity(intent);
    }

    // Keyboard changed
    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        if (parent.getId() == R.id.keyboardSpinner) {
            keyboardOption = position;
            preferences.edit().putInt("keyboard", position).apply();
        } else if (parent.getId() == R.id.extraKeyboardSpinner) {
            extraKeyboardOption = position;
            preferences.edit().putInt("extra_keyboard", position).apply();
        }
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
        // This shouldn't happen
    }

    @Override
    public void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

    @Override
    public void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {
        try {
            int keyboardSizeDp = Integer.parseInt(charSequence.toString());
            keyboardSizePx = keyboardSizeDp * density;
            preferences.edit().putInt("keyboard_size", keyboardSizeDp).apply();
        } catch (NumberFormatException e) {
            Log.e(TAG, "Invalid keyboard size: " + e.getMessage());
            keyboardSizePx = defaultKbSizeDp * density;
            ksizeEditText.setText(Integer.toString(defaultKbSizeDp));
        }
    }

    @Override
    public void afterTextChanged(Editable editable) {}

    @Override
    public void onCheckedChanged(CompoundButton compoundButton, boolean b) {
        fullScreen = b;
        preferences.edit().putBoolean("full_screen", fullScreen).apply();
    }

}
