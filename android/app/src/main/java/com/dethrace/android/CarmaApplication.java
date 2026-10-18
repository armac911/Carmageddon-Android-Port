package com.dethrace.android;

public final class CarmaApplication extends android.app.Application {
    @Override public void onCreate(){
        super.onCreate();ErrorLogs.install(this);ErrorLogs.recoverAsync(this);
    }
}
