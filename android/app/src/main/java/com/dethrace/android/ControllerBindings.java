package com.dethrace.android;

import android.content.SharedPreferences;

final class ControllerBindings {
    static final String[] ACTIONS = {"Accelerate", "Brake / reverse", "Handbrake", "Repair", "Recover / respawn", "Camera view", "Map", "Pause", "Gear up", "Gear down", "Accelerate · second input", "Menu confirm", "Menu back"};
    static final String[] DEFAULTS = {"111","109","9","10","3","8","4","6","106","107","2","0","1"};
    static final String[] LABELS = {"Unassigned", "A", "B", "X", "Y", "Select / Back", "Guide", "Start", "L3", "R3", "LB", "RB", "D-pad up", "D-pad down", "D-pad left", "D-pad right", "Left stick left", "Left stick right", "Left stick up", "Left stick down", "Right stick left", "Right stick right", "Right stick up", "Right stick down", "LT", "RT"};
    static final String[] VALUES = {"-1","0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","100","101","102","103","104","105","106","107","109","111"};
    static String serialize(SharedPreferences prefs) {
        StringBuilder result = new StringBuilder();
        for (int i=0; i<ACTIONS.length; i++) {
            String value=prefs.getString("bind_"+i, DEFAULTS[i]);
            boolean valid=false; for(String id:VALUES) if(id.equals(value)) valid=true;
            if(!valid) value=DEFAULTS[i];
            if(i>0) result.append(','); result.append(value);
        }
        return result.toString();
    }
    private ControllerBindings() {}
}
