package com.trediresearch.devssdashboard

import android.annotation.SuppressLint
import android.app.*
import android.content.Intent
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat

class BackgroundService : Service() {

    companion object {
        const val CHANNEL_ID = "background_service_channel"
        const val NOTIFICATION_ID = 1
        const val ACTION_STOP = "ACTION_STOP_SERVICE"
    }

    private var isRunning = false

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    @SuppressLint("ForegroundServiceType")
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopSelf()
            return START_NOT_STICKY
        }

        val notification = buildNotification()
        startForeground(NOTIFICATION_ID, notification)

        if (!isRunning) {
            isRunning = true
            startWork()
        }

        // Se il sistema uccide il processo, prova a ricrearlo
        return START_STICKY
    }

    private fun startWork() {
        // Qui va la logica che vuoi eseguire in background
        // Esempio: un loop, un listener, una sincronizzazione periodica
    }

    private fun buildNotification(): Notification {
        val stopIntent = Intent(this, BackgroundService::class.java).apply {
            action = ACTION_STOP
        }
        val stopPendingIntent = PendingIntent.getService(
            this, 0, stopIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("App attiva")
            .setContentText("Il servizio è in esecuzione in background")
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setOngoing(true)
            .addAction(android.R.drawable.ic_media_pause, "Ferma", stopPendingIntent)
            .build()
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "Servizio in background",
                NotificationManager.IMPORTANCE_LOW
            )
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(channel)
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        isRunning = false
        super.onDestroy()
    }
}