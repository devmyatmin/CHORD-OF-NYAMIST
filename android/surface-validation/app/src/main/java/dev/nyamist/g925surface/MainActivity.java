package dev.nyamist.g925surface;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;

public final class MainActivity extends Activity implements SurfaceHolder.Callback {
    static {
        System.loadLibrary("nyamist-surface-validation");
    }

    private TextView statusView;
    private TextView reportView;
    private boolean validatedCurrentSurface;
    private String latestReport = "";

    private static native String validateSurfaceNative(Surface surface, int width, int height);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(12);
        root.setPadding(pad, pad, pad, pad);

        TextView title = new TextView(this);
        title.setText("CHORD OF NYAMIST — G925 R1.5");
        title.setTextSize(20.0f);
        root.addView(title, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        statusView = new TextView(this);
        statusView.setText("Waiting for Android SurfaceView...");
        statusView.setTextSize(14.0f);
        root.addView(statusView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        SurfaceView surfaceView = new SurfaceView(this);
        root.addView(surfaceView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dp(220)));
        surfaceView.getHolder().addCallback(this);

        Button copyButton = new Button(this);
        copyButton.setText("Copy JSON report");
        copyButton.setOnClickListener(view -> copyReport());
        root.addView(copyButton, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        ScrollView scroll = new ScrollView(this);
        reportView = new TextView(this);
        reportView.setTextSize(11.0f);
        reportView.setTextIsSelectable(true);
        scroll.addView(reportView);
        root.addView(scroll, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1.0f));

        setContentView(root);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        validatedCurrentSurface = false;
        statusView.setText("Android Surface created; waiting for dimensions...");
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (validatedCurrentSurface || width <= 0 || height <= 0) return;
        validatedCurrentSurface = true;
        statusView.setText("Running Vulkan surface / swapchain validation...");
        final Surface surface = holder.getSurface();
        final int surfaceWidth = width;
        final int surfaceHeight = height;

        new Thread(() -> {
            String json;
            try {
                json = validateSurfaceNative(surface, surfaceWidth, surfaceHeight);
            } catch (RuntimeException error) {
                json = "{\"status\":\"failed\",\"failure_reason\":\""
                        + error.getClass().getSimpleName() + "\"}";
            }
            saveReport(json);
            final String result = json;
            runOnUiThread(() -> {
                latestReport = result;
                reportView.setText(result);
                statusView.setText(result.contains("\"status\": \"success\"")
                        ? "R1.5 Vulkan surface/swapchain: SUCCESS"
                        : "R1.5 validation did not pass; inspect JSON report");
            });
        }, "nyamist-r1.5-validation").start();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        validatedCurrentSurface = false;
        statusView.setText("Surface destroyed. Reopen or rotate to run again.");
    }

    private void saveReport(String json) {
        try {
            File directory = getExternalFilesDir(null);
            if (directory == null) directory = getFilesDir();
            File output = new File(directory, "nyamist-g925-r1.5-surface-swapchain.json");
            try (FileOutputStream stream = new FileOutputStream(output, false)) {
                stream.write(json.getBytes(StandardCharsets.UTF_8));
            }
            String digest = sha256(json);
            android.util.Log.i("NYAMIST_R1_5", "Report: " + output.getAbsolutePath());
            android.util.Log.i("NYAMIST_R1_5", "SHA-256: " + digest);
        } catch (Exception error) {
            android.util.Log.e("NYAMIST_R1_5", "Could not save report", error);
        }
    }

    private void copyReport() {
        if (latestReport.isEmpty()) {
            statusView.setText("No report is available yet.");
            return;
        }
        ClipboardManager clipboard =
                (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        clipboard.setPrimaryClip(ClipData.newPlainText("NYAMIST R1.5 JSON", latestReport));
        statusView.setText("JSON report copied to clipboard.");
    }

    private static String sha256(String value) throws Exception {
        byte[] digest = MessageDigest.getInstance("SHA-256")
                .digest(value.getBytes(StandardCharsets.UTF_8));
        StringBuilder output = new StringBuilder();
        for (byte item : digest) output.append(String.format("%02x", item));
        return output.toString();
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}
