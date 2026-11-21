package com.dethrace.android;

import android.content.Context;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;

/** Phone vibration for crashes, scaled by how hard the hit was. */
final class ImpactVibrator {
    private final Vibrator vibrator;
    private final float scale;

    /** Setting: "off", "normal" or "strong". */
    static float scaleFor(String setting) {
        if ("off".equals(setting)) return 0f;
        if ("strong".equals(setting)) return 1f;
        return 0.65f;
    }

    @SuppressWarnings("deprecation")
    ImpactVibrator(Context context, String setting) {
        scale = scaleFor(setting);
        Vibrator found = null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            VibratorManager manager = (VibratorManager) context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
            if (manager != null) found = manager.getDefaultVibrator();
        } else {
            found = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
        }
        vibrator = found != null && found.hasVibrator() ? found : null;
    }

    /** Duration in ms for a strength of 0..1000. */
    static int durationFor(int strength) { return 25 + Math.max(0, Math.min(1000, strength)) * 90 / 1000; }

    /** Amplitude 1..255 for a strength of 0..1000. */
    static int amplitudeFor(int strength, float scale) {
        float level = 0.25f + 0.75f * Math.max(0, Math.min(1000, strength)) / 1000f;
        return Math.max(1, Math.min(255, Math.round(255f * scale * level)));
    }

    @SuppressWarnings("deprecation")
    void impact(int strength) {
        if (vibrator == null || scale <= 0f || strength <= 0) return;
        int duration = durationFor(strength);
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                int amplitude = vibrator.hasAmplitudeControl()
                    ? amplitudeFor(strength, scale) : VibrationEffect.DEFAULT_AMPLITUDE;
                vibrator.vibrate(VibrationEffect.createOneShot(duration, amplitude));
            } else {
                // No amplitude control before API 26: shorten instead.
                vibrator.vibrate(Math.max(15, Math.round(duration * scale)));
            }
        } catch (RuntimeException ignored) {
            // Some devices throw when vibration is disabled system-wide.
        }
    }

    void cancel() {
        if (vibrator != null) try { vibrator.cancel(); } catch (RuntimeException ignored) { }
    }
}
