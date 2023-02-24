package org.develz.crawl;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

public class DCSSLauncher extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.launcher);
    }

    // Start game
    public void startGame(android.view.View view) {
        try {
            Intent intent = new Intent(getBaseContext(), DungeonCrawlStoneSoup.class);
            startActivity(intent);
        } finally {
            finish();
        }
    }
}