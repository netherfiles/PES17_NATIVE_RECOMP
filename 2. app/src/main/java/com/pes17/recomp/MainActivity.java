package com.pes17.recomp;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

public class MainActivity extends Activity {
    static {
        try {
            System.loadLibrary("pes17");
        } catch (UnsatisfiedLinkError e) {
            Log.e("PES17", "Falha ao carregar libpes17.so: " + e.getMessage());
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i("PES17", "App iniciado. A biblioteca nativa foi carregada!");
    }
}
