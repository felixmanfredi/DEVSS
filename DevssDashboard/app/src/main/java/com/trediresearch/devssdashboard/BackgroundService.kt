package com.trediresearch.devssdashboard

import android.app.Service
import android.content.Intent
import android.os.IBinder
import androidx.annotation.Nullable

class BackgroundService: Service() {

    @Nullable
    override fun onBind(intent: Intent?): IBinder? {
        return null
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        // do your jobs here
        return super.onStartCommand(intent, flags, startId)
    }
}