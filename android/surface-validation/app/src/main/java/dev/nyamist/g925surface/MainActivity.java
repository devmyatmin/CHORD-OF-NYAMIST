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
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;

public final class MainActivity extends Activity implements SurfaceHolder.Callback {
    static {
        System.loadLibrary("nyamist_surface_validation");
    }

    private final Object lifecycleLock = new Object();
    private final ExecutorService validationExecutor =
            Executors.newSingleThreadExecutor(runnable -> {
                Thread thread = new Thread(runnable, "nyamist-r2.1-wsi-session");
                thread.setDaemon(false);
                return thread;
            });

    private TextView statusView;
    private TextView reportView;
    private SurfaceHolder currentHolder;
    private String latestReport = "";

    private boolean surfaceAvailable;
    private boolean surfaceWasDestroyed;
    private volatile boolean activityDestroyed;
    private boolean activityResumed;
    private int surfaceGeneration;
    private int currentSurfaceWidth;
    private int currentSurfaceHeight;
    private int scheduledGeneration = -1;
    private int scheduledWidth = -1;
    private int scheduledHeight = -1;
    private long requestSerial;
    private long latestRequestId;

    private int surfaceCreatedCount;
    private int surfaceRecreatedCount;
    private int surfaceChangedCount;
    private int surfaceDestroyedCount;
    private int surfaceResizeCount;
    private int activityResumeCount;
    private int activityPauseCount;
    private int manualRebuildRequestCount;
    private int nativeSessionStartedCount;
    private int nativeSessionCompletedCount;
    private int nativeSessionSuccessCount;
    private int nativeSessionSkippedCount;
    private int activeNativeSessions;
    private int maxConcurrentNativeSessions;
    private String lastSurfaceEvent = "waiting_for_surface";

    private static native String validateSurfaceNative(Surface surface, int width, int height);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(12);
        root.setPadding(pad, pad, pad, pad);

        TextView title = new TextView(this);
        title.setText("CHORD OF NYAMIST — G925 R2.1");
        title.setTextSize(20.0f);
        root.addView(title, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        statusView = new TextView(this);
        statusView.setText("Waiting for Android Surface lifecycle...");
        statusView.setTextSize(14.0f);
        root.addView(statusView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        SurfaceView surfaceView = new SurfaceView(this);
        root.addView(surfaceView, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dp(220)));
        surfaceView.getHolder().addCallback(this);

        Button rebuildButton = new Button(this);
        rebuildButton.setText("REBUILD VULKAN SURFACE / SWAPCHAIN");
        rebuildButton.setOnClickListener(view -> requestManualRebuild());
        root.addView(rebuildButton, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

        Button copyButton = new Button(this);
        copyButton.setText("COPY JSON REPORT");
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
    protected void onResume() {
        super.onResume();
        SurfaceHolder holderToRebuild = null;
        int width = 0;
        int height = 0;
        synchronized (lifecycleLock) {
            activityResumed = true;
            activityResumeCount++;
            if (surfaceAvailable && currentHolder != null && nativeSessionCompletedCount > 0) {
                holderToRebuild = currentHolder;
                width = currentSurfaceWidth;
                height = currentSurfaceHeight;
            }
        }
        if (holderToRebuild != null) {
            scheduleValidation(holderToRebuild, width, height, "activity_resumed", true);
        }
    }

    @Override
    protected void onPause() {
        synchronized (lifecycleLock) {
            activityResumed = false;
            activityPauseCount++;
            lastSurfaceEvent = "activity_paused";
        }
        super.onPause();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        synchronized (lifecycleLock) {
            currentHolder = holder;
            surfaceAvailable = true;
            surfaceGeneration++;
            surfaceCreatedCount++;
            if (surfaceWasDestroyed) surfaceRecreatedCount++;
            surfaceWasDestroyed = false;
            currentSurfaceWidth = 0;
            currentSurfaceHeight = 0;
            scheduledGeneration = -1;
            scheduledWidth = -1;
            scheduledHeight = -1;
            lastSurfaceEvent = "surface_created";
        }
        statusView.setText("Surface created. Waiting for valid dimensions...");
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (width <= 0 || height <= 0) return;

        String reason;
        synchronized (lifecycleLock) {
            if (!surfaceAvailable || holder != currentHolder) return;
            boolean resized = currentSurfaceWidth > 0 && currentSurfaceHeight > 0
                    && (currentSurfaceWidth != width || currentSurfaceHeight != height);
            if (resized) surfaceResizeCount++;
            currentSurfaceWidth = width;
            currentSurfaceHeight = height;
            surfaceChangedCount++;
            reason = resized ? "surface_resized" : "surface_ready";
            lastSurfaceEvent = reason;
        }
        scheduleValidation(holder, width, height, reason, false);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        synchronized (lifecycleLock) {
            surfaceAvailable = false;
            surfaceWasDestroyed = true;
            surfaceDestroyedCount++;
            latestRequestId = ++requestSerial;
            currentHolder = null;
            scheduledGeneration = -1;
            scheduledWidth = -1;
            scheduledHeight = -1;
            lastSurfaceEvent = "surface_destroyed";
        }
        statusView.setText(
                "Surface destroyed. Queued stale sessions will be skipped; active native work will clean up.");
    }

    private void requestManualRebuild() {
        SurfaceHolder holder;
        int width;
        int height;
        synchronized (lifecycleLock) {
            holder = currentHolder;
            width = currentSurfaceWidth;
            height = currentSurfaceHeight;
            if (surfaceAvailable && holder != null) manualRebuildRequestCount++;
        }
        if (holder == null || width <= 0 || height <= 0) {
            statusView.setText("No valid Surface is available for rebuilding.");
            return;
        }
        scheduleValidation(holder, width, height, "manual_rebuild", true);
    }

    private void scheduleValidation(
            SurfaceHolder holder, int width, int height, String reason, boolean force) {
        if (holder == null || width <= 0 || height <= 0) return;

        final Surface sessionSurface;
        try {
            Surface source = holder.getSurface();
            if (source == null || !source.isValid()) {
                recordSkippedSession();
                return;
            }
            sessionSurface = new Surface(source);
            if (!sessionSurface.isValid()) {
                sessionSurface.release();
                recordSkippedSession();
                return;
            }
        } catch (RuntimeException error) {
            recordSkippedSession();
            return;
        }

        final int sessionGeneration;
        final long requestId;
        synchronized (lifecycleLock) {
            if (activityDestroyed || !surfaceAvailable || currentHolder != holder) {
                nativeSessionSkippedCount++;
                sessionSurface.release();
                return;
            }
            sessionGeneration = surfaceGeneration;
            if (!force && scheduledGeneration == sessionGeneration
                    && scheduledWidth == width && scheduledHeight == height) {
                sessionSurface.release();
                return;
            }
            requestId = ++requestSerial;
            latestRequestId = requestId;
            scheduledGeneration = sessionGeneration;
            scheduledWidth = width;
            scheduledHeight = height;
            lastSurfaceEvent = reason;
        }

        statusView.setText("Building a fresh Vulkan Surface / Swapchain session: " + reason);
        try {
            validationExecutor.execute(() -> runValidationSession(
                    sessionSurface, width, height, sessionGeneration, requestId, reason));
        } catch (RejectedExecutionException error) {
            sessionSurface.release();
            recordSkippedSession();
        }
    }

    private void runValidationSession(
            Surface sessionSurface,
            int width,
            int height,
            int sessionGeneration,
            long requestId,
            String reason) {
        synchronized (lifecycleLock) {
            boolean stale = activityDestroyed || !surfaceAvailable
                    || sessionGeneration != surfaceGeneration
                    || requestId != latestRequestId
                    || !sessionSurface.isValid();
            if (stale) {
                nativeSessionSkippedCount++;
                sessionSurface.release();
                return;
            }
            nativeSessionStartedCount++;
            activeNativeSessions++;
            if (activeNativeSessions > maxConcurrentNativeSessions) {
                maxConcurrentNativeSessions = activeNativeSessions;
            }
        }

        String nativeJson;
        try {
            nativeJson = validateSurfaceNative(sessionSurface, width, height);
            if (nativeJson == null || nativeJson.trim().isEmpty()) {
                nativeJson = failureJson("Native validation returned an empty report");
            }
        } catch (RuntimeException error) {
            nativeJson = failureJson("Native validation raised " + error.getClass().getSimpleName());
        } finally {
            sessionSurface.release();
        }

        boolean sessionSuccess = nativeJson.contains("\"status\": \"success\"")
                || nativeJson.contains("\"status\":\"success\"");
        boolean sessionCurrent;
        synchronized (lifecycleLock) {
            nativeSessionCompletedCount++;
            if (sessionSuccess) nativeSessionSuccessCount++;
            activeNativeSessions--;
            sessionCurrent = surfaceAvailable && sessionGeneration == surfaceGeneration
                    && width == currentSurfaceWidth && height == currentSurfaceHeight;
        }

        String completeReport = appendLifecycleMetadata(
                nativeJson, sessionGeneration, requestId, width, height, reason,
                sessionSuccess, sessionCurrent);
        saveReport(completeReport);

        final String result = completeReport;
        final boolean currentAtCompletion = sessionCurrent;
        if (!activityDestroyed) {
            runOnUiThread(() -> {
                if (activityDestroyed) return;
                latestReport = result;
                reportView.setText(result);
                if (!currentAtCompletion) {
                    statusView.setText(
                            "A previous Surface session finished. The current Surface session is queued.");
                } else if (!sessionSuccess) {
                    statusView.setText("R2.1 WSI session failed; inspect the JSON report.");
                } else if (result.contains("\"pixel_verification_status\": \"success\"")) {
                    statusView.setText("R2.1 WSI session SUCCESS — pixel readback also passed.");
                } else {
                    statusView.setText("R2.1 WSI session completed; inspect the validation fields.");
                }
            });
        }
    }

    private void recordSkippedSession() {
        synchronized (lifecycleLock) {
            nativeSessionSkippedCount++;
        }
    }

    private String appendLifecycleMetadata(
            String nativeJson,
            int sessionGeneration,
            long requestId,
            int width,
            int height,
            String reason,
            boolean sessionSuccess,
            boolean sessionCurrent) {
        String clean = nativeJson == null ? "{}" : nativeJson.trim();
        int closingBrace = clean.lastIndexOf('}');
        if (!clean.startsWith("{") || closingBrace < 0) {
            clean = failureJson("Native validation returned invalid JSON").trim();
            closingBrace = clean.lastIndexOf('}');
        }

        synchronized (lifecycleLock) {
            String lifecycle = "\n  \"android_surface_lifecycle\": {"
                    + "\n    \"schema_version\": 1,"
                    + "\n    \"session_generation\": " + sessionGeneration + ","
                    + "\n    \"session_request_id\": " + requestId + ","
                    + "\n    \"session_reason\": \"" + reason + "\","
                    + "\n    \"session_width\": " + width + ","
                    + "\n    \"session_height\": " + height + ","
                    + "\n    \"session_success\": " + sessionSuccess + ","
                    + "\n    \"session_current_at_completion\": " + sessionCurrent + ","
                    + "\n    \"surface_generation_at_report\": " + surfaceGeneration + ","
                    + "\n    \"surface_lifecycle_status\": \""
                    + (surfaceAvailable ? "surface_available" : "surface_destroyed") + "\","
                    + "\n    \"surface_created_count\": " + surfaceCreatedCount + ","
                    + "\n    \"surface_recreated_count\": " + surfaceRecreatedCount + ","
                    + "\n    \"surface_changed_count\": " + surfaceChangedCount + ","
                    + "\n    \"surface_destroyed_count\": " + surfaceDestroyedCount + ","
                    + "\n    \"surface_resize_count\": " + surfaceResizeCount + ","
                    + "\n    \"activity_resume_count\": " + activityResumeCount + ","
                    + "\n    \"activity_pause_count\": " + activityPauseCount + ","
                    + "\n    \"activity_resumed_at_report\": " + activityResumed + ","
                    + "\n    \"manual_rebuild_request_count\": " + manualRebuildRequestCount + ","
                    + "\n    \"native_session_started_count\": " + nativeSessionStartedCount + ","
                    + "\n    \"native_session_completed_count\": " + nativeSessionCompletedCount + ","
                    + "\n    \"native_session_success_count\": " + nativeSessionSuccessCount + ","
                    + "\n    \"native_session_skipped_count\": " + nativeSessionSkippedCount + ","
                    + "\n    \"native_session_active_count\": " + activeNativeSessions + ","
                    + "\n    \"max_concurrent_native_sessions\": " + maxConcurrentNativeSessions + ","
                    + "\n    \"last_surface_event\": \"" + lastSurfaceEvent + "\","
                    + "\n    \"session_rebuild_policy\": \"fresh_wsi_session_per_request\""
                    + "\n  }";
            return clean.substring(0, closingBrace) + "," + lifecycle + "\n}\n";
        }
    }

    private String failureJson(String reason) {
        return "{\n  \"schema_version\": 6,"
                + "\n  \"project\": \"CHORD OF NYAMIST / NYAMIST G925 R2.1\","
                + "\n  \"status\": \"failed\","
                + "\n  \"failure_reason\": \"" + reason + "\""
                + "\n}";
    }

    private void saveReport(String json) {
        try {
            File directory = getExternalFilesDir(null);
            if (directory == null) directory = getFilesDir();
            File output = new File(directory, "nyamist-g925-r2.1-surface-lifecycle.json");
            try (FileOutputStream stream = new FileOutputStream(output, false)) {
                stream.write(json.getBytes(StandardCharsets.UTF_8));
            }
            String digest = sha256(json);
            android.util.Log.i("NYAMIST_R2_1", "Report: " + output.getAbsolutePath());
            android.util.Log.i("NYAMIST_R2_1", "SHA-256: " + digest);
        } catch (Exception error) {
            android.util.Log.e("NYAMIST_R2_1", "Could not save report", error);
        }
    }

    private void copyReport() {
        if (latestReport.isEmpty()) {
            statusView.setText("No report is available yet.");
            return;
        }
        ClipboardManager clipboard =
                (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        clipboard.setPrimaryClip(ClipData.newPlainText("NYAMIST R2.1 JSON", latestReport));
        statusView.setText("R2.1 JSON report copied to clipboard.");
    }

    @Override
    protected void onDestroy() {
        synchronized (lifecycleLock) {
            activityDestroyed = true;
            surfaceAvailable = false;
            latestRequestId = ++requestSerial;
            lastSurfaceEvent = "activity_destroyed";
        }
        validationExecutor.shutdown();
        super.onDestroy();
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
