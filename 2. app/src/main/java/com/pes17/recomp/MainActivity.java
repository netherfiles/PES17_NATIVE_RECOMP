package com.pes17.recomp;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

public class MainActivity extends Activity {
    public native String getNativeMessage();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        TextView textView = new TextView(this);
        textView.setTextSize(18);
        textView.setPadding(40, 40, 40, 40);
        textView.setText("Iniciando o app...\n");
        setContentView(textView);

        try {
            System.loadLibrary("pes17");
            textView.append("✅ libpes17.so carregada!\n");
        } catch (Throwable t) {
            textView.append("❌ Erro ao carregar libpes17.so:\n" + t.getMessage());
            return;
        }

        try {
            String msg = getNativeMessage();
            textView.append("✅ Ponte JNI funcionou:\n" + msg);
        } catch (Throwable t) {
            textView.append("❌ Erro na Ponte JNI:\n" + t.getMessage());
            Log.e("PES17", "Erro JNI", t);
        }
    }
}
