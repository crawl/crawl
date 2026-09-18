package org.develz.crawl;

import android.content.res.Resources;
import android.graphics.Color;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
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

    // Returns the selected item
    public int getSelected() {
        return selected;
    }

    // Single element in the RecyclerView
    public static class ViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener, View.OnFocusChangeListener {
        private final LinearLayout layout;
        private final TextView nameView;
        private OnModsListener modsListener;

        public ViewHolder(View view, OnModsListener modsListener) {
            super(view);
            layout = view.findViewById(R.id.layout);
            nameView = view.findViewById(R.id.fileName);
            view.setOnClickListener(this);
            view.setOnFocusChangeListener(this);
            this.modsListener = modsListener;
        }

        public LinearLayout getLayout() {
            return layout;
        }

        public TextView getNameView() {
            return nameView;
        }

        @Override
        public void onClick(View v) {
            this.modsListener.onModsClick(getBindingAdapterPosition());
        }

        @Override
        public void onFocusChange(View view, boolean b) {
            Resources resources = getNameView().getResources();
            int selected = ((DCSSModsAdapter)getBindingAdapter()).getSelected();
            if (getBindingAdapterPosition() == selected) {
                if (view.isFocused()) {
                    getLayout().setBackgroundColor(resources.getColor(R.color.dark_green_focused));
                } else {
                    getLayout().setBackgroundColor(resources.getColor(R.color.dark_green));
                }
            } else {
                if (view.isFocused()) {
                    getLayout().setBackgroundColor(resources.getColor(R.color.black_focused));
                } else {
                    getLayout().setBackgroundColor(resources.getColor(R.color.black));
                }
            }
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
            viewHolder.getLayout().setBackgroundColor(resources.getColor(R.color.dark_green_focused));
        } else {
            viewHolder.getLayout().setBackgroundColor(resources.getColor(R.color.black));
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
