package org.develz.crawl;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.util.Log;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.TextView;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class DCSSMods extends AppCompatActivity implements DCSSModsAdapter.OnModsListener {

    private static final String MODS_DIR = "/dat";

    // Progress bar
    private View progress;

    // RecyclerView Adapter
    DCSSModsAdapter adapter;

    // Status messages
    private TextView status;

    // Mods directory
    private File modsDir;

    // Buttons that come and go
    private Button modsDownload;
    private Button modsDelete;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.mods);

        progress = findViewById(R.id.progressBar);
        ArrayAdapter<CharSequence> arrayAdapter = ArrayAdapter.createFromResource(
                this, R.array.sort_options, android.R.layout.simple_spinner_item);
        arrayAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        status = findViewById(R.id.status);

        RecyclerView recyclerView = findViewById(R.id.recyclerView);
        LinearLayoutManager layoutManager = new LinearLayoutManager(getBaseContext());
        recyclerView.setLayoutManager(layoutManager);

        findViewById(R.id.modsBack).setOnClickListener(this::onClickClose);
        findViewById(R.id.modsUpload).setOnClickListener(this::onClickUpload);
        modsDownload = findViewById(R.id.modsDownload);
        modsDownload.setOnClickListener(this::onClickDownload);
        modsDelete = findViewById(R.id.modsDelete);
        modsDelete.setOnClickListener(this::onClickDelete);

        modsDir = new File(getExternalFilesDir(null) + MODS_DIR);
        adapter = new DCSSModsAdapter(modsDir, this);
        recyclerView.setAdapter(adapter);
        adapter.sortModsFiles();
        progress.setVisibility(View.GONE);
    }

    // Select mod
    @Override
    public void onModsClick(int position) {
        Log.d(DCSSLauncher.TAG, "Mods item selected: " + position);
        File mods = adapter.getModsFile(position);
        if (mods != null) {
            Log.d(DCSSLauncher.TAG, "File selected: " + mods.getName());
            if (adapter.getSelectedFile() == null) {
                modsDownload.setVisibility(View.VISIBLE);
                modsDownload.setEnabled(true);
                modsDelete.setVisibility(View.VISIBLE);
                modsDelete.setEnabled(true);
            }
            adapter.setSelectedPosition(position);
        }
    }

    private void setOkText(int resource) {
        status.setText(resource);
        status.setTextColor(getResources().getColor(R.color.light_green));
    }

    private void setErrorText(int resource) {
        status.setText(resource);
        status.setTextColor(getResources().getColor(R.color.error));
    }

    // Close button
    private void onClickClose(View v) {
        finish();
    }

    // Upload button
    private void onClickUpload(View v) {
        status.setText("");
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        uploadActivityResultLauncher.launch(new String[]{"*/*"}); // Any mime type
    }

    // Handling the upload
    // Let the user select a file and confirm overwrites
    private ActivityResultLauncher<String[]> uploadActivityResultLauncher = registerForActivityResult(
        new ActivityResultContracts.OpenDocument(), uri -> {
            if (uri != null) {
                String fileName = getUriName(uri);
                Log.i(DCSSLauncher.TAG, "Uploading file: " + fileName);
                File outputFile = new File(modsDir, fileName);
                if (outputFile.exists()) {
                    AlertDialog.Builder builder = new AlertDialog.Builder(DCSSMods.this);
                    builder.setMessage(getString(R.string.upload_confirm, fileName));
                    builder.setCancelable(false);
                    builder.setPositiveButton(R.string.yes, (dialog, which) -> {
                        uploadCopy(uri, outputFile);
                    });
                    builder.setNegativeButton(R.string.no, (dialog, which) -> {
                        dialog.cancel();
                    });
                    builder.create().show();
                } else {
                    uploadCopy(uri, outputFile);
                }
            }
        });

    // Get the file name from Uri
    private String getUriName(Uri uri) {
        String name = null;
        String[] projection = new String[] { OpenableColumns.DISPLAY_NAME };
        Cursor returnCursor = getContentResolver().query(uri, projection, null, null, null);
        if (returnCursor != null) {
            returnCursor.moveToFirst();
            name = returnCursor.getString(0);
            returnCursor.close();
        }
        return name;
    }

    // Copy data from Uri to a File in the mods folder
    private void uploadCopy(Uri uri, File outputFile) {
        try {
            progress.setVisibility(View.VISIBLE);
            InputStream input = getContentResolver().openInputStream(uri);
            if (input == null) {
                throw new IOException("No input");
            }
            FileOutputStream output = new FileOutputStream(outputFile);
            simpleCopy(input, output);
            adapter.reloadModsFiles();
            setOkText(R.string.upload_ok);
        } catch (IOException e) {
            Log.e(DCSSLauncher.TAG, "Error uploading file", e);
            setErrorText(R.string.upload_error);
        } finally {
            progress.setVisibility(View.GONE);
        }
    }

    // Copy data from InputStream to OutputStream
    // Going old school to preserve compatibility with older devices
    private void simpleCopy(InputStream input, OutputStream output) throws IOException {
        byte[] buffer = new byte[8192];
        int readed;
        while ((readed = input.read(buffer)) != -1) {
            output.write(buffer, 0, readed);
        }
        output.close();
        input.close();
    }

    // Download button
    private void onClickDownload(View v) {
        status.setText("");
        File fileSelected = adapter.getSelectedFile();
        if (fileSelected != null) {
            String fileName = fileSelected.getName();
            Log.i(DCSSLauncher.TAG, "Downloading file: " + fileName);
            Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("application/octet-stream");
            intent.putExtra(Intent.EXTRA_TITLE, fileName);
            downloadActivityResultLauncher.launch(intent);
        }
    }

    // Handling the download
    // Save the file in the destination chosen by the user
    private ActivityResultLauncher<Intent> downloadActivityResultLauncher = registerForActivityResult(
        new ActivityResultContracts.StartActivityForResult(),
        result -> {
            if (result.getResultCode() == Activity.RESULT_OK) {
                // There are no request codes
                Intent resultData = result.getData();
                if (resultData != null && resultData.getData() != null) {
                    try {
                        progress.setVisibility(View.VISIBLE);
                        Uri uri = resultData.getData();
                        Log.i(DCSSLauncher.TAG, "Destination: " + uri.toString());
                        File fileSelected = adapter.getSelectedFile();
                        FileInputStream input = new FileInputStream(fileSelected);
                        OutputStream output = getContentResolver().openOutputStream(uri);
                        if (output == null) {
                            throw new IOException("No output");
                        }
                        simpleCopy(input, output);
                        setOkText(R.string.download_ok);
                    } catch (IOException e) {
                        Log.e(DCSSLauncher.TAG, "Can't download file: " + e.getMessage());
                        setErrorText(R.string.download_error);
                    } finally {
                        progress.setVisibility(View.GONE);
                    }
                }
            }
        });

    // Delete button
    private void onClickDelete(View v) {
        status.setText("");
        File fileSelected = adapter.getSelectedFile();
        if (fileSelected != null) {
            AlertDialog.Builder builder = new AlertDialog.Builder(DCSSMods.this);
            builder.setMessage(getString(R.string.delete_confirm, fileSelected.getName()));
            builder.setCancelable(false);
            builder.setPositiveButton(R.string.yes, (dialog, which) -> {
                progress.setVisibility(View.VISIBLE);
                if (adapter.deleteSelectedFile()) {
                    setOkText(R.string.delete_ok);
                    modsDownload.setVisibility(View.INVISIBLE);
                    modsDownload.setEnabled(false);
                    modsDelete.setVisibility(View.INVISIBLE);
                    modsDelete.setEnabled(false);
                } else {
                    setErrorText(R.string.delete_error);
                }
                progress.setVisibility(View.GONE);
            });
            builder.setNegativeButton(R.string.no, (dialog, which) -> {
                dialog.cancel();
            });
            builder.create().show();
        }
    }

}
