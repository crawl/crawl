package org.develz.crawl;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

import androidx.appcompat.app.AppCompatActivity;

public class DCSSLauncher extends AppCompatActivity implements AdapterView.OnItemSelectedListener {

    public final static String TAG = "LAUNCHER";

    private static final String INIT_FILE = "/init.txt";

    private static final int DEFAULT_KEYBOARD = 0;

    // Crawl's init file
    private File initFile;

    // Android options
    private SharedPreferences preferences;

    // Current keyboard type
    private int keyboardOption;

    @Override
    protected void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);
        setContentView(R.layout.launcher);

        findViewById(R.id.startButton).setOnClickListener(this::startGame);
        findViewById(R.id.editInitFile).setOnClickListener(this::editInitFile);
        findViewById(R.id.morgueButton).setOnClickListener(this::openMorgue);

        preferences = getPreferences(Context.MODE_PRIVATE);
        keyboardOption = preferences.getInt("keyboard", DEFAULT_KEYBOARD);

        Spinner keyboardSpinner = findViewById(R.id.keyboardSpinner);
        ArrayAdapter<CharSequence> arrayAdapter = ArrayAdapter.createFromResource(
                this, R.array.keyboard_options, android.R.layout.simple_spinner_item);
        arrayAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        keyboardSpinner.setAdapter(arrayAdapter);
        keyboardSpinner.setOnItemSelectedListener(this);
        keyboardSpinner.setSelection(keyboardOption);

        initFile = new File(getExternalFilesDir(null)+INIT_FILE);
        resetInitFile(false);
    }

    // Start game
    private void startGame(View v) {
        Intent intent = new Intent(getBaseContext(), DungeonCrawlStoneSoup.class);
        intent.putExtra("keyboard", keyboardOption);
        startActivity(intent);
        finish();
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

    // Keyboard changed
    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        keyboardOption = position;
        preferences.edit().putInt("keyboard", position).apply();
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
        // This shouldn't happen
    }

}
