package org.develz.crawl;

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
import java.nio.CharBuffer;

public class DCSSTextBase extends AppCompatActivity {

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

    // Close the activity without saving
    protected void close() {
        finish();
    }

}
