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

    private TextView status;

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
        status = findViewById(R.id.status);

        findViewById(R.id.backButton).setOnClickListener(this::onClickClose);
        Button downloadButton = findViewById(R.id.downloadButton);
        downloadButton.setOnClickListener(this::onClickDownload);

        if (file != null) {
            if (openFile(file, viewer)) {
                fileName = file.getName();
            } else {
                status.setText(R.string.open_error);
            }
        }

        if (asset != null) {
            if (openAsset(asset, viewer)) {
                fileName = new File(asset).getName();
            } else {
                status.setText(R.string.open_error);
            }
        }

        // Storage Access Framework is only available since Android 4.4
        if (download && (Build.VERSION.SDK_INT >= 19)) {
            downloadButton.setVisibility(View.VISIBLE);
        }
    }

    // Close button
    private void onClickClose(View v) {
        status.setText("");
        close();
    }

    // Download button
    private void onClickDownload(View v) {
        status.setText("");
        downloadFile(viewer, fileName);
    }

    @Override
    protected void onDownloadOk() {
        status.setText(R.string.download_ok);
        status.setTextColor(getResources().getColor(R.color.light_green));
    }

    @Override
    protected void onDownloadError() {
        status.setText(R.string.download_error);
        status.setTextColor(getResources().getColor(R.color.error));
    }

}
