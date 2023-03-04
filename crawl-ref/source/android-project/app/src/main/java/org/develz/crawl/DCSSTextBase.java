package org.develz.crawl;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.widget.EditText;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.nio.CharBuffer;

public class DCSSTextBase extends AppCompatActivity {

    private static final int CREATE_FILE = 1;

    private TextView textToDownload;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    // Load the file
    protected void openFile(File file, TextView text) {
        try {
            Log.i(DCSSLauncher.TAG, "Opening file: " + file.getAbsolutePath());
            FileReader reader = new FileReader(file);
            CharBuffer buffer = CharBuffer.allocate((int) file.length());
            reader.read(buffer);
            reader.close();
            buffer.rewind();
            text.setText(buffer);
        } catch (IOException e) {
            Log.e(DCSSLauncher.TAG, "Can't open file: " + e.getMessage());
        }
    }

    // Save the file and close the activity
    protected void saveFile(File file, EditText text) {
        try {
            Log.i(DCSSLauncher.TAG, "Saving file: " + file.getAbsolutePath());
            FileWriter writer = new FileWriter(file);
            CharSequence buffer = text.getText();
            writer.append(buffer);
            writer.close();
        } catch (IOException e) {
            Log.e(DCSSLauncher.TAG, "Can't save file: " + e.getMessage());
        } finally {
            close();
        }
    }

    // Load the asset
    protected void openAsset(String asset, TextView text) {
        try {
            Log.i(DCSSLauncher.TAG, "Opening asset: " + asset);
            InputStream inputStream = getAssets().open(asset);
            byte[] buffer = new byte[inputStream.available()];
            inputStream.read(buffer);
            text.setText(new String(buffer));
        } catch (IOException e) {
            Log.e(DCSSLauncher.TAG, "Can't open asset: " + e.getMessage());
        }
    }

    // Download the file
    // Actually show a file picker to save the file
    protected void downloadFile(TextView text, String fileName) {
        Log.i(DCSSLauncher.TAG, "Downloading file: " + fileName);
        textToDownload = text;
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_TITLE, fileName);
        startActivityForResult(intent, CREATE_FILE);
    }

    // Handling the download
    // Save the file in the destination chosen by the user
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent resultData) {
        super.onActivityResult(requestCode, resultCode, resultData);
        try {
            if (requestCode == CREATE_FILE && resultCode == Activity.RESULT_OK) {
                if (resultData != null) {
                    Uri uri = resultData.getData();
                    Log.i(DCSSLauncher.TAG, "Destination: " + uri.toString());
                    OutputStream outputStream = getContentResolver().openOutputStream(uri);
                    OutputStreamWriter writer = new OutputStreamWriter(outputStream);
                    writer.append(textToDownload.getText());
                    writer.close();
                    onDownloadOk();
                }
            }
        } catch (IOException e) {
            Log.e(DCSSLauncher.TAG, "Can't download file: " + e.getMessage());
            onDownloadError();
        }
    }

    // Overridden by children to show download results
    protected void onDownloadOk() {};
    protected void onDownloadError() {};

    // Close the activity without saving
    protected void close() {
        finish();
    }

}
