package com.trediresearch.devssdashboard

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.util.Log
import android.view.WindowManager
import android.widget.Toast
import androidx.activity.ComponentActivity
import com.trediresearch.devssdashboard.databinding.MainBinding
import kotlinx.coroutines.*
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.SocketException
import kotlin.io.outputStream


class MainActivity : ComponentActivity() {
    private lateinit var binding: MainBinding

    private var mSerialPortConnection: SerialPortConnection? = null
    private var datagramPacket: DatagramPacket? = null
    private var UDPSocket:DatagramSocket?=null
    private var datagramSocket: DatagramSocket? = null



    private val UDPSERVERPORT=14553

    private var address: InetAddress? = null



    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = MainBinding.inflate(layoutInflater)
        val view = binding.root



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


        val serialPortConnection3: SerialPortConnection =
            SerialPortConnection.newBuilder("/dev/ttyHS0", 4000000).flags(8192).build()

        val serialPortConnection: SerialPortConnection =
            SerialPortConnection.newBuilder("/dev/ttyHS1", 921600).flags(8192).build()

       // serialPortConnection3!!.outputStream.write(10)

        serialPortConnection3.setDelegate(object : SerialPortConnection.Delegate{
            override fun connect() {
                Log.d("H12Starter", "Connected2");

                // Cancelliamo un eventuale timer precedente per sicurezza

                timerJob?.cancel()

                timerJob = timerScope.launch {
                    while (isActive) { // Continua finché la coroutine è attiva
                        try {
                            val messaggio="lorem ipsum daskdjaslkjlkas dlka jslkdj aksl jkldas lkjd ajlks djlka lkjd ajlks djlkas lkjd ajlk djlka";
                            val datiDaInviare = messaggio.toByteArray(Charsets.UTF_8)

                            serialPortConnection3.outputStream.write(datiDaInviare)
                            serialPortConnection3.outputStream.flush()
                            // Se necessario, fai il flush per forzare l'invio immediato dei dati
                        } catch (e: Exception) {
                            Log.e("H12Starter", "Errore durante l'invio seriale", e)
                            break // Interrompi il ciclo in caso di errore di connessione
                        }

                        delay(1000) // Aspetta 1 secondo (1000 millisecondi)
                    }
                }


            }

            override fun received(param1ArrayOfbyte: ByteArray, param1Int: Int) {
                val messaggioStringa = String(param1ArrayOfbyte, 0, param1Int, Charsets.UTF_8)

                Log.d("UART3", messaggioStringa);
            }
        })


        mSerialPortConnection = serialPortConnection


        serialPortConnection.setDelegate(object : SerialPortConnection.Delegate {

            override fun connect() {
                Log.d("H12Starter", "Connected");
            }

            override fun received(param1ArrayOfbyte: ByteArray, param1Int: Int) {
                val packet = DatagramPacket(param1ArrayOfbyte, param1Int, address, UDPSERVERPORT)
                UDPSocket!!.send(packet)

                Log.d("H12Starter", param1ArrayOfbyte.toString());
            }
        })
        try {
            serialPortConnection.openConnection();
            serialPortConnection3.openConnection();
        }catch (e: SecurityException){
            Log.d("UART3", e.message.toString());
        }

    }

}
