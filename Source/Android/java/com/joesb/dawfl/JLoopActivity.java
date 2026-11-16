package com.joesb.dawfl;

import android.app.Activity;
import android.content.Intent;

/** Activity di AudioNaut: come quella standard, ma quando l'app e' gia' aperta e arriva un file
    ("Apri con AudioNaut") lo rende visibile con getIntent(), cosi' l'app lo trova e lo apre. */
public class JLoopActivity extends Activity
{
    @Override
    protected void onNewIntent (Intent intent)
    {
        super.onNewIntent (intent);
        setIntent (intent);
    }
}
