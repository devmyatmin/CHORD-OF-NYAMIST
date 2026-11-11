package dev.nyamist.dx11live;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Color;
import android.opengl.GLSurfaceView;
import android.opengl.GLES30;
import android.opengl.Matrix;
import android.os.BatteryManager;
import android.os.Bundle;
import android.os.Debug;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;
import java.util.Arrays;
import java.util.Locale;

/**
 * Native Android rendering and telemetry companion.
 * This APK renders a real GLES 3.0 cube. The separate Windows workload in
 * windows-dx11-cube is the true Direct3D 11 / DXVK test path.
 */
public final class MainActivity extends Activity {
    private final Handler handler = new Handler(Looper.getMainLooper());
    private CubeView cubeView;
    private TextView hud;
    private long lastCpuMs;
    private long lastCpuWallMs;
    private double cpuPercent;
    private float batteryTemp = Float.NaN;

    private final Runnable updateHud = new Runnable() {
        @Override public void run() {
            if (cubeView == null || hud == null) return;
            Intent battery = registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
            if (battery != null) {
                int raw = battery.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, -1);
                batteryTemp = raw >= 0 ? raw / 10.0f : Float.NaN;
            }
            long nowWall = android.os.SystemClock.elapsedRealtime();
            long nowCpu = Process.getElapsedCpuTime();
            if (lastCpuWallMs > 0 && nowWall > lastCpuWallMs) {
                long cpuDelta = Math.max(0, nowCpu - lastCpuMs);
                cpuPercent = Math.min(100.0, 100.0 * cpuDelta /
                        ((nowWall - lastCpuWallMs) * Math.max(1, Runtime.getRuntime().availableProcessors())));
            }
            lastCpuMs = nowCpu;
            lastCpuWallMs = nowWall;

            Debug.MemoryInfo memoryInfo = new Debug.MemoryInfo();
            Debug.getMemoryInfo(memoryInfo);
            double ramMb = memoryInfo.getTotalPss() / 1024.0;
            ActivityManager activityManager = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
            ActivityManager.MemoryInfo deviceMemory = new ActivityManager.MemoryInfo();
            activityManager.getMemoryInfo(deviceMemory);
            double totalRamMb = deviceMemory.totalMem / (1024.0 * 1024.0);
            double freeRamMb = deviceMemory.availMem / (1024.0 * 1024.0);
            CubeRenderer.Metrics m = cubeView.renderer.metrics();
            String temp = Float.isNaN(batteryTemp) ? "N/A" : String.format(Locale.US, "%.1f C", batteryTemp);
            hud.setText(String.format(Locale.US,
                    "NYAMIST  |  LIVE 3D CUBE + DEVICE TELEMETRY\n" +
                    "Native Android reference renderer: OpenGL ES 3.0\n" +
                    "DX11 MAIN TEST: run windows-dx11-cube\\NYAMIST-DX11-Live-Cube.exe in PanPlay/DXVK\n\n" +
                    "FPS  %.1f current   %.1f avg   %.1f 1%% low   %.1f 0.1%% low\n" +
                    "Frame  %.2f ms current   %.2f avg   P50 %.2f   P95 %.2f   P99 %.2f\n" +
                    "GPU  %s\n" +
                    "GPU load  N/A (not exposed by public Android API)\n" +
                    "CPU  %.1f%% of device   App PSS %.0f MB\n" +
                    "Device RAM  %.0f MB free / %.0f MB total\n" +
                    "Resolution  %d x %d\n" +
                    "Battery temperature  %s\n" +
                    "Frames %d   Stutter counter %d   Session %.0f s",
                    m.currentFps, m.averageFps, m.low1, m.low01,
                    m.currentFrameMs, m.averageFrameMs, m.p50, m.p95, m.p99,
                    m.gpuRenderer, cpuPercent, ramMb, freeRamMb, totalRamMb,
                    m.width, m.height, temp, m.frames, m.stutters, m.durationSeconds));
            handler.postDelayed(this, 500);
        }
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        FrameLayout root = new FrameLayout(this);
        cubeView = new CubeView(this);
        root.addView(cubeView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        hud = new TextView(this);
        hud.setTextColor(Color.WHITE);
        hud.setTextSize(12);
        hud.setTypeface(android.graphics.Typeface.MONOSPACE);
        hud.setBackgroundColor(0xB8000000);
        hud.setPadding(18, 14, 18, 14);
        FrameLayout.LayoutParams hp = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.TOP | Gravity.START);
        hp.leftMargin = 12;
        hp.topMargin = 12;
        root.addView(hud, hp);
        setContentView(root);
        handler.post(updateHud);
    }

    @Override protected void onResume() {
        super.onResume();
        if (cubeView != null) cubeView.onResume();
        handler.removeCallbacks(updateHud);
        handler.post(updateHud);
    }

    @Override protected void onPause() {
        handler.removeCallbacks(updateHud);
        if (cubeView != null) cubeView.onPause();
        super.onPause();
    }

    private static final class CubeView extends GLSurfaceView {
        final CubeRenderer renderer;
        CubeView(Context context) {
            super(context);
            setEGLContextClientVersion(3);
            setEGLConfigChooser(8, 8, 8, 8, 24, 8);
            renderer = new CubeRenderer();
            setRenderer(renderer);
            setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        }
    }

    private static final class CubeRenderer implements GLSurfaceView.Renderer {
        private static final String VERTEX_SHADER =
                "#version 300 es\n" +
                "layout(location=0) in vec3 aPosition;\n" +
                "layout(location=1) in vec3 aColor;\n" +
                "uniform mat4 uMvp;\n" +
                "out vec3 vColor;\n" +
                "void main(){ gl_Position=uMvp*vec4(aPosition,1.0); vColor=aColor; }";
        private static final String FRAGMENT_SHADER =
                "#version 300 es\n" +
                "precision mediump float;\n" +
                "in vec3 vColor; out vec4 outColor;\n" +
                "void main(){ outColor=vec4(vColor,1.0); }";

        private static final float[] VERTICES = {
                -1,-1,-1, 1,0,0,  1,-1,-1, 1,0,0,  1,1,-1, 1,0,0, -1,1,-1, 1,0,0,
                -1,-1, 1, 0,1,0,  1,-1, 1, 0,1,0,  1,1, 1, 0,1,0, -1,1, 1, 0,1,0
        };
        private static final short[] INDICES = {
                0,2,1, 0,3,2, 4,5,6, 4,6,7, 0,1,5, 0,5,4,
                3,7,6, 3,6,2, 1,2,6, 1,6,5, 0,4,7, 0,7,3
        };

        private final float[] projection = new float[16];
        private final float[] view = new float[16];
        private final float[] model = new float[16];
        private final float[] vp = new float[16];
        private final float[] mvp = new float[16];
        private final long[] frameTimesNs = new long[600];
        private int frameCursor;
        private int frameSamples;
        private long startNs;
        private long previousFrameNs;
        private long lastMetricsNs;
        private long frames;
        private long stutters;
        private double frameTimeSumMs;
        private int width = 1;
        private int height = 1;
        private int program;
        private int vao;
        private int vbo;
        private int ebo;
        private int mvpLocation;
        private String gpuRenderer = "Unknown";
        private volatile Metrics latest = new Metrics();

        static final class Metrics {
            double currentFps, averageFps, low1, low01;
            double currentFrameMs, averageFrameMs, p50, p95, p99;
            double durationSeconds;
            long frames, stutters;
            String gpuRenderer = "Unknown";
            int width = 0, height = 0;
        }

        Metrics metrics() { return latest; }

        @Override public void onSurfaceCreated(javax.microedition.khronos.opengles.GL10 gl,
                                                javax.microedition.khronos.egl.EGLConfig config) {
            GLES30.glEnable(GLES30.GL_DEPTH_TEST);
            GLES30.glEnable(GLES30.GL_CULL_FACE);
            GLES30.glCullFace(GLES30.GL_BACK);
            GLES30.glClearColor(0.025f, 0.035f, 0.055f, 1f);
            gpuRenderer = GLES30.glGetString(GLES30.GL_RENDERER) + " | " + GLES30.glGetString(GLES30.GL_VERSION);
            program = createProgram(VERTEX_SHADER, FRAGMENT_SHADER);
            mvpLocation = GLES30.glGetUniformLocation(program, "uMvp");

            FloatBuffer vertices = ByteBuffer.allocateDirect(VERTICES.length * 4)
                    .order(ByteOrder.nativeOrder()).asFloatBuffer();
            vertices.put(VERTICES).position(0);
            ShortBuffer indices = ByteBuffer.allocateDirect(INDICES.length * 2)
                    .order(ByteOrder.nativeOrder()).asShortBuffer();
            indices.put(INDICES).position(0);

            int[] ids = new int[1];
            GLES30.glGenVertexArrays(1, ids, 0); vao = ids[0];
            GLES30.glBindVertexArray(vao);
            GLES30.glGenBuffers(1, ids, 0); vbo = ids[0];
            GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, vbo);
            GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, VERTICES.length * 4, vertices, GLES30.GL_STATIC_DRAW);
            GLES30.glGenBuffers(1, ids, 0); ebo = ids[0];
            GLES30.glBindBuffer(GLES30.GL_ELEMENT_ARRAY_BUFFER, ebo);
            GLES30.glBufferData(GLES30.GL_ELEMENT_ARRAY_BUFFER, INDICES.length * 2, indices, GLES30.GL_STATIC_DRAW);
            GLES30.glEnableVertexAttribArray(0);
            GLES30.glVertexAttribPointer(0, 3, GLES30.GL_FLOAT, false, 24, 0);
            GLES30.glEnableVertexAttribArray(1);
            GLES30.glVertexAttribPointer(1, 3, GLES30.GL_FLOAT, false, 24, 12);
            GLES30.glBindVertexArray(0);
            startNs = System.nanoTime();
            previousFrameNs = startNs;
        }

        @Override public void onSurfaceChanged(javax.microedition.khronos.opengles.GL10 gl,
                                                int w, int h) {
            width = Math.max(1, w);
            height = Math.max(1, h);
            GLES30.glViewport(0, 0, width, height);
            Matrix.perspectiveM(projection, 0, 52f, (float) width / height, 0.1f, 50f);
            Matrix.setLookAtM(view, 0, 3.2f, 2.5f, 5.5f, 0f, 0f, 0f, 0f, 1f, 0f);
        }

        @Override public void onDrawFrame(javax.microedition.khronos.opengles.GL10 gl) {
            long now = System.nanoTime();
            double dtMs = Math.max(0.001, (now - previousFrameNs) / 1_000_000.0);
            previousFrameNs = now;
            if (frames > 10 && now - lastMetricsNs >= 250_000_000L && dtMs > 2.0 * rollingMedianMs()) stutters++;
            frameTimesNs[frameCursor] = (long) (dtMs * 1_000_000.0);
            frameCursor = (frameCursor + 1) % frameTimesNs.length;
            frameSamples = Math.min(frameSamples + 1, frameTimesNs.length);
            frames++;
            frameTimeSumMs += dtMs;

            float angle = (float) ((now - startNs) / 1_000_000_000.0 * 42.0);
            Matrix.setRotateM(model, 0, angle, 0.8f, 1.0f, 0.35f);
            Matrix.multiplyMM(vp, 0, projection, 0, view, 0);
            Matrix.multiplyMM(mvp, 0, vp, 0, model, 0);

            GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT | GLES30.GL_DEPTH_BUFFER_BIT);
            GLES30.glUseProgram(program);
            GLES30.glUniformMatrix4fv(mvpLocation, 1, false, mvp, 0);
            GLES30.glBindVertexArray(vao);
            GLES30.glDrawElements(GLES30.GL_TRIANGLES, INDICES.length, GLES30.GL_UNSIGNED_SHORT, 0);
            GLES30.glBindVertexArray(0);
            publishMetrics(dtMs, now);
        }

        private void publishMetrics(double dtMs, long now) {
            Metrics previous = latest;
            Metrics m = new Metrics();
            double duration = Math.max(0.001, (now - startNs) / 1_000_000_000.0);
            m.currentFps = 1000.0 / dtMs;
            m.averageFps = frames / duration;
            m.currentFrameMs = dtMs;
            m.averageFrameMs = frameTimeSumMs / Math.max(1, frames);
            m.durationSeconds = duration;
            m.frames = frames;
            m.stutters = stutters;
            m.gpuRenderer = gpuRenderer;
            m.width = width;
            m.height = height;
            if (now - lastMetricsNs >= 250_000_000L || previous.frames == 0) {
                long[] copy = Arrays.copyOf(frameTimesNs, frameSamples);
                Arrays.sort(copy);
                m.p50 = percentile(copy, 0.50) / 1_000_000.0;
                m.p95 = percentile(copy, 0.95) / 1_000_000.0;
                m.p99 = percentile(copy, 0.99) / 1_000_000.0;
                m.low1 = m.p99 > 0 ? 1000.0 / m.p99 : 0;
                double p999 = percentile(copy, 0.999) / 1_000_000.0;
                m.low01 = p999 > 0 ? 1000.0 / p999 : 0;
                lastMetricsNs = now;
            } else {
                m.p50 = previous.p50; m.p95 = previous.p95; m.p99 = previous.p99;
                m.low1 = previous.low1; m.low01 = previous.low01;
            }
            latest = m;
        }

        private double rollingMedianMs() {
            if (frameSamples == 0) return 16.7;
            long[] copy = Arrays.copyOf(frameTimesNs, frameSamples);
            Arrays.sort(copy);
            return percentile(copy, 0.50) / 1_000_000.0;
        }

        private static long percentile(long[] sorted, double p) {
            if (sorted.length == 0) return 0;
            int index = Math.max(0, Math.min(sorted.length - 1,
                    (int) Math.ceil(p * sorted.length) - 1));
            return sorted[index];
        }

        private static int compileShader(int type, String source) {
            int shader = GLES30.glCreateShader(type);
            GLES30.glShaderSource(shader, source);
            GLES30.glCompileShader(shader);
            int[] ok = new int[1];
            GLES30.glGetShaderiv(shader, GLES30.GL_COMPILE_STATUS, ok, 0);
            if (ok[0] == 0) {
                String log = GLES30.glGetShaderInfoLog(shader);
                GLES30.glDeleteShader(shader);
                throw new IllegalStateException("Shader compile failed: " + log);
            }
            return shader;
        }

        private static int createProgram(String vs, String fs) {
            int vertex = compileShader(GLES30.GL_VERTEX_SHADER, vs);
            int fragment = compileShader(GLES30.GL_FRAGMENT_SHADER, fs);
            int p = GLES30.glCreateProgram();
            GLES30.glAttachShader(p, vertex);
            GLES30.glAttachShader(p, fragment);
            GLES30.glLinkProgram(p);
            int[] ok = new int[1];
            GLES30.glGetProgramiv(p, GLES30.GL_LINK_STATUS, ok, 0);
            GLES30.glDeleteShader(vertex);
            GLES30.glDeleteShader(fragment);
            if (ok[0] == 0) {
                String log = GLES30.glGetProgramInfoLog(p);
                GLES30.glDeleteProgram(p);
                throw new IllegalStateException("Program link failed: " + log);
            }
            return p;
        }
    }
}
