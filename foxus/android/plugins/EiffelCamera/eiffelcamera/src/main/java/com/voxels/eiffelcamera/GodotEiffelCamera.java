/*************************************************************************/
/* Copyright (c) 2022 Nolan Consulting Limited.                          */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

package com.voxels.eiffelcamera;

import org.godotengine.godot.Godot;
import org.godotengine.godot.plugin.SignalInfo;
import org.godotengine.godot.utils.PermissionsUtil;

import java.util.Arrays;
import java.util.List;
import java.util.Set;
import java.util.HashMap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.collection.ArraySet;

import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;
import android.util.Log;

import android.app.PendingIntent;
import android.content.Context;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;



public class GodotEiffelCamera extends org.godotengine.godot.plugin.GodotPlugin{

    UsbManager usbManager;
    UsbDevice camDevice;
    PendingIntent usbPermIntent;

    int fileDescriptor;

    public GodotEiffelCamera(Godot godot) {
        super(godot);
        fileDescriptor=-1;
    }

    static final String ACTION_USB_PERMISSION = "android.permission.USB_PERMISSION";

    public class PermissionReceiver extends BroadcastReceiver {
        GodotEiffelCamera eiffelCamera;

        PermissionReceiver(GodotEiffelCamera eiffelCamera) {
            super();
            this.eiffelCamera = eiffelCamera;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            //Log.d("PERMISSIONS","ACTION RECV: "+action);
            if (ACTION_USB_PERMISSION.equals(action)) {
                synchronized (this) {
                    //Log.d("PERMISSIONS","INTENT " + intent);
                    UsbDevice device = (UsbDevice)intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);

                    if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                        if(device != null) {
                            camDevice = device;
                            //call method to set up device communication
                            UsbDeviceConnection usbDeviceConnection = usbManager.openDevice(camDevice);
                            fileDescriptor = usbDeviceConnection.getFileDescriptor();
                            Log.d("PERMISSIONS","got permissions for device FD: " + fileDescriptor);
                            eiffelCamera.emitSignal("permission_received", fileDescriptor);
                            Log.d("DEBUG", "Emitting signal permission received " + fileDescriptor);
                        }
                    }
                    else {
                        Log.d("PERMISSIONS", "permission denied for device");
                    }
                }
            }
        }
    }

    @Override
    public String getPluginName() {
        return "EiffelCamera";
    }

    @Override
    protected Set<String> getPluginGDNativeLibrariesPaths() {
        return super.getPluginGDNativeLibrariesPaths();
    }

    @Override
    public List<String> getPluginMethods() {
        return Arrays.asList("isCameraConnected","connectCamera", "isCameraAttached");
    }

    public int isCameraConnected() {
        return fileDescriptor;
    }

    public boolean connectCamera(int vid,int pid) {
        GodotEiffelCamera.PermissionReceiver p = new GodotEiffelCamera.PermissionReceiver(this);
        fileDescriptor = -1;
        Context c = this.getActivity().getApplicationContext();
        usbPermIntent = PendingIntent.getBroadcast(c, 0, new Intent(ACTION_USB_PERMISSION), 0);
        IntentFilter filter = new IntentFilter(ACTION_USB_PERMISSION);
        this.getActivity().registerReceiver(p, filter);
        usbManager = (UsbManager) this.getActivity().getSystemService(c.USB_SERVICE);
        HashMap<String, UsbDevice> deviceList = usbManager.getDeviceList();
        for (UsbDevice usbDevice : deviceList.values()) {
            if (usbDevice.getVendorId() == vid && usbDevice.getProductId() == pid) {
                camDevice = usbDevice;
                usbManager.requestPermission(camDevice, usbPermIntent);
                PermissionsUtil.requestPermission("CAMERA", getActivity());
                return true;
            }
        }
        return false;
    }

    public boolean isCameraAttached(int vid, int pid) {
        Context c = this.getActivity().getApplicationContext();

        UsbManager usbManager = (UsbManager) this.getActivity().getSystemService(c.USB_SERVICE);

        HashMap<String, UsbDevice> deviceList = usbManager.getDeviceList();

        for (UsbDevice usbDevice : deviceList.values()) {
            if (usbDevice.getVendorId() == vid && usbDevice.getProductId() == pid) {
                return true;
            }
        }

        return false;
    }

	@NonNull
	@Override
	public Set<SignalInfo> getPluginSignals() {
		Set<SignalInfo> signals = new ArraySet<>();

		signals.add(new SignalInfo("permission_received", Integer.class));

		return signals;
	}
}
