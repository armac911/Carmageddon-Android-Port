package com.dethrace.android;

import android.app.Activity;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Build;
import android.view.Surface;

/** Steer by turning the phone like a steering wheel (rotation within the
 *  screen plane). Works whether the phone is held upright or reclined, and is
 *  re-centred on the current hold whenever a race starts or the game resumes. */
final class TiltSteering implements SensorEventListener {
    interface Output { void tilt(int axis, boolean active); }

    /** Full lock angle in degrees for a launcher setting, or 0 when off. */
    static float fullLockDegrees(String setting) {
        if ("low".equals(setting)) return 35f;
        if ("medium".equals(setting)) return 25f;
        if ("high".equals(setting)) return 16f;
        return 0f;
    }

    private static final float DEAD_ZONE_DEGREES = 1.5f;
    /** Below this in-plane gravity (m/s^2) the phone is too flat to read. */
    private static final float MIN_IN_PLANE_GRAVITY = 2.5f;

    private final Activity activity;
    private final Output output;
    private final SensorManager sensors;
    private final Sensor sensor;
    private final boolean filtered;
    private final float fullLock;
    private final float[] gravity = new float[3];
    private boolean haveGravity, running;
    private float centre = Float.NaN;
    private int lastAxis;
    /** Display rotation, read once per start(): the game activity is locked
     *  to landscape, so it cannot change while the sensor runs. */
    private int displayRotation;

    TiltSteering(Activity activity, float fullLockDegrees, Output output) {
        this.activity = activity;
        this.output = output;
        this.fullLock = fullLockDegrees;
        sensors = (SensorManager) activity.getSystemService(Activity.SENSOR_SERVICE);
        Sensor gravitySensor = sensors == null ? null : sensors.getDefaultSensor(Sensor.TYPE_GRAVITY);
        // TYPE_GRAVITY is already fused and smoothed; raw accelerometer needs a filter.
        sensor = gravitySensor != null ? gravitySensor
            : sensors == null ? null : sensors.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        filtered = gravitySensor == null;
    }

    boolean available() { return sensor != null; }

    void start() {
        if (running || sensor == null) return;
        running = true;
        haveGravity = false;
        displayRotation = rotation();
        recenter();
        sensors.registerListener(this, sensor, SensorManager.SENSOR_DELAY_GAME);
    }

    void stop() {
        if (!running) return;
        running = false;
        sensors.unregisterListener(this);
        output.tilt(0, false);
    }

    /** The next reading becomes "straight ahead". */
    void recenter() { centre = Float.NaN; }

    @Override public void onSensorChanged(SensorEvent event) {
        if (!running) return;
        if (filtered && haveGravity) {
            for (int i = 0; i < 3; i++) gravity[i] += 0.2f * (event.values[i] - gravity[i]);
        } else {
            System.arraycopy(event.values, 0, gravity, 0, 3);
        }
        haveGravity = true;

        // Device axes -> screen axes for the current display rotation.
        float sx, sy;
        switch (displayRotation) {
            case Surface.ROTATION_90:  sx = -gravity[1]; sy = gravity[0]; break;
            case Surface.ROTATION_180: sx = -gravity[0]; sy = -gravity[1]; break;
            case Surface.ROTATION_270: sx = gravity[1];  sy = -gravity[0]; break;
            default:                   sx = gravity[0];  sy = gravity[1]; break;
        }
        if ((float) Math.hypot(sx, sy) < MIN_IN_PLANE_GRAVITY) {
            output.tilt(lastAxis, true); // too flat to tell: hold the last value
            return;
        }
        // Clockwise rotation (turning right) gives a positive angle.
        float angle = (float) Math.toDegrees(Math.atan2(-sx, sy));
        if (Float.isNaN(centre)) centre = angle;
        float delta = angle - centre;
        while (delta > 180f) delta -= 360f;
        while (delta < -180f) delta += 360f;
        output.tilt(lastAxis = axisFor(delta, fullLock), true);
    }

    /** Degrees from centre -> -65535..65535 with a small dead zone. */
    static int axisFor(float delta, float fullLock) {
        float magnitude = Math.abs(delta);
        if (magnitude <= DEAD_ZONE_DEGREES || fullLock <= DEAD_ZONE_DEGREES) return 0;
        float value = Math.min(1f, (magnitude - DEAD_ZONE_DEGREES) / (fullLock - DEAD_ZONE_DEGREES));
        return Math.round(Math.signum(delta) * value * 65535f);
    }

    @SuppressWarnings("deprecation")
    private int rotation() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && activity.getDisplay() != null)
            return activity.getDisplay().getRotation();
        return activity.getWindowManager().getDefaultDisplay().getRotation();
    }

    @Override public void onAccuracyChanged(Sensor sensor, int accuracy) { }
}
