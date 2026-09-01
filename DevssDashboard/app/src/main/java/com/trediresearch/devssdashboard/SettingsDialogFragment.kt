package com.trediresearch.devssdashboard

import android.content.Context
import androidx.fragment.app.DialogFragment // Usa questa!
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import android.widget.ToggleButton

class SettingsDialogFragment : DialogFragment() {

    private lateinit var onStartNTRIPClient: ToggleButton;

    private lateinit var ntripClient_host: EditText;
    private lateinit var ntripClient_port: EditText;
    private lateinit var ntripClient_username: EditText;
    private lateinit var ntripClient_password: EditText;
    private lateinit var ntripClient_mountpoint: EditText;


    // Listener per restituire i dati all'Activity/Fragment chiamante
    interface OnImpostazioniSalvateListener {
        fun onImpostazioniSalvate(nome: String, attiva: Boolean, intensita: Float)
    }

    var listener: OnImpostazioniSalvateListener? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.settings, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)



        onStartNTRIPClient=view.findViewById<ToggleButton>(R.id.onStartNtripClient)
        ntripClient_host=view.findViewById<EditText>(R.id.ntripclient_host)
        ntripClient_port=view.findViewById<EditText>(R.id.ntripclient_port)
        ntripClient_username=view.findViewById<EditText>(R.id.ntripclient_username)
        ntripClient_password=view.findViewById<EditText>(R.id.ntripclient_password)
        ntripClient_mountpoint=view.findViewById<EditText>(R.id.ntripclient_mountpoint)

        if(MainActivity.onStartNTRIPClient)
            onStartNTRIPClient.toggle()
        ntripClient_host.setText(MainActivity.ntripClient_host.toString())
        ntripClient_port.setText(MainActivity.ntripClient_port.toString())
        ntripClient_username.setText(MainActivity.ntripClient_username)
        ntripClient_password.setText(MainActivity.ntripClient_password)
        ntripClient_mountpoint.setText(MainActivity.ntripClient_mountpoint)

        // Riferimenti ai controlli del layout
        view.findViewById<Button>(R.id.btnSave).setOnClickListener {
            save()
        }
    }

    fun save(){
        MainActivity.onStartNTRIPClient= onStartNTRIPClient.isChecked

        MainActivity.ntripClient_host= ntripClient_host.text.toString();
        MainActivity.ntripClient_port=  ntripClient_port.text.toString().toInt();
        MainActivity.ntripClient_username= ntripClient_username.text.toString();
        MainActivity.ntripClient_password= ntripClient_password.text.toString();
        MainActivity.ntripClient_mountpoint= ntripClient_mountpoint.text.toString();

        MainActivity.activity.savePreference()

        if(MainActivity.onStartNTRIPClient)
            MainActivity.activity.startNTRIPClient(
                MainActivity.Companion.ntripClient_host,
                MainActivity.Companion.ntripClient_port,
                MainActivity.Companion.ntripClient_username,
                MainActivity.Companion.ntripClient_password,
                MainActivity.Companion.ntripClient_mountpoint
            )

        this.dismiss()
    }

    // Facoltativo: Imposta larghezza quasi a schermo intero
    override fun onStart() {
        super.onStart()
        dialog?.window?.setLayout(
            (resources.displayMetrics.widthPixels * 0.90).toInt(),
            ViewGroup.LayoutParams.WRAP_CONTENT
        )
    }
}