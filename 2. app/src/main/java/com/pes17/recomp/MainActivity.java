package com.pes17.recomp;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends Activity {
    
    // A ponte para o C++
    public native String getNativeMessage();

    static {
        try {
            System.loadLibrary("pes17");
            Log.i("PES17", "Biblioteca carregada com sucesso!");
        } catch (UnsatisfiedLinkError e) {
            Log.e("PES17", "Falha ao carregar libpes17.so: " + e.getMessage());
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // Cria uma tela simples
        TextView textView = new TextView(this);
        textView.setTextSize(20);
        textView.setPadding(50, 50, 50, 50);
        
        try {
            // Chama a função do C++ para ver se a ponte funciona
            String msg = getNativeMessage();
            textView.setText("SUCESSO!\n" + msg);
        } catch (Exception e) {
            textView.setText("O APP ABRIU, mas o C++ não respondeu.\nErro: " + e.getMessage());
        }
        
        setContentView(textView);
    }
}
