package com.trediresearch.devssdashboard

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbManager
import android.util.Log
import com.hoho.android.usbserial.util.SerialInputOutputManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlin.coroutines.coroutineContext
import kotlin.math.pow

/**
 * Legge le correzioni RTCM3 da un ricevitore GNSS ArduSimple (u-blox ZED-F9P)
 * collegato via USB, esposto sul device Android come porta seriale
 * (tipicamente /dev/ttyACM0), e le inoltra a un MavlinkRtcmForwarder che le
 * reimpacchetta in messaggi MAVLink GPS_RTCM_DATA verso il veicolo.
 *
 * Monitora l'attacco/distacco USB e ritenta automaticamente l'apertura della
 * porta con backoff esponenziale finché non riesce (utile se l'ArduSimple
 * viene collegato dopo l'avvio dell'app, o si scollega e riattacca in campo).
 */
class ArduSimpleRtcmReceiver(
    private val context: Context,
    private val forwarder: MavlinkRtcmForwarder,
    private val devicePath: String = "/dev/ttyACM0",
    private val baudRate: Int = 460800,       // baud rate output RTCM del ricevitore u-blox, verifica la tua config
    private val initialRetryDelayMs: Long = 2000L,
    private val maxRetryDelayMs: Long = 30_000L,
    private val backoffMultiplier: Double = 1.8
) {

    interface Listener {
        fun onConnected() {}
        fun onDisconnected() {}
        fun onError(error: String) {}
        fun onRetrying(attempt: Int, delayMs: Long) {}
    }

    companion object {
        private const val TAG = "ArduSimpleRtcm"
    }

    private var listener: Listener? = null
    private var serialPortConnection: SerialPortConnection? = null

    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private var connectJob: Job? = null
    private var shouldReconnect = false
    private var retryAttempt = 0
    private var connected = false

    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context, intent: Intent) {
            when (intent.action) {
                UsbManager.ACTION_USB_DEVICE_ATTACHED -> {
                    Log.d(TAG, "Dispositivo USB collegato")
                    if (shouldReconnect && !connected) {
                        retryAttempt = 0
                        connectJob?.cancel()
                        connectJob = scope.launch { connectWithRetry() }
                    }
                }
                UsbManager.ACTION_USB_DEVICE_DETACHED -> {
                    Log.d(TAG, "Dispositivo USB scollegato")
                    handleDisconnection()
                }
            }
        }
    }

    fun setListener(l: Listener) {
        listener = l
    }

    /** Avvia il monitoraggio USB e tenta la connessione, ritentando finché non riesce */
    fun start() {
        shouldReconnect = true
        retryAttempt = 0

        val filter = IntentFilter().apply {
            addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
        }
        context.registerReceiver(usbReceiver, filter)

        connectJob?.cancel()
        connectJob = scope.launch { connectWithRetry() }
    }

    private suspend fun connectWithRetry() {
        while (shouldReconnect && coroutineContext.isActive) {
            val success = tryOpenPort()
            if (success) {
                retryAttempt = 0
                return
            }
            if (!shouldReconnect) return

            retryAttempt++
            val delayMs = computeBackoffDelay(retryAttempt)
            withContext(Dispatchers.Main) { listener?.onRetrying(retryAttempt, delayMs) }
            delay(delayMs)
        }
    }

    private fun computeBackoffDelay(attempt: Int): Long {
        val raw = initialRetryDelayMs * backoffMultiplier.pow((attempt - 1).toDouble())
        return raw.toLong().coerceAtMost(maxRetryDelayMs)
    }



    private suspend fun tryOpenPort(): Boolean {
        return try {
            val connection = SerialPortConnection.newBuilder(devicePath, baudRate)
                .flags(8192) // stesso flag usato per l'H12; adatta se il tuo ArduSimple richiede altro
                .build()

            connection.setDelegate(object : SerialPortConnection.Delegate {
                override fun connect() {
                    connected = true
                    scope.launch(Dispatchers.Main) { listener?.onConnected() }
                }

                override fun received(param1ArrayOfbyte: ByteArray, param1Int: Int) {
                    val chunk = if (param1Int == param1ArrayOfbyte.size)
                        param1ArrayOfbyte
                    else
                        param1ArrayOfbyte.copyOf(param1Int)
                    forwarder.feed(chunk) // inoltra al parser RTCM3 -> MAVLink
                }
            })

            connection.openConnection()
            serialPortConnection = connection
            connected = true
            true
        } catch (e: Exception) {
            withContext(Dispatchers.Main) {
                listener?.onError("Impossibile aprire $devicePath: ${e.message}")
            }
            closeQuietly()
            false
        }
    }

    private fun handleDisconnection() {
        val wasConnected = connected
        connected = false
        closeQuietly()
        if (wasConnected) {
            scope.launch(Dispatchers.Main) { listener?.onDisconnected() }
        }
        if (shouldReconnect) {
            retryAttempt = 0
            connectJob?.cancel()
            connectJob = scope.launch { connectWithRetry() }
        }
    }

    private fun closeQuietly() {
        try {
            serialPortConnection?.closeConnection() // verifica il nome esatto nella tua classe SerialPortConnection
        } catch (_: Exception) {
        }
        serialPortConnection = null
    }

    /** Ferma il monitoraggio USB e ogni tentativo di riconnessione, chiude la porta */
    fun stop() {
        shouldReconnect = false
        connectJob?.cancel()
        try {
            context.unregisterReceiver(usbReceiver)
        } catch (_: IllegalArgumentException) {
        }
        connected = false
        closeQuietly()
    }

    fun release() {
        stop()
        scope.cancel()
    }
}