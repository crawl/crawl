package org.develz.crawl;

import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Date;

public class DCSSMorgueAdapter extends RecyclerView.Adapter<DCSSMorgueAdapter.ViewHolder> {

    private File[] morgueFiles;

    private OnMorgueListener morgueListener;

    // Constructor
    public DCSSMorgueAdapter(File morgueDir, OnMorgueListener morgueListener) {
        if (!morgueDir.exists()) {
            if (!morgueDir.mkdir()) {
                Log.e(DCSSLauncher.TAG, "Can't create folder "+morgueDir.getPath());
            }
        }
        this.morgueFiles = morgueDir.listFiles();
        this.morgueListener = morgueListener;
    }

    // Single element in the RecyclerView
    public static class ViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener {
        private final TextView nameView;
        private final TextView timeView;
        private OnMorgueListener morgueListener;

        public ViewHolder(View view, OnMorgueListener morgueListener) {
            super(view);
            nameView = view.findViewById(R.id.fileName);
            timeView = view.findViewById(R.id.fileTime);
            view.setOnClickListener(this);
            this.morgueListener = morgueListener;
        }

        public TextView getNameView() {
            return nameView;
        }

        public TextView getTimeView() {
            return timeView;
        }

        @Override
        public void onClick(View v) {
            this.morgueListener.onMorgueClick(getBindingAdapterPosition());
        }
    }

    // Used by the layout manager to render each element in the RecyclerView
    @Override
    public ViewHolder onCreateViewHolder(ViewGroup viewGroup, int viewType) {
        View view = LayoutInflater.from(viewGroup.getContext())
                .inflate(R.layout.morgue_file, viewGroup, false);
        return new ViewHolder(view, morgueListener);
    }

    // Replace the contents of a view (invoked by the layout manager)
    @Override
    public void onBindViewHolder(ViewHolder viewHolder, final int position) {
        viewHolder.getNameView().setText(morgueFiles[position].getName());

        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
        Date modifiedDate = new Date(morgueFiles[position].lastModified());
        viewHolder.getTimeView().setText(dateFormat.format(modifiedDate));
    }

    // Return the size of your dataset (invoked by the layout manager)
    @Override
    public int getItemCount() {
        if (morgueFiles == null) {
            return 0;
        } else {
            return morgueFiles.length;
        }
    }

    // order 0: Number asc.
    // order 1: Number desc.
    // order 2: Time asc.
    // order 3: Time asc.
    public void sortMorgueFiles(int order) {
        int modifier = (order % 2 == 0) ? 1 : -1;
        if (order < 2) {
            Arrays.sort(morgueFiles, (a, b) -> a.getName().compareToIgnoreCase(b.getName())*modifier);
        } else {
            Arrays.sort(morgueFiles, (a, b) -> Long.compare(a.lastModified(), b.lastModified())*modifier);
        }
        notifyDataSetChanged();
    }

    public File getMorgueFile(int position) {
        if (position >= 0 && position < morgueFiles.length) {
            return morgueFiles[position];
        } else {
            return null;
        }
    }

    public interface OnMorgueListener{
        void onMorgueClick(int position);
    }

}
