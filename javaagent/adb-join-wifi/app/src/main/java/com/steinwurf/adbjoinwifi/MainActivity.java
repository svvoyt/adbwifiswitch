package com.steinwurf.adbjoinwifi;

import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.ProxyInfo;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.RequiresApi;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.Gravity;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;
import android.widget.Toast;

import java.lang.reflect.Field;
import java.text.ParseException;

public class MainActivity extends AppCompatActivity implements CheckSSIDBroadcastReceiver.SSIDFoundListener
{
    private static final String TAG = "adbjoinwifi";

    private static final String WEP_PASSWORD = "WEP";
    private static final String WPA_PASSWORD = "WPA";

    private static final String RUNMODE = "mode";
    private static final String SSID = "ssid";
    private static final String PASSWORD_TYPE = "password_type";
    private static final String PASSWORD = "password";
    private static final String UNIQPARAM = "uniq";

    private static final String ConnectSignature = "Mode connect run completed";
    private static final String DisconnectSignature = "Mode disconnect run completed";

    private static final String CLEAR_DEVICE_ADMIN = "clear_device_admin";

    enum RunMode {Connect, Disconnect};

    private RunMode m_runMode = RunMode.Connect;

    String mSSID;
    String mPassword;
    String mPasswordType;
    String mUniqTag;

    CheckSSIDBroadcastReceiver broadcastReceiver;
    WifiManager mWifiManager;

    Thread mThread;

    private void printUsage(final String errmsg)
    {
        Log.d(TAG, errmsg + " Use the following adb command:");
        Log.d(TAG,
                "adb shell am start" +
                " -n com.steinwurf.adbjoinwifi/.MainActivity " +
                "-e mode [connect|disconnect]" +
                "-e uniq <uniq tag>" +
                "-e ssid SSID " +
                "-e password_type [WEP|WPA] " +
                "-e password PASSWORD\n" +
                "If app was granted device owner using dpm, you can unset it with:\n" +
                "    -e clear_device_admin true");
        Toast.makeText(this, "This application is meant to be used with ADB",
                Toast.LENGTH_SHORT).show();
        finish();
    }

    final boolean modeConnect() {
        return m_runMode == RunMode.Connect;
    }

    final void notifyConnected() {
        Log.w(TAG, mUniqTag + " " + ConnectSignature + " " + mSSID);
    }

    final void notifyDisconnected() {
        Log.w(TAG, mUniqTag + " " + DisconnectSignature);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        boolean clearDeviceAdmin = getIntent().getExtras() != null && getIntent().getExtras().containsKey(CLEAR_DEVICE_ADMIN);

        if (clearDeviceAdmin) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                clearDeviceOwner();
            }
            else
            {
                throw new UnsupportedOperationException("API level 21 or higher required for this");
            }
            finish();
            return;
        }

        // Get Content
        mSSID = getIntent().getStringExtra(SSID);
        mPasswordType = getIntent().getStringExtra(PASSWORD_TYPE);
        mPassword = getIntent().getStringExtra(PASSWORD);
        mUniqTag = getIntent().getStringExtra(UNIQPARAM);

        final String mode = getIntent().getStringExtra(RUNMODE);


        if (mode != null) {
            if (mode.equals("connect")) m_runMode = RunMode.Connect;
            else if (mode.equals("disconnect")) m_runMode = RunMode.Disconnect;
            else {
                printUsage("Unknown mode " + mode + ".");
                return;
            }
        }

        // Validate
        if (modeConnect()) {
            if ((mSSID == null) || // SSID REQUIRED
                    (mPasswordType != null && mPassword == null) || // PASSWORD REQUIRED IF PASSWORD TYPE GIVEN
                    (mPassword != null && mPasswordType == null) || // PASSWORD TYPE REQUIRED IF PASSWORD GIVEN
                    (mPasswordType != null && !mPasswordType.equals(WPA_PASSWORD) && !mPasswordType.equals(WEP_PASSWORD))) // PASSWORD TYPE MUST BE NULL OR WPA OR WEP
            {
                printUsage("Connect mode: no datastring provided.");
                return;
            }

            Log.d(TAG, "Trying to join:");
            Log.d(TAG, "SSID: " + mSSID);
            if(mPasswordType != null && mPassword != null)
            {
                Log.d(TAG, "Password Type: " + mPasswordType);
                Log.d(TAG, "Password: " + mPassword);
            }

        } else {
            mSSID = mPassword = mPasswordType = "";
        }

        // Setup layout

        LinearLayout layout = new LinearLayout(this);
        setContentView(layout);

        layout.setGravity(Gravity.CENTER);
        layout.setOrientation(LinearLayout.VERTICAL);

        LayoutParams params = new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);

        TextView textview = new TextView(this);
        textview.setText(getString(R.string.trying_to_connect_to));
        textview.setTextSize(20);
        layout.addView(textview, params);

        TextView SSIDtextview = new TextView(this);
        SSIDtextview.setText(mSSID);
        layout.addView(SSIDtextview, params);

        mWifiManager = (WifiManager)getApplicationContext().getSystemService(WIFI_SERVICE);

        if (!modeConnect()) {
            if (mWifiManager.isWifiEnabled()) {
                mWifiManager.disconnect();
                mWifiManager.setWifiEnabled( false );
            }
            notifyDisconnected();
            finish();
            return;
        }

        // Setup broadcast receiver

        broadcastReceiver = new CheckSSIDBroadcastReceiver(mSSID);
        broadcastReceiver.setSSIDFoundListener(this);

        IntentFilter filter = new IntentFilter();
        filter.addAction(WifiManager.WIFI_STATE_CHANGED_ACTION);
        filter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        registerReceiver(broadcastReceiver, filter);

        // Check if wifi is enabled, and act accordingly

        if (!mWifiManager.isWifiEnabled())
            mWifiManager.setWifiEnabled(true);
        else {
            WifiEnabled();
        }
    }

    @Override
    protected void onDestroy()
    {
        super.onDestroy();
        Log.d(TAG, "onDestroy");
        if (broadcastReceiver != null)
        unregisterReceiver(broadcastReceiver);
        broadcastReceiver = null;
    }

    @Override
    public void SSIDFound()
    {
        Log.d(TAG, "Device Connected to " + mSSID);
        if (mThread != null)
        {
            mThread.interrupt();
            try
            {
                mThread.join();
            }
            catch (InterruptedException e)
            {
                Log.e(TAG, "Hit exception", e);
            }
        }
        notifyConnected();
        finish();
    }

    @Override
    public void WifiEnabled()
    {
        Log.d(TAG, "WifiEnabled");
        if (mThread != null)
            return;

        WifiConfiguration wfc = getExistingWifiConfiguration();
        int networkId;

        if (wfc == null)
        {
            // Wifi configuration didn't exist for this SSID, create it.
            wfc = new WifiConfiguration();
            updateWifiConfiguration(wfc);
            networkId = mWifiManager.addNetwork(wfc);
        }
        else if (permittedToUpdate(wfc))
        {
            // Wifi configuration already exists, update if we can
            updateWifiConfiguration(wfc);
            networkId = mWifiManager.updateNetwork(wfc);
        }
        else {
            // Wifi configuration already exists, we cannot update it so just join it
            networkId = wfc.networkId;
        }

        if (networkId == -1)
        {
            Log.d(TAG, "Invalid wifi network (ensure this SSID exists, auth method and password are correct, etc.)");
            finish();
            return;
        }

        final int finalNetworkId = networkId;

        mThread = new Thread() {
            @Override
            public void run() {
                mWifiManager.disconnect();
                try
                {
                    while(!isInterrupted())
                    {
                        Log.d(TAG, "Joining, network id=" + Integer.toString(finalNetworkId));
                        mWifiManager.enableNetwork(finalNetworkId, true);
                        mWifiManager.reconnect();
                        // Wait and see if it worked. Otherwise try again.
                        sleep(10000);
                    }
                } catch (InterruptedException ignored) {
                }
            }
        };
        mThread.start();
    }

    private boolean permittedToUpdate(WifiConfiguration wfc)
    {
        Field field;
        int creatorUid;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            try
            {
                field = wfc.getClass().getDeclaredField("creatorUid");
                creatorUid = field.getInt(wfc);
            }
            catch (ReflectiveOperationException e)
            {
                Log.e(TAG, "Hit exception", e);
                return false;
            }
            if (creatorUid == getApplicationInfo().uid || canEditWifi())
            {
                Log.d(TAG, "App is permitted to modify this wifi configuration");
                return true;
            }
        }
        // Since app doesn't have proper permissions, we will join the existing Wifi network as configured
        Log.w(TAG, "App does not have admin access, unable to modify a wifi network created by another app");
        return false;
    }

    private void updateWifiConfiguration(WifiConfiguration wfc)
    {
        wfc.SSID = "\"".concat(mSSID).concat("\"");
        wfc.status = WifiConfiguration.Status.ENABLED;
        wfc.priority = 100;
        if (mPasswordType == null) // no password
        {
            wfc.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
            wfc.allowedProtocols.set(WifiConfiguration.Protocol.RSN);
            wfc.allowedProtocols.set(WifiConfiguration.Protocol.WPA);
            wfc.allowedAuthAlgorithms.clear();
            wfc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.CCMP);
            wfc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.TKIP);
            wfc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP40);
            wfc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP104);
            wfc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.CCMP);
            wfc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.TKIP);
        }
        else if (mPasswordType.equals(WEP_PASSWORD)) // WEP
        {
            wfc.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
            wfc.allowedProtocols.set(WifiConfiguration.Protocol.RSN);
            wfc.allowedProtocols.set(WifiConfiguration.Protocol.WPA);
            wfc.allowedAuthAlgorithms.set(WifiConfiguration.AuthAlgorithm.OPEN);
            wfc.allowedAuthAlgorithms.set(WifiConfiguration.AuthAlgorithm.SHARED);
            wfc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.CCMP);
            wfc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.TKIP);
            wfc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP40);
            wfc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP104);

            // if hex string
            // wfc.wepKeys[0] = password;

            wfc.wepKeys[0] = "\"".concat(mPassword).concat("\"");
            wfc.wepTxKeyIndex = 0;
        }
        else if (mPasswordType.equals(WPA_PASSWORD)) // WPA(2)
        {
            wfc.allowedProtocols.set(WifiConfiguration.Protocol.RSN);
            wfc.allowedProtocols.set(WifiConfiguration.Protocol.WPA);
            wfc.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);
            wfc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.CCMP);
            wfc.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.TKIP);
            wfc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP40);
            wfc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP104);
            wfc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.CCMP);
            wfc.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.TKIP);

            wfc.preSharedKey = "\"".concat(mPassword).concat("\"");
        }
    }

    private WifiConfiguration getExistingWifiConfiguration()
    {
        for( WifiConfiguration i : mWifiManager.getConfiguredNetworks())
        {
            if(i.SSID != null && i.SSID.equals("\"".concat(mSSID).concat("\"")))
            {
                Log.d(TAG, "wifi network already exists.");
                return i;
            }
        }
        return null;
    }

    @RequiresApi(api = Build.VERSION_CODES.JELLY_BEAN_MR2)
    private boolean canEditWifi()
    {
        DevicePolicyManager devicePolicyManager =
                (DevicePolicyManager) getSystemService(Context.DEVICE_POLICY_SERVICE);

        return devicePolicyManager.isAdminActive(new ComponentName(this, AdminReceiver.class)) &&
               devicePolicyManager.isDeviceOwnerApp(getPackageName());
    }

    @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
    private void clearDeviceOwner()
    {
        if (canEditWifi())
        {
            DevicePolicyManager devicePolicyManager =
                    (DevicePolicyManager) getSystemService(Context.DEVICE_POLICY_SERVICE);

            devicePolicyManager.clearDeviceOwnerApp(getPackageName());
        }
    }
}
