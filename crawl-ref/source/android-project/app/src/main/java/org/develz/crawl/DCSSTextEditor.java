package org.develz.crawl;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.EditText;

import java.io.File;

public class DCSSTextEditor extends DCSSTextBase {

    private static final String
            OPTIONS_GUIDE = "docs/options_guide.txt";

    private File file;

    private EditText editor;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.text_editor);

        Intent intent = getIntent();
        file = (File) intent.getSerializableExtra("file");

        editor = findViewById(R.id.textEditor);

        findViewById(R.id.saveButton).setOnClickListener(this::onClickSave);
        findViewById(R.id.cancelButton).setOnClickListener(this::onClickClose);
        findViewById(R.id.helpButton).setOnClickListener(this::onClickHelp);

        openFile(file, editor);
    }

    // Save button
    private void onClickSave(View v) {
        saveFile(file, editor);
    }

    // Close button
    private void onClickClose(View v) {
        close();
    }

    // Help button
    private void onClickHelp(View v) {
        Intent intent = new Intent(getBaseContext(), DCSSTextViewer.class);
        intent.putExtra("asset", OPTIONS_GUIDE);
        startActivity(intent);
    }
}
