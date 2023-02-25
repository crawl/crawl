package org.develz.crawl;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;

public class DCSSLauncher extends AppCompatActivity implements View.OnClickListener {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.launcher);

        findViewById(R.id.startButton).setOnClickListener(this);
    }

    // Launcher button pressed
    @Override
    public void onClick(View v) {
        if (v.getId() == R.id.startButton) {
            startGame();
        }
    }

    // Start game
    private void startGame() {
        try {
            Intent intent = new Intent(getBaseContext(), DungeonCrawlStoneSoup.class);
            startActivity(intent);
        } finally {
            finish();
        }
    }

}