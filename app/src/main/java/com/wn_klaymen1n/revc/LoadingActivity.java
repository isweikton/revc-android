package com.wn_klaymen1n.revc;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.TextView;

import com.google.android.material.progressindicator.LinearProgressIndicator;

import org.libsdl.app.SDLActivity;

import java.io.File;

public class LoadingActivity extends Activity {
    public static final String EXTRA_GAME_PATH = "game_path";

    private final Handler handler = new Handler(Looper.getMainLooper());
    private LinearProgressIndicator progressIndicator;
    private TextView statusText;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        setContentView(R.layout.activity_loading);

        progressIndicator = findViewById(R.id.loadingProgress);
        statusText = findViewById(R.id.loadingStatus);

        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        );

        String gamePath = getIntent().getStringExtra(EXTRA_GAME_PATH);
        if (gamePath == null || gamePath.isEmpty()) {
            finish();
            return;
        }

        LauncherActivity.setCurrentGamePath(gamePath);
        new Thread(() -> prepareAndLaunch(gamePath), "revc-loader").start();
    }

    private void prepareAndLaunch(String gamePath) {
        postProgress(10, getString(R.string.loading_checking_files));

        File gameImg = new File(gamePath, "models/gta3.img");
        if (!gameImg.exists()) {
            handler.post(this::finish);
            return;
        }

        sleepBriefly(120);
        postProgress(35, getString(R.string.loading_copying_assets));
        LauncherActivity.copyMobileUiAssets(this, gamePath);

        sleepBriefly(160);
        postProgress(70, getString(R.string.loading_preparing_runtime));
        LauncherActivity.setCurrentGamePath(gamePath);

        sleepBriefly(140);
        postProgress(100, getString(R.string.loading_starting_game));
        sleepBriefly(180);

        handler.post(() -> {
            Intent intent = new Intent(LoadingActivity.this, SDLActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
            startActivity(intent);
            finish();
        });
    }

    private void postProgress(int progress, String status) {
        handler.post(() -> {
            progressIndicator.setProgressCompat(progress, true);
            statusText.setText(status);
        });
    }

    private void sleepBriefly(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException ignored) {
            Thread.currentThread().interrupt();
        }
    }
}
