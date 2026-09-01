package com.trediresearch.devssdashboard

import android.Manifest
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.hardware.usb.UsbManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.util.Log
import android.view.WindowManager
import android.widget.ImageView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AppCompatActivity
import com.hoho.android.usbserial.driver.UsbSerialPort
import com.hoho.android.usbserial.util.SerialInputOutputManager
import com.trediresearch.devssdashboard.databinding.MainBinding
import kotlinx.coroutines.*
import java.io.OutputStream
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.SocketException
import java.util.concurrent.Executors
import kotlin.io.outputStream


class MainActivity : AppCompatActivity() {

    companion object{
        lateinit var activity:MainActivity;


        var onStartNTRIPClient=false
        var ntripClient_host=""
        var ntripClient_port=0
        var ntripClient_username="";
        var ntripClient_password="";
        var ntripClient_mountpoint="";
    }


    private lateinit var binding: MainBinding

    private var mSerialPortConnection: SerialPortConnection? = null
    private var datagramPacket: DatagramPacket? = null
    private var UDPSocket:DatagramSocket?=null
    private var datagramSocket: DatagramSocket? = null
    private var arduSimpleReceiver: ArduSimpleRtcmReceiver? = null
    private var usbSerialManager: UsbSerialManager? = null
    private var arduSimpleForwarder: MavlinkRtcmForwarder? = null

    private val UDPSERVERPORT=14553

    private var address: InetAddress? = null
    private var currentPort: UsbSerialPort? = null

    @RequiresApi(Build.VERSION_CODES.O)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = MainBinding.inflate(layoutInflater)
        val view = binding.root

        activity=this;

        actionBar?.hide();

        getWindow().setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        );

        setContentView(view);




        StartSocketClient(InetAddress.getByName("127.0.0.1"))
        connectSerialReceiverH12()


        binding.btnGcs.setOnClickListener {
            startQGroundControl()
        }

        binding.btnUcamera.setOnClickListener {
            startUCamera()
        }

        binding.btnChrome.setOnClickListener {
            startChrome()
        }

        binding.btnSamba.setOnClickListener {
            startAntSMB()
        }

        binding.btnRemotedesktop.setOnClickListener {
            startRemoteDesktop()
        }

        binding.logo.setOnDoubleClickListener { openSettings() }
        requestPermission();
        loadPreferences();

        registerReceiver(usbDetachReceiver, IntentFilter(UsbManager.ACTION_USB_DEVICE_DETACHED))
        registerReceiver(usbAttachReceiver, IntentFilter(UsbManager.ACTION_USB_DEVICE_ATTACHED))
    }


    @RequiresApi(Build.VERSION_CODES.O)
    fun loadPreferences(){
        val sharedPref = getSharedPreferences("DevssDashboard", Context.MODE_PRIVATE)

        onStartNTRIPClient=sharedPref.getBoolean("on_start_ntripclient",false);
        ntripClient_host= sharedPref.getString("ntripclient_host","").toString();
        ntripClient_port=sharedPref.getInt("ntripclient_port",0);
        ntripClient_username= sharedPref.getString("ntripclient_username","").toString();
        ntripClient_password= sharedPref.getString("ntripclient_password","").toString();
        ntripClient_mountpoint= sharedPref.getString("ntripclient_mountpoint","").toString();

        if(onStartNTRIPClient){
            startNTRIPClient(
                ntripClient_host,
                ntripClient_port,
                ntripClient_username,
                ntripClient_password,
                ntripClient_mountpoint
            )
        }

        startArduSimpleUsb();
    }

    fun savePreference(){
        val sharedPref = getSharedPreferences("DevssDashboard", Context.MODE_PRIVATE)
        with(sharedPref.edit()) {
            putBoolean("on_start_ntripclient", onStartNTRIPClient)
            putString("ntripclient_host", ntripClient_host)
            putInt("ntripclient_port", ntripClient_port)
            putString("ntripclient_username", ntripClient_username)
            putString("ntripclient_password", ntripClient_password)
            putString("ntripclient_mountpoint", ntripClient_mountpoint)

            apply() // apply() salva in background in modo asincrono
        }
    }


    fun startQGroundControl(){
        val packageName = "org.mavlink.qgroundcontrolbeta"
        val activityName = "org.mavlink.qgroundcontrol.QGCActivity"

        try {
            val intent = Intent().apply {
                setClassName(packageName, activityName)
                action = Intent.ACTION_MAIN
                addCategory(Intent.CATEGORY_LAUNCHER)
            }
            startActivity(intent)

            //requireActivity().finish()
        } catch (e: Exception) {
            Toast.makeText(
                this,
                "App not installed",
                Toast.LENGTH_LONG
            ).show()
        }
    }

    fun startAntSMB(){
        val packageName = "lysesoft.andsmb"
        val activityName = "$packageName.SplashActivity"

        try {
            val intent = Intent().apply {
                setClassName(packageName, activityName)
                action = Intent.ACTION_MAIN
                addCategory(Intent.CATEGORY_LAUNCHER)
            }
            startActivity(intent)

            //requireActivity().finish()
        } catch (e: Exception) {
            Toast.makeText(
                this,
                "App not installed",
                Toast.LENGTH_LONG
            ).show()
        }
    }

    fun startRemoteDesktop(){
        val packageName = "com.microsoft.rdc.android"
        val activityName = "com.microsoft.rdc.ui.activities.HomeActivity"

        try {
            val intent = Intent().apply {
                setClassName(packageName, activityName)
                action = Intent.ACTION_MAIN
                addCategory(Intent.CATEGORY_LAUNCHER)
            }
            startActivity(intent)

            //requireActivity().finish()
        } catch (e: Exception) {
            Toast.makeText(
                this,
                "App not installed",
                Toast.LENGTH_LONG
            ).show()
        }
    }

    fun startUCamera(){
        val packageName = "com.trediresearch.ucamera"
        val activityName = "$packageName.MainActivity"

        try {
            val intent = Intent().apply {
                setClassName(packageName, activityName)
                action = Intent.ACTION_MAIN
                addCategory(Intent.CATEGORY_LAUNCHER)
            }
            startActivity(intent)

            //requireActivity().finish()
        } catch (e: Exception) {
            Toast.makeText(
                this,
                "App not installed",
                Toast.LENGTH_LONG
            ).show()
        }
    }


    fun startChrome(){
        val packageName = "com.google.android.apps"
        val activityName = "$packageName.Main"



        try {
            val browserIntent = Intent(Intent.ACTION_VIEW, Uri.parse("http://www.google.com"))
            startActivity(browserIntent)

            //requireActivity().finish()
        } catch (e: Exception) {
            Toast.makeText(
                this,
                "App not installed",
                Toast.LENGTH_LONG
            ).show()
        }
    }


    /// scan le port mis en parametre
    fun ReceiveData(portNum: Int) {
        object : Thread() {
            override fun run() {
                try {
                    val taille = 1024
                    val buffer = ByteArray(taille)

                    while (true) {
                        val data = DatagramPacket(buffer, buffer.size)
                        UDPSocket?.receive(data)
                        if(mSerialPortConnection!!.isConnection())
                            mSerialPortConnection!!.outputStream.write(data.data)




                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                }
            }
        }.start()
    }


    fun StartSocketClient(address: InetAddress) {
        try {
            this.UDPSocket = DatagramSocket()
            this.address=address
            ReceiveData(UDPSERVERPORT)
        } catch (e: SocketException) {
            e.printStackTrace()
        }
    }

    private val timerScope = CoroutineScope(Dispatchers.IO + Job())
    private var timerJob: Job? = null

    fun connectSerialReceiverH12() {




        val serialPortConnection: SerialPortConnection =
            SerialPortConnection.newBuilder("/dev/ttyHS1", 921600).flags(8192).build()

        mSerialPortConnection = serialPortConnection


        serialPortConnection.setDelegate(object : SerialPortConnection.Delegate {

            override fun connect() {
                Log.d("H12Starter", "Connected");
            }

            override fun received(param1ArrayOfbyte: ByteArray, param1Int: Int) {
                val addr = address
                val socket = UDPSocket
                if (addr == null || socket == null) {
                    Log.w("H12Starter", "Impossibile inoltrare: address o UDPSocket null")
                    return
                }
                try {
                    val packet = DatagramPacket(param1ArrayOfbyte, param1Int, addr, UDPSERVERPORT)
                    socket.send(packet)
                } catch (e: Exception) {
                    Log.e("H12Starter", "Errore invio UDP: ${e.message}", e)
                }
            }
        })
        try {
            serialPortConnection.openConnection();

        }catch (e: SecurityException){
            Log.d("UART3", e.message.toString());
        }

    }



    @RequiresApi(Build.VERSION_CODES.O)
    fun startNTRIPClient(host:String, port:Int, username:String, password:String, mountPoint:String){
        findViewById<TextView>(R.id.rtcm_enable).visibility= TextView.GONE
        val serialOutputStream = object : OutputStream() {
            override fun write(b: Int) {
                write(byteArrayOf(b.toByte()))
            }
            override fun write(b: ByteArray, off: Int, len: Int) {
                mSerialPortConnection?.outputStream?.write(b,off, b.size) // timeout in ms
            }
        }
        val forwarder = MavlinkRtcmForwarder(serialOutput = serialOutputStream)

        val client = NtripClient(
            context = this,
            host = host,
            port = port,
            mountPoint = mountPoint,
            username = username,
            password = password
        )
        client.setListener(object : NtripClient.NtripListener {
            override fun onRtcmData(data: ByteArray) {
                /* inoltra al ricevitore GNSS */
                forwarder.feed(data)
                findViewById<TextView>(R.id.rtcm_enable).visibility= TextView.VISIBLE




            }
            override fun onConnected() { }
            override fun onDisconnected() {
                findViewById<TextView>(R.id.rtcm_enable).visibility= TextView.GONE

                startNTRIPClient(
                    ntripClient_host,
                    ntripClient_port,
                    ntripClient_username,
                    ntripClient_password,
                    ntripClient_mountpoint
                )


            }
            override fun onError(error: String) {

                //riavvia l'NTRIP



                findViewById<TextView>(R.id.rtcm_enable).visibility= TextView.GONE
            }
            override fun onGgaSent(sentence: String) { }
        })
        client.startLocationUpdates()
        client.connect()
    }


    fun openSettings() {
        val dialog = SettingsDialogFragment()

        // Mostra il DialogFragment
        dialog.show(supportFragmentManager,"Settings")


    }


    fun requestPermission() {
        val haFineLocation = checkSelfPermission(
            Manifest.permission.ACCESS_FINE_LOCATION
        ) == PackageManager.PERMISSION_GRANTED

        if (!haFineLocation) {
            // Richiedi il permesso all'utente
            requestPermissions(
                arrayOf(
                    Manifest.permission.ACCESS_FINE_LOCATION,
                    Manifest.permission.ACCESS_COARSE_LOCATION
                ),
                1001 // Codice identificativo della richiesta
            )
        } else {
            // Il permesso è già stato concesso, puoi avviare il GPS o il Servizio

        }
    }


    fun startArduSimpleUsb() {
        val serialOutputStream = object : OutputStream() {
            override fun write(b: Int) {
                write(byteArrayOf(b.toByte()))
            }
            override fun write(b: ByteArray, off: Int, len: Int) {
                findViewById<TextView>(R.id.rtcm_enable).visibility= TextView.VISIBLE

                mSerialPortConnection?.outputStream?.write(b, off, len)
            }
        }
        val forwarder = MavlinkRtcmForwarder(serialOutput = serialOutputStream)
        arduSimpleForwarder = forwarder

        val manager = UsbSerialManager(this)
        manager.setListener(object : UsbSerialManager.Listener {
            override fun onPermissionGranted(port: UsbSerialPort) {
                currentPort=port;
                Log.d("ArduSimpleUSB", "Porta USB aperta, avvio lettura")
                startReading(port, forwarder)
            }
            override fun onPermissionDenied() {
                findViewById<TextView>(R.id.rtcm_enable).visibility= TextView.GONE

                Log.w("ArduSimpleUSB", "Permesso USB negato dall'utente")
            }
            override fun onDeviceNotFound() {
                findViewById<TextView>(R.id.rtcm_enable).visibility= TextView.GONE

                Log.w("ArduSimpleUSB", "Nessun dispositivo USB-seriale trovato")
            }
            override fun onError(message: String) {
                findViewById<TextView>(R.id.rtcm_enable).visibility= TextView.GONE

                Log.e("ArduSimpleUSB", message)
            }
        })
        manager.requestConnection()
        usbSerialManager = manager
    }


    private var ioManager: SerialInputOutputManager? = null

    private fun startReading(port: UsbSerialPort, forwarder: MavlinkRtcmForwarder) {
        ioManager = SerialInputOutputManager(port, object : SerialInputOutputManager.Listener {
            override fun onNewData(data: ByteArray) {
                forwarder.feed(data)
            }
            override fun onRunError(e: Exception) {
                Log.e("ArduSimpleUSB", "Errore lettura seriale: ${e.message}", e)
            }
        })
        Executors.newSingleThreadExecutor().submit(ioManager)
    }

    private fun stopReading() {
        ioManager?.listener = null
        ioManager?.stop()
        ioManager = null
    }

    override fun onDestroy() {
        super.onDestroy()
        usbSerialManager?.release()
        ioManager?.stop()
    }

    val usbAttachReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context, intent: Intent) {
            if (intent.action == UsbManager.ACTION_USB_DEVICE_ATTACHED) {
                usbSerialManager?.requestConnection()
            }
        }
    }

    val usbDetachReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context, intent: Intent) {
            if (intent.action == UsbManager.ACTION_USB_DEVICE_DETACHED) {
                stopReading()
                try { currentPort?.close() } catch (_: Exception) {}
                currentPort = null
            }
        }
    }

}

