package org.develz.crawl;

import android.content.Intent;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.view.View;
import android.widget.TextView;

import java.io.File;

public class DCSSTextViewer extends DCSSTextBase {

    private TextView viewer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.text_viewer);

        Intent intent = getIntent();
        File file = (File) intent.getSerializableExtra("file");
        String asset = intent.getStringExtra("asset");

        viewer = findViewById(R.id.textViewer);
        viewer.setMovementMethod(new ScrollingMovementMethod());

        findViewById(R.id.backButton).setOnClickListener(this::onClickClose);

        if (file != null) {
            openFile(file, viewer);
        }

        if (asset != null) {
            openAsset(asset, viewer);
        }
    }

    // Close button
    private void onClickClose(View v) {
        close();
    }

}
