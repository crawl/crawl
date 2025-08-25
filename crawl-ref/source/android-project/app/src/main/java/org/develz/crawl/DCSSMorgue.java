package org.develz.crawl;

import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;

import java.io.File;

public class DCSSMorgue extends AppCompatActivity
        implements AdapterView.OnItemSelectedListener, DCSSMorgueAdapter.OnMorgueListener {

    private static final String MORGUE_DIR = "/morgue";

    private static final int DEFAULT_ORDER = 3;

    // Progress bar
    private View progress;

    // RecyclerView Adapter
    DCSSMorgueAdapter adapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.morgue);

        progress = findViewById(R.id.progressBar);
        Spinner sortSpinner = findViewById(R.id.sortSpinner);
        ArrayAdapter<CharSequence> arrayAdapter = ArrayAdapter.createFromResource(
                this, R.array.sort_options, android.R.layout.simple_spinner_item);
        arrayAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        sortSpinner.setAdapter(arrayAdapter);
        sortSpinner.setOnItemSelectedListener(this);
        sortSpinner.setSelection(DEFAULT_ORDER);

        RecyclerView recyclerView = findViewById(R.id.recyclerView);
        recyclerView.setHasFixedSize(true);
        LinearLayoutManager layoutManager = new LinearLayoutManager(getBaseContext());
        recyclerView.setLayoutManager(layoutManager);

        findViewById(R.id.morgueBack).setOnClickListener(this::onClickClose);

        File morgueDir = new File(getExternalFilesDir(null)+MORGUE_DIR);
        adapter = new DCSSMorgueAdapter(morgueDir, this);
        recyclerView.setAdapter(adapter);
        adapter.sortMorgueFiles(DEFAULT_ORDER);
        progress.setVisibility(View.GONE);
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        progress.setVisibility(View.VISIBLE);
        adapter.sortMorgueFiles(position);
        progress.setVisibility(View.GONE);
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
        // This shouldn't happen
    }

    // Show morgue file
    @Override
    public void onMorgueClick(int position) {
        Log.e(DCSSLauncher.TAG, "Morgue item selected: " + position);
        File morgue = adapter.getMorgueFile(position);
        if (morgue != null) {
            Intent intent = new Intent(getBaseContext(), DCSSTextViewer.class);
            intent.putExtra("file", morgue);
            intent.putExtra("download", true);
            startActivity(intent);
        }
    }

    // Close button
    private void onClickClose(View v) {
        finish();
    }

}
