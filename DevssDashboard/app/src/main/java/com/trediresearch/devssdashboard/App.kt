package com.trediresearch.devssdashboard

import android.app.Application
import android.content.Intent
import android.os.Build


class App : Application() {
    override fun onCreate() {
        super.onCreate()
        startBackgroundService();
        //startService(Intent(this, BackgroundService::class.java))
    }

    fun startBackgroundService() {
        val intent = Intent(this, BackgroundService::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(intent)
        } else {
            startService(intent)
        }
    }

    fun stopBackgroundService() {
        val intent = Intent(this, BackgroundService::class.java).apply {
            action = BackgroundService.ACTION_STOP
        }
        startService(intent)
    }
}