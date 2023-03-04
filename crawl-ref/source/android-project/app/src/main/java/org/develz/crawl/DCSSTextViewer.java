package org.develz.crawl;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.io.File;

public class DCSSTextViewer extends DCSSTextBase {

    private TextView viewer;

    private TextView downloadStatus;

    private String fileName;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.text_viewer);

        Intent intent = getIntent();
        File file = (File) intent.getSerializableExtra("file");
        String asset = intent.getStringExtra("asset");
        boolean download = intent.getBooleanExtra("download", false);

        viewer = findViewById(R.id.textViewer);
        viewer.setMovementMethod(new ScrollingMovementMethod());
        downloadStatus = findViewById(R.id.downloadStatus);

        findViewById(R.id.backButton).setOnClickListener(this::onClickClose);
        Button downloadButton = findViewById(R.id.downloadButton);
        downloadButton.setOnClickListener(this::onClickDownload);

        if (file != null) {
            openFile(file, viewer);
            fileName = file.getName();
        }

        if (asset != null) {
            openAsset(asset, viewer);
            fileName = new File(asset).getName();
        }

        // Storage Access Framework is only available since Android 4.4
        if (download && (Build.VERSION.SDK_INT >= 19)) {
            downloadButton.setVisibility(View.VISIBLE);
        }
    }

    // Close button
    private void onClickClose(View v) {
        close();
    }

    // Download button
    private void onClickDownload(View v) {
        downloadFile(viewer, fileName);
    }

    @Override
    protected void onDownloadOk() {
        super.onDownloadOk();
        downloadStatus.setText(R.string.download_ok);
        downloadStatus.setTextColor(getResources().getColor(R.color.light_green));
    }

    @Override
    protected void onDownloadError() {
        super.onDownloadError();
        downloadStatus.setText(R.string.download_error);
        downloadStatus.setTextColor(getResources().getColor(R.color.error));
    }

}
