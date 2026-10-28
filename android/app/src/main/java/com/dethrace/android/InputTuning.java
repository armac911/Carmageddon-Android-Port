package com.dethrace.android;

/** Launcher ranges and safe defaults; settings apply on the next Play. */
public final class InputTuning {
    private InputTuning() {}
    public static final String[] KEYS = {"stick_deadzone", "stick_sensitivity", "touch_deadzone", "touch_sensitivity",
        "trigger_deadzone", "trigger_sensitivity", "action_deadzone", "dpad_sensitivity", "steer_rise", "steer_return", "pedal_rise"};
    public static final String[] LABELS = {"Stick steering deadzone", "Stick steering sensitivity", "Touch steering deadzone", "Touch steering sensitivity",
        "Trigger deadzone", "Pedal sensitivity", "Action stick activation", "Digital steering strength", "Steering build time", "Steering return time", "Pedal build time"};
    public static final int[] DEFAULTS = {24,85,8,85,10,100,50,85,250,180,120};
    public static final int[] MIN = {0,25,0,25,0,25,20,25,0,0,0};
    public static final int[] MAX = {50,150,50,150,50,150,90,100,1000,1000,500};
    public static int clamp(int index, int value) { return Math.max(MIN[index],Math.min(MAX[index],value)); }
    public static String unit(int index) { return index >= 8 ? " ms" : "%"; }
}
