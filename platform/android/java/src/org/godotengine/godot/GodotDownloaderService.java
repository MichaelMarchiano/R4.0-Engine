package org.godotengine.godot;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import com.google.android.vending.expansion.downloader.impl.DownloaderService;

/**
 * This class demonstrates the minimal client implementation of the
 * DownloaderService from the Downloader library.
 */
public class GodotDownloaderService extends DownloaderService {
    // stuff for LVL -- MODIFY FOR YOUR APPLICATION!
    private static final String BASE64_PUBLIC_KEY = "REPLACE THIS WITH YOUR PUBLIC KEY";
    // used by the preference obfuscater
    private static final byte[] SALT = new byte[] {
	    1, 43, -12, -1, 54, 98,
	    -100, -12, 43, 2, -8, -4, 9, 5, -106, -108, -33, 45, -1, 84
    };

    /**
     * This public key comes from your Android Market publisher account, and it
     * used by the LVL to validate responses from Market on your behalf.
     */
    @Override
    public String getPublicKey() {
    	SharedPreferences prefs = getApplicationContext().getSharedPreferences("app_data_keys", Context.MODE_PRIVATE);
    	Log.d("GODOT", "getting public key:" + prefs.getString("store_public_key", null));
    	return prefs.getString("store_public_key", null);
		
//	return BASE64_PUBLIC_KEY;
    }

    /**
     * This is used by the preference obfuscater to make sure that your
     * obfuscated preferences are different than the ones used by other
     * applications.
     */
    @Override
    public byte[] getSALT() {
	return SALT;
    }

    /**
     * Fill this in with the class name for your alarm receiver. We do this
     * because receivers must be unique across all of Android (it's a good idea
     * to make sure that your receiver is in your unique package)
     */
    @Override
    public String getAlarmReceiverClassName() {
    	Log.d("GODOT", "getAlarmReceiverClassName()");
    	return GodotDownloaderAlarmReceiver.class.getName();
    }

}
