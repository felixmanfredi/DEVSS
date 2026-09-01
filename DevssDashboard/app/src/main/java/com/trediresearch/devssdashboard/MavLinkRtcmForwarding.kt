package com.trediresearch.devssdashboard
import android.util.Log
import java.io.OutputStream

/**
 * Impacchetta correzioni RTCM3 in messaggi MAVLink v2 GPS_RTCM_DATA (msg id 233)
 * e le invia alla Pixhawk tramite una porta seriale, esposta come OutputStream
 * e istanziata altrove nell'app (es. UsbSerialPort, BluetoothSocket, ecc.).
 */
class MavlinkRtcmForwarder(
    private val serialOutput: OutputStream,
    private val systemId: Int = 255,   // sysid del "GCS/companion" che invia i dati
    private val componentId: Int = 0   // 0 va bene per un semplice forwarder
) {
    companion object {
        private const val MAVLINK_STX = 0xFD
        private const val MSG_ID_GPS_RTCM_DATA = 233
        private const val CRC_EXTRA_GPS_RTCM_DATA = 35
        private const val MAX_RTCM_FRAGMENT = 180
        private const val MAX_FRAGMENTS = 4
        private const val RTCM3_PREAMBLE = 0xD3
        private const val CRC24Q_POLY = 0x1864CFB
    }

    private var mavlinkSeq = 0        // sequenza pacchetto MAVLink (0..255)
    private var rtcmFragmentSeq = 0   // sequenza di frammentazione RTCM (0..31)
    private val rtcmBuffer = ArrayDeque<Byte>() // buffer di riassemblaggio dello stream NTRIP

    /**
     * Da chiamare ogni volta che arrivano byte grezzi dallo stream NTRIP
     * (possono contenere frame RTCM3 parziali o più frame concatenati).
     */
    @Synchronized
    fun feed(rawData: ByteArray) {
        rtcmBuffer.addAll(rawData.toList())
        while (true) {
            val frame = extractNextFrame() ?: break
            sendRtcmFrame(frame)
        }
    }

    /** Estrae dal buffer il prossimo frame RTCM3 completo e con CRC valido, o null se non ancora disponibile */
    private fun extractNextFrame(): ByteArray? {
        while (rtcmBuffer.isNotEmpty()) {
            if ((rtcmBuffer.first().toInt() and 0xFF) != RTCM3_PREAMBLE) {
                rtcmBuffer.removeFirst()
                continue
            }
            if (rtcmBuffer.size < 3) return null

            val b1 = rtcmBuffer.elementAt(1).toInt() and 0xFF
            val b2 = rtcmBuffer.elementAt(2).toInt() and 0xFF

            if (b1 and 0xFC != 0) {
                // bit riservati non nulli -> preambolo falso positivo
                rtcmBuffer.removeFirst()
                continue
            }

            val payloadLen = ((b1 and 0x03) shl 8) or b2
            val frameLen = 3 + payloadLen + 3 // header(3) + payload + crc24(3)

            if (rtcmBuffer.size < frameLen) return null // frame ancora incompleto

            val frame = ByteArray(frameLen) { i -> rtcmBuffer.elementAt(i) }

            val crcCalc = crc24q(frame, frameLen - 3)
            val crcRecv = ((frame[frameLen - 3].toInt() and 0xFF) shl 16) or
                    ((frame[frameLen - 2].toInt() and 0xFF) shl 8) or
                    (frame[frameLen - 1].toInt() and 0xFF)

            repeat(frameLen) { rtcmBuffer.removeFirst() }

            if (crcCalc != crcRecv) continue // frame corrotto, scarta e riprova

            return frame
        }
        return null
    }

    /** CRC24Q (Qualcomm), usato dai frame RTCM3. Polinomio 0x1864CFB, init 0 */
    private fun crc24q(data: ByteArray, length: Int): Int {
        var crc = 0
        for (i in 0 until length) {
            crc = crc xor ((data[i].toInt() and 0xFF) shl 16)
            repeat(8) {
                crc = crc shl 1
                if (crc and 0x1000000 != 0) crc = crc xor CRC24Q_POLY
            }
        }
        return crc and 0xFFFFFF
    }

    /** Impacchetta un frame RTCM3 in uno o più GPS_RTCM_DATA (frammentazione se > 180 byte) */
    private fun sendRtcmFrame(frame: ByteArray) {
        if (frame.size <= MAX_RTCM_FRAGMENT) {
            sendGpsRtcmData(flags = 0, data = frame)
            return
        }

        val totalFragments = (frame.size + MAX_RTCM_FRAGMENT - 1) / MAX_RTCM_FRAGMENT
        if (totalFragments > MAX_FRAGMENTS) {
            // Oltre 4 x 180 = 720 byte il protocollo GPS_RTCM_DATA non può rappresentare
            // il messaggio: viene scartato (capita raramente con messaggi RTCM standard).
            return
        }

        val seq = rtcmFragmentSeq
        rtcmFragmentSeq = (rtcmFragmentSeq + 1) and 0x1F // 5 bit: 0..31

        for (fragmentId in 0 until totalFragments) {
            val start = fragmentId * MAX_RTCM_FRAGMENT
            val end = minOf(start + MAX_RTCM_FRAGMENT, frame.size)
            val chunk = frame.copyOfRange(start, end)

            // bit0 = fragmented, bit1-2 = fragment id, bit3-7 = sequence id
            val flags = 0x01 or (fragmentId shl 1) or (seq shl 3)
            sendGpsRtcmData(flags = flags, data = chunk)
        }
    }

    /** Costruisce e invia un singolo pacchetto MAVLink v2 GPS_RTCM_DATA sulla porta seriale */
    @Synchronized
    private fun sendGpsRtcmData(flags: Int, data: ByteArray) {
        val payload = ByteArray(2 + data.size)
        payload[0] = flags.toByte()
        payload[1] = data.size.toByte()
        System.arraycopy(data, 0, payload, 2, data.size)

        val packet = buildMavlinkV2Packet(MSG_ID_GPS_RTCM_DATA, CRC_EXTRA_GPS_RTCM_DATA, payload)

        try {
            serialOutput.write(packet)
            serialOutput.flush()
        } catch (e: Exception) {
            Log.e("MavlinkForward",e.message.toString())
            // gestire/loggare l'errore di scrittura sulla seriale secondo necessità
        }
    }

    /** Costruisce un frame MAVLink v2 completo (header + payload + checksum X.25), senza firma */
    private fun buildMavlinkV2Packet(msgId: Int, crcExtra: Int, payload: ByteArray): ByteArray {
        val header = ByteArray(10)
        header[0] = MAVLINK_STX.toByte()
        header[1] = payload.size.toByte()
        header[2] = 0 // incompat_flags
        header[3] = 0 // compat_flags
        header[4] = mavlinkSeq.toByte()
        header[5] = systemId.toByte()
        header[6] = componentId.toByte()
        header[7] = (msgId and 0xFF).toByte()
        header[8] = ((msgId shr 8) and 0xFF).toByte()
        header[9] = ((msgId shr 16) and 0xFF).toByte()

        mavlinkSeq = (mavlinkSeq + 1) and 0xFF

        var crc = 0xFFFF
        for (i in 1 until header.size) crc = crcAccumulate(header[i], crc) // CRC parte da payload_len, esclude STX
        for (b in payload) crc = crcAccumulate(b, crc)
        crc = crcAccumulate(crcExtra.toByte(), crc)

        val out = ByteArray(header.size + payload.size + 2)
        System.arraycopy(header, 0, out, 0, header.size)
        System.arraycopy(payload, 0, out, header.size, payload.size)
        out[out.size - 2] = (crc and 0xFF).toByte()
        out[out.size - 1] = ((crc shr 8) and 0xFF).toByte()
        return out
    }

    /** CRC-16/MCRF4XX (X.25) usato da MAVLink */
    private fun crcAccumulate(data: Byte, crcIn: Int): Int {
        var tmp = (data.toInt() and 0xFF) xor (crcIn and 0xFF)
        tmp = (tmp xor (tmp shl 4)) and 0xFF
        val crc = ((crcIn shr 8) and 0xFF) xor (tmp shl 8) xor (tmp shl 3) xor (tmp shr 4)
        return crc and 0xFFFF
    }

    /** Da chiamare, ad es., dopo una riconnessione al caster NTRIP */
    @Synchronized
    fun reset() {
        rtcmBuffer.clear()
        rtcmFragmentSeq = 0
    }
}