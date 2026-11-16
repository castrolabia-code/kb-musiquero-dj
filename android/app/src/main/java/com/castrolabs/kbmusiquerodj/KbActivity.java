// =============================================================================
//  KB - MUSIQUERO DJ · Android (Fase 10)
//  Pantalla principal. JUCE dibuja todo desde C++ (libjuce_jni.so, que carga
//  com.rmsl.juce.JuceApp al iniciar la app); acá solo se configura la ventana:
//    * pantalla completa (sin barras del sistema; aparecen deslizando del borde),
//    * la pantalla NO se apaga mientras la app está abierta (se está pasando música),
//    * el contenido llega hasta los bordes (también en pantallas con muesca).
// =============================================================================
package com.castrolabs.kbmusiquerodj;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

public class KbActivity extends Activity
{
    @Override
    protected void onCreate (Bundle savedInstanceState)
    {
        super.onCreate (savedInstanceState);
        getWindow().addFlags (WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().getDecorView();   // crea la ventana ya (sin ella no hay control de las barras)
        enterFullScreen();
    }

    @Override
    public void onWindowFocusChanged (boolean hasFocus)
    {
        super.onWindowFocusChanged (hasFocus);
        if (hasFocus)
            enterFullScreen();   // al volver de otra app o de un diálogo del sistema
    }

    private void enterFullScreen()
    {
        // Pantalla completa es un "extra": si el sistema no lo permite, la app sigue igual.
        try { applyFullScreen(); }
        catch (RuntimeException e) { android.util.Log.w ("KBMusiqueroDJ", "Pantalla completa no disponible", e); }
    }

    @SuppressWarnings ("deprecation")
    private void applyFullScreen()
    {
        final Window window = getWindow();

        if (Build.VERSION.SDK_INT >= 30)
        {
            window.setDecorFitsSystemWindows (false);
            final WindowInsetsController controller = window.getInsetsController();
            if (controller != null)
            {
                controller.hide (WindowInsets.Type.systemBars());
                controller.setSystemBarsBehavior (WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        }
        else
        {
            window.getDecorView().setSystemUiVisibility (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                                                         | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                                                         | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                                                         | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                                                         | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                                                         | View.SYSTEM_UI_FLAG_FULLSCREEN);
        }

        if (Build.VERSION.SDK_INT >= 28)
            window.getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
    }
}
