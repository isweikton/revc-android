package com.wn_klaymen1n.revc;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Environment;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.os.Build;
import android.view.MenuItem;
import android.view.Window;
import android.view.View;
import android.Manifest;
import android.content.pm.PackageManager;
import android.content.pm.ActivityInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.PopupMenu;
import android.widget.Toast;
import android.provider.Settings;
import android.net.Uri;

import org.libsdl.app.SDLActivity;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.File;
import java.util.Arrays;
import java.util.List;

public class LauncherActivity extends Activity {
    public static native void setenv(String value);
    private static String currentGamePath;
    private static final List<String> MOBILE_UI_ASSETS = Arrays.asList(
            "mobileui/ArcadeJoystick_Base.png",
            "mobileui/hud_circle.png",
            "mobileui/hud_analognub.png",
            "mobileui/hud_car.png",
            "mobileui/punch.png",
            "mobileui/sprint.png",
            "mobileui/hud_left.png",
            "mobileui/hud_right.png",
            "mobileui/brake.png",
            "mobileui/accelerate.png"
    );

    static public EditText editText;
    private static final int REQUEST_PERMISSION = 1001;
    private static final int REQUEST_MANAGE_STORAGE = 1002;
    private static LauncherActivity lastInstance;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        lastInstance = this;
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        setContentView(R.layout.activity_main);

        SharedPreferences prefs = getSharedPreferences("app_prefs", MODE_PRIVATE);

        editText = findViewById(R.id.editText);
        String savedPath = prefs.getString("game_path", "");
        if(savedPath == "")
            savedPath = "/storage/emulated/0/revc";
        editText.setText(savedPath);

        editText.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                String newText = s.toString();
                getSharedPreferences("app_prefs", MODE_PRIVATE)
                        .edit()
                        .putString("game_path", newText)
                        .apply();
            }
        });

        ImageView menuButton = findViewById(R.id.menuIcon);
        menuButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showMenu();
            }
        });

        Button browseButton = findViewById(R.id.browseButton);
        browseButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent intent = new Intent(LauncherActivity.this, FilepickerActivity.class);
                startActivityForResult(intent, 123);
            }
        });

        Button launchButton = findViewById(R.id.launchButton);
        launchButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                startGta();
            }
        });

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                requestAllFilesAccess();
            }
        } else {
            if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, REQUEST_PERMISSION);
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            Window window = getWindow();
            window.getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            );
        }
    }

    public static void openSettingsFromNative() {
        if (lastInstance == null) return;
        Intent intent = new Intent(lastInstance, SettingsActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        lastInstance.startActivity(intent);
    }

    private void requestAllFilesAccess() {
        Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
        intent.setData(Uri.parse("package:" + getPackageName()));
        startActivityForResult(intent, REQUEST_MANAGE_STORAGE);
        Toast.makeText(this, "Please grant 'All Files Access'", Toast.LENGTH_LONG).show();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_PERMISSION) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Toast.makeText(this, "Permission granted", Toast.LENGTH_SHORT).show();
            } else {
                Toast.makeText(this, "No permissions", Toast.LENGTH_SHORT).show();
            }
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        switch (requestCode)
        {
            case REQUEST_MANAGE_STORAGE: {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && Environment.isExternalStorageManager()) {
                    Toast.makeText(this, "All Files Access granted", Toast.LENGTH_SHORT).show();
                } else {
                    Toast.makeText(this, "All Files Access denied", Toast.LENGTH_SHORT).show();
                }
            }
            case 123:
                if (resultCode == RESULT_OK) {

                    editText.setText(data.getStringExtra("path"));
                }

        }
    }

    public void startGta() {
        String gamepath = editText.getText().toString();
        File file = new File(gamepath + "/models/gta3.img");
        if(!file.exists())
        {
            AlertDialog.Builder dlgAlert  = new AlertDialog.Builder(this);
            dlgAlert.setMessage("An error occurred while trying to start the application."
                + System.getProperty("line.separator")
                + System.getProperty("line.separator")
                + "Error: " + "gta3.img not found. Check your file path");
                dlgAlert.setTitle("Game files not found");
                dlgAlert.setPositiveButton("Exit",
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog,int id) {
                    }
                });
        dlgAlert.setCancelable(false);
        dlgAlert.create().show();
        return;
        }
        setCurrentGamePath(gamepath);
        Intent intent = new Intent(LauncherActivity.this, LoadingActivity.class);
        intent.putExtra(LoadingActivity.EXTRA_GAME_PATH, gamepath);
        startActivity(intent);
    }

    static public void initEnv() {
        String gamepath = currentGamePath;
        if ((gamepath == null || gamepath.isEmpty()) && editText != null) {
            gamepath = editText.getText().toString();
        }
        if (gamepath == null || gamepath.isEmpty()) {
            gamepath = "/storage/emulated/0/revc";
        }
        setenv(gamepath);
        File file = new File(gamepath);
        Log.d("REVC", "Game directory: " + file.exists());
    }

    public static void setCurrentGamePath(String gamepath) {
        currentGamePath = gamepath;
    }

    public static void copyMobileUiAssets(Activity activity, String gamepath) {
        File outDir = new File(gamepath, "mobileui");
        if (!outDir.exists() && !outDir.mkdirs()) {
            Log.w("REVC", "Failed to create mobileui directory: " + outDir.getAbsolutePath());
            return;
        }

        for (String assetName : MOBILE_UI_ASSETS) {
            File outFile = new File(gamepath, assetName);
            if (outFile.exists() && outFile.length() > 0) {
                continue;
            }

            File parent = outFile.getParentFile();
            if (parent != null && !parent.exists() && !parent.mkdirs()) {
                Log.w("REVC", "Failed to create asset parent: " + parent.getAbsolutePath());
                continue;
            }

            try (InputStream in = activity.getAssets().open(assetName);
                 OutputStream out = new FileOutputStream(outFile)) {
                byte[] buffer = new byte[8192];
                int read;
                while ((read = in.read(buffer)) != -1) {
                    out.write(buffer, 0, read);
                }
            } catch (IOException e) {
                Log.w("REVC", "Failed to copy asset " + assetName + " to " + outFile.getAbsolutePath(), e);
            }
        }
    }

    void showMenu() {
        PopupMenu popupMenu = new PopupMenu(LauncherActivity.this, findViewById(R.id.menuIcon));
        popupMenu.getMenuInflater().inflate(R.menu.menu_main, popupMenu.getMenu());
        popupMenu.show();


        popupMenu.setOnMenuItemClickListener(new PopupMenu.OnMenuItemClickListener() {
            @Override
            public boolean onMenuItemClick(MenuItem item) {
                if (item.getItemId() == R.id.action_menu) {
                    AlertDialog.Builder builder = new AlertDialog.Builder(LauncherActivity.this);
                    builder.setMessage(R.string.about_text)
                            .setTitle(R.string.action_about);
                    AlertDialog dialog = builder.create();
                    dialog.show();
                    return true;
                } else
                    return false;
            }
        });
    }
}
