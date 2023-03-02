package org.develz.crawl;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;

import androidx.appcompat.app.AppCompatActivity;

public class DCSSLauncher extends AppCompatActivity {

    public final static String TAG = "LAUNCHER";

    private static final String INIT_FILE = "/init.txt";

    private File initFile;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.launcher);

        findViewById(R.id.startButton).setOnClickListener(this::startGame);
        findViewById(R.id.editInitFile).setOnClickListener(this::editInitFile);
        findViewById(R.id.morgueButton).setOnClickListener(this::openMorgue);

        initFile = new File(getExternalFilesDir(null)+INIT_FILE);
        resetInitFile(false);
    }

    // Start game
    private void startGame(View v) {
        Intent intent = new Intent(getBaseContext(), DungeonCrawlStoneSoup.class);
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

}
