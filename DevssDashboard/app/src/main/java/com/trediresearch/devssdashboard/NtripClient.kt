package com.trediresearch.devssdashboard

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import androidx.core.app.ActivityCompat
import kotlinx.coroutines.*
import java.io.BufferedReader
import java.io.IOException
import java.io.InputStreamReader
import java.io.OutputStream
import java.net.Socket
import java.net.SocketException
import java.util.*
import kotlin.math.abs
import android.util.Log
/**
 * Client NTRIP per Android.
 * Si connette a un caster NTRIP, invia periodicamente la posizione GPS
 * dello smartphone come sentenza NMEA GGA e riceve i dati di correzione RTCM.
 */
class NtripClient(
    private val context: Context,
    private val host: String,
    private val port: Int,
    private val mountPoint: String,
    private val username: String,
    private val password: String,
    private val ggaIntervalMs: Long = 5000L
) {

    interface NtripListener {
        fun onRtcmData(data: ByteArray)
        fun onConnected()
        fun onDisconnected()
        fun onError(error: String)
        fun onGgaSent(sentence: String)
    }

    private var listener: NtripListener? = null
    private var socket: Socket? = null
    private var outputStream: OutputStream? = null
    private var connected = false

    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private var readJob: Job? = null
    private var ggaJob: Job? = null

    private var lastLocation: Location? = null

    private val locationManager by lazy {
        context.getSystemService(Context.LOCATION_SERVICE) as LocationManager
    }

    private val locationListener = object : LocationListener {
        override fun onLocationChanged(location: Location) {
            lastLocation = location
        }
        @Deprecated("Deprecated in Java")
        override fun onStatusChanged(provider: String?, status: Int, extras: android.os.Bundle?) {}
        override fun onProviderEnabled(provider: String) {}
        override fun onProviderDisabled(provider: String) {}
    }

    fun setListener(l: NtripListener) {
        listener = l
    }

    @SuppressLint("MissingPermission")
    fun startLocationUpdates() {
        if (ActivityCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION)
            != PackageManager.PERMISSION_GRANTED
        ) {
            listener?.onError("Permesso ACCESS_FINE_LOCATION non concesso")
            return
        }
        try {
            locationManager.requestLocationUpdates(
                LocationManager.GPS_PROVIDER,
                1000L,
                0f,
                locationListener
            )
            lastLocation = locationManager.getLastKnownLocation(LocationManager.GPS_PROVIDER)
        } catch (e: SecurityException) {
            listener?.onError("Errore permessi GPS: ${e.message}")
        }
    }

    fun stopLocationUpdates() {
        try {
            locationManager.removeUpdates(locationListener)
        } catch (_: SecurityException) {
        }
    }

    /** Avvia la connessione al caster NTRIP */
    fun connect(ntripVersion: Int = 2) {
        scope.launch {
            try {
                socket = Socket(host, port).apply { soTimeout = 10000 }
                outputStream = socket?.getOutputStream()

                val credentials = Base64.getEncoder()
                    .encodeToString("$username:$password".toByteArray())

                val request = buildString {
                    if (ntripVersion == 2) {
                        append("GET /$mountPoint HTTP/1.1\r\n")
                        append("Host: $host:$port\r\n")
                        append("Ntrip-Version: Ntrip/2.0\r\n")
                    } else {
                        append("GET /$mountPoint HTTP/1.0\r\n")
                        append("Host: $host:$port\r\n")
                    }
                    append("User-Agent: NTRIP AndroidClient/1.0\r\n")
                    append("Authorization: Basic $credentials\r\n")
                    append("Accept: */*\r\n")
                    append("Connection: close\r\n")
                    append("\r\n")
                }

                outputStream?.write(request.toByteArray())
                outputStream?.flush()

                val input = socket?.getInputStream() ?: throw IOException("Stream nullo")
                val headerReader = BufferedReader(InputStreamReader(input))

                // Leggi TUTTI gli header di risposta, non solo la status line
                val responseLines = mutableListOf<String>()
                var line: String?
                do {
                    line = headerReader.readLine()
                    if (line != null) responseLines.add(line)
                } while (!line.isNullOrEmpty())

                val statusLine = responseLines.firstOrNull() ?: ""

                withContext(Dispatchers.Main) {
                    listener?.onError("Risposta caster:\n" + responseLines.joinToString("\n"))
                }

                if (!statusLine.contains("200") && !statusLine.contains("ICY 200 OK")) {
                    disconnect()
                    return@launch
                }

                connected = true
                withContext(Dispatchers.Main) { listener?.onConnected() }

                startGgaLoop()
                startReadLoop(input)

            } catch (e: IOException) {
                Log.e("NTRIP",e.message.toString())
                disconnect()
            }catch (e: SocketException){
                withContext(Dispatchers.Main) {
                    listener?.onDisconnected()
                }
            }
        }
    }

    private fun startReadLoop(input: java.io.InputStream) {
        readJob = scope.launch {
            val buffer = ByteArray(4096)
            try {
                while (isActive && connected) {
                    val bytesRead = input.read(buffer)
                    if (bytesRead == -1) break
                    val data = buffer.copyOf(bytesRead)
                    withContext(Dispatchers.Main) {
                        listener?.onRtcmData(data)
                    }
                }
            } catch (e: IOException) {
                withContext(Dispatchers.Main) {
                    listener?.onError("Errore lettura RTCM: ${e.message}")
                }
            } finally {
                disconnect()
            }
        }
    }

    private fun startGgaLoop() {
        ggaJob = scope.launch {
            while (isActive && connected) {
                val loc = lastLocation
                if (loc != null) {
                    val gga = buildGgaSentence(loc)
                    try {
                        outputStream?.write((gga + "\r\n").toByteArray())
                        outputStream?.flush()
                        withContext(Dispatchers.Main) {
                            listener?.onGgaSent(gga)
                        }
                    } catch (e: IOException) {
                        withContext(Dispatchers.Main) {
                            listener?.onError("Errore invio GGA: ${e.message}")
                        }
                        disconnect()
                        break
                    }
                }
                delay(ggaIntervalMs)
            }
        }
    }

    /** Costruisce una sentenza NMEA GGA a partire da un oggetto Location */
    private fun buildGgaSentence(location: Location): String {
        val time = String.format(
            Locale.US, "%02d%02d%05.2f",
            Calendar.getInstance(TimeZone.getTimeZone("UTC")).get(Calendar.HOUR_OF_DAY),
            Calendar.getInstance(TimeZone.getTimeZone("UTC")).get(Calendar.MINUTE),
            Calendar.getInstance(TimeZone.getTimeZone("UTC")).get(Calendar.SECOND).toDouble()
        )

        val lat = location.latitude
        val lon = location.longitude

        val latDeg = abs(lat).toInt()
        val latMin = (abs(lat) - latDeg) * 60
        val latHemisphere = if (lat >= 0) "N" else "S"
        val latStr = String.format(Locale.US, "%02d%07.4f", latDeg, latMin)

        val lonDeg = abs(lon).toInt()
        val lonMin = (abs(lon) - lonDeg) * 60
        val lonHemisphere = if (lon >= 0) "E" else "W"
        val lonStr = String.format(Locale.US, "%03d%07.4f", lonDeg, lonMin)

        val fixQuality = 1 // GPS fix
        val numSatellites = "08" // valore indicativo, non sempre disponibile da Location
        val hdop = "1.0"
        val altitude = if (location.hasAltitude()) location.altitude else 0.0
        val geoidHeight = "0.0"

        val body = "GPGGA,$time,$latStr,$latHemisphere,$lonStr,$lonHemisphere," +
                "$fixQuality,$numSatellites,$hdop,${"%.1f".format(altitude)},M,$geoidHeight,M,,"

        val checksum = calculateNmeaChecksum(body)
        return "$$body*$checksum"
    }

    private fun calculateNmeaChecksum(sentence: String): String {
        var checksum = 0
        for (c in sentence) {
            checksum = checksum xor c.code
        }
        return String.format("%02X", checksum)
    }

    /** Chiude la connessione e libera le risorse */
    fun disconnect() {
        connected = false
        readJob?.cancel()
        ggaJob?.cancel()
        try {
            outputStream?.close()
            socket?.close()
        } catch (_: IOException) {
        }
        scope.launch(Dispatchers.Main) {
            listener?.onDisconnected()
        }
    }

    fun release() {
        disconnect()
        stopLocationUpdates()
        scope.cancel()
    }
}