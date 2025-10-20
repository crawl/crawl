package org.develz.crawl;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import java.io.File;

public class DCSSTextEditor extends DCSSTextBase {

    private static final String OPTIONS_GUIDE = "docs/options_guide.txt";

    private File file;

    private EditText editor;

    private TextView status;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.text_editor);

        Intent intent = getIntent();
        file = (File) intent.getSerializableExtra("file");

        editor = findViewById(R.id.textEditor);
        status = findViewById(R.id.status);

        findViewById(R.id.saveButton).setOnClickListener(this::onClickSave);
        findViewById(R.id.cancelButton).setOnClickListener(this::onClickClose);
        findViewById(R.id.helpButton).setOnClickListener(this::onClickHelp);

        if (!openFile(file, editor)) {
            status.setText(R.string.open_error);
        }
    }

    // Save button
    private void onClickSave(View v) {
        status.setText("");
        if (saveFile(file, editor)) {
            close();
        } else {
            status.setText(R.string.save_error);
        }
    }

    // Close button
    private void onClickClose(View v) {
        status.setText("");
        close();
    }

    // Help button
    private void onClickHelp(View v) {
        status.setText("");
        Intent intent = new Intent(getBaseContext(), DCSSTextViewer.class);
        intent.putExtra("asset", OPTIONS_GUIDE);
        startActivity(intent);
    }

    @Override
    protected void onDownloadOk() {}

    @Override
    protected void onDownloadError() {}

}
