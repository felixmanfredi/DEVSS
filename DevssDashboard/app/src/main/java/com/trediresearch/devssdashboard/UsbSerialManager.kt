package com.trediresearch.devssdashboard

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.os.Build
import androidx.core.content.ContextCompat
import kotlin.getValue
import com.hoho.android.usbserial.driver.UsbSerialDriver
import com.hoho.android.usbserial.driver.UsbSerialPort
import com.hoho.android.usbserial.driver.UsbSerialProber
import com.hoho.android.usbserial.util.SerialInputOutputManager

class UsbSerialManager(private val context: Context) {

    companion object {
        private const val ACTION_USB_PERMISSION = "com.trediresearch.devssdashboard.USB_PERMISSION"
        private const val TAG = "UsbSerialManager"
    }

    private val usbManager by lazy {
        context.getSystemService(Context.USB_SERVICE) as UsbManager
    }

    interface Listener {
        fun onPermissionGranted(port: UsbSerialPort)
        fun onPermissionDenied()
        fun onDeviceNotFound()
        fun onError(message: String)
    }

    private var listener: Listener? = null
    private var ioManager: SerialInputOutputManager? = null
    private val usbPermissionReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context, intent: Intent) {
            if (intent.action != ACTION_USB_PERMISSION) return

            val device: UsbDevice? = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
            val granted = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)

            if (granted && device != null) {
                openPort(device)
            } else {
                listener?.onPermissionDenied()
            }
        }
    }

    fun setListener(l: Listener) {
        listener = l
    }



    /** Cerca il primo dispositivo USB-seriale collegato e chiede il permesso se necessario */
    fun requestConnection() {
        val availableDrivers = UsbSerialProber.getDefaultProber().findAllDrivers(usbManager)
        if (availableDrivers.isEmpty()) {
            listener?.onDeviceNotFound()
            return
        }

        val driver = availableDrivers[0]
        val device = driver.device

        if (usbManager.hasPermission(device)) {
            openPort(device)
        } else {
            val permissionIntent = PendingIntent.getBroadcast(
                context, 0, Intent(ACTION_USB_PERMISSION),
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
                    PendingIntent.FLAG_MUTABLE
                else
                    0
            )
            val filter = IntentFilter(ACTION_USB_PERMISSION)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                context.registerReceiver(usbPermissionReceiver, filter, Context.RECEIVER_NOT_EXPORTED)
            } else {
                ContextCompat.registerReceiver(
                    context,
                    usbPermissionReceiver,
                    filter,
                    ContextCompat.RECEIVER_NOT_EXPORTED
                )
            }
            usbManager.requestPermission(device, permissionIntent)
        }
    }

    private fun openPort(device: UsbDevice) {
        try {
            val driver = UsbSerialProber.getDefaultProber().probeDevice(device)
                ?: run { listener?.onError("Nessun driver compatibile per il dispositivo"); return }

            val connection = usbManager.openDevice(device)
                ?: run { listener?.onError("Impossibile aprire il device USB (permesso mancante?)"); return }

            val port = driver.ports[0]
            port.open(connection)
            port.setParameters(
                460800, // baud rate: verifica quello configurato sul tuo ArduSimple
                UsbSerialPort.DATABITS_8,
                UsbSerialPort.STOPBITS_1,
                UsbSerialPort.PARITY_NONE
            )
            listener?.onPermissionGranted(port)
        } catch (e: Exception) {
            listener?.onError("Errore apertura porta: ${e.message}")
        }
    }

    fun release() {
        try {
            context.unregisterReceiver(usbPermissionReceiver)
        } catch (_: IllegalArgumentException) {
        }
    }
}