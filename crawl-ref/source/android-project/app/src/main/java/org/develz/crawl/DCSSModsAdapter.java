package org.develz.crawl;

import android.content.res.Resources;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import java.io.File;
import java.util.Arrays;

public class DCSSModsAdapter extends RecyclerView.Adapter<DCSSModsAdapter.ViewHolder> {

    private File modsDir;

    private File[] modsFiles;

    private int selected;

    private OnModsListener modsListener;

    // Constructor
    public DCSSModsAdapter(File modsDir, OnModsListener modsListener) {
        this.modsDir = modsDir;
        if (!modsDir.exists()) {
            if (!modsDir.mkdir()) {
                Log.e(DCSSLauncher.TAG, "Can't create folder "+modsDir.getPath());
            }
        }
        this.modsFiles = modsDir.listFiles();
        this.selected = -1;
        this.modsListener = modsListener;
    }

    // Single element in the RecyclerView
    public static class ViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener {
        private final TextView nameView;
        private OnModsListener modsListener;

        public ViewHolder(View view, OnModsListener modsListener) {
            super(view);
            nameView = view.findViewById(R.id.fileName);
            view.setOnClickListener(this);
            this.modsListener = modsListener;
        }

        public TextView getNameView() {
            return nameView;
        }

        @Override
        public void onClick(View v) {
            this.modsListener.onModsClick(getBindingAdapterPosition());
        }
    }

    // Used by the layout manager to render each element in the RecyclerView
    @Override
    public ViewHolder onCreateViewHolder(ViewGroup viewGroup, int viewType) {
        View view = LayoutInflater.from(viewGroup.getContext())
                .inflate(R.layout.mods_file, viewGroup, false);
        return new ViewHolder(view, modsListener);
    }

    // Replace the contents of a view (invoked by the layout manager)
    @Override
    public void onBindViewHolder(ViewHolder viewHolder, final int position) {
        viewHolder.getNameView().setText(modsFiles[position].getName());
        Resources resources = viewHolder.getNameView().getResources();
        if (position == selected) {
            viewHolder.getNameView().setBackgroundColor(resources.getColor(R.color.light_green));
        } else {
            viewHolder.getNameView().setBackgroundColor(resources.getColor(R.color.black));
        }
    }

    // Return the size of your dataset (invoked by the layout manager)
    @Override
    public int getItemCount() {
        if (modsFiles == null) {
            return 0;
        } else {
            return modsFiles.length;
        }
    }

    public void reloadModsFiles() {
        modsFiles = modsDir.listFiles();
        sortModsFiles();
    }

    public void sortModsFiles() {
        Arrays.sort(modsFiles, (a, b) -> a.getName().compareToIgnoreCase(b.getName()));
        notifyDataSetChanged();
    }

    public File getModsFile(int position) {
        if (position >= 0 && position < modsFiles.length) {
            return modsFiles[position];
        } else {
            return null;
        }
    }

    public File getSelectedFile() {
        return getModsFile(selected);
    }

    public boolean deleteSelectedFile() {
        if (getModsFile(selected).delete()) {
            selected = -1;
            reloadModsFiles();
            return true;
        } else {
            return false;
        }
    }

    public void setSelectedPosition(int selected) {
        if (selected != this.selected) {
            int oldSelected = this.selected;
            this.selected = selected;
            notifyItemChanged(oldSelected);
            notifyItemChanged(selected);
        }
    }

    public interface OnModsListener{
        void onModsClick(int position);
    }

}
