package com.wn_klaymen1n.revc;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import java.io.File;
import java.util.ArrayList;

public class FileAdapter extends ArrayAdapter<File> {
    private ArrayList<File> fileList;
    private LayoutInflater inflater;
    private static final int VIEW_TYPE_BACK_BUTTON = 0;
    private static final int VIEW_TYPE_FILE = 1;
    public static final File BACK_BUTTON_MARKER = new File("");


    public FileAdapter(Context context, ArrayList<File>fileList) {
        super(context,0, fileList);
        this.fileList = fileList;
        inflater = LayoutInflater.from(context);
    }

    @Override
    public int getItemViewType(int position) {
        return fileList.get(position) == BACK_BUTTON_MARKER ? VIEW_TYPE_BACK_BUTTON : VIEW_TYPE_FILE;
    }

    @Override
    public int getViewTypeCount() {
        return 2;
    }

    static class ViewHolder{
        TextView textView;
        ImageView imageView;
    }
    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        int viewType = getItemViewType(position);
        ViewHolder holder;
        if(convertView == null)
        {
            if(viewType == VIEW_TYPE_BACK_BUTTON) {
                convertView = inflater.inflate(R.layout.back_button_layout, parent, false);
                return convertView;
            }
            else {
                convertView = inflater.inflate(R.layout.list_item, parent, false);
                holder = new ViewHolder();
                holder.imageView = convertView.findViewById(R.id.imageView);
                holder.textView = convertView.findViewById(android.R.id.text1);
                convertView.setTag(holder);

            }
        }

        if(viewType == VIEW_TYPE_BACK_BUTTON)
            return convertView;

        File dir = fileList.get(position);
        holder = (ViewHolder) convertView.getTag();
        holder.textView.setText(dir.getName());
        holder.imageView.setImageResource(R.drawable.folder);
        return convertView;
    }
}
