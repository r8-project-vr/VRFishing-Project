// Copyright (c) 2020 Nineva Studios
package com.ninevastudios.ble;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.pm.PackageManager;
import android.os.ParcelUuid;
import android.util.Log;
import android.util.SparseArray;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

public class BleGoodiesManager {

	public static native void onDeviceFound(long callbackAddr, BleGoodiesDevice device);
	public static native void onAdvertisement(long callbackAddr, String deviceName, byte[] bytes);
	public static native void onScanError(String errorMessage);

	private BluetoothAdapter mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
	private Activity mActivity = null;
	private long mCallbackAddr = 0;
	private boolean mScanningForAdvertisements = false;

	private HashSet<String> mFoundDevices = new HashSet<String>();

	private ScanCallback mScanCallback = new ScanCallback() {
		@Override
		public void onScanResult(int callbackType, final ScanResult result) {
			if (mScanningForAdvertisements) {
				ScanRecord record = result.getScanRecord();
				if (record != null) {
					SparseArray<byte[]> manufacturerData = record.getManufacturerSpecificData();
					for (int i = 0; i < manufacturerData.size(); i++) {
						BleGoodiesUtils.log("Record found: " + record);
						onAdvertisement(mCallbackAddr, record.getDeviceName(), manufacturerData.valueAt(i));
					}
				}
			}
			else {
				BluetoothDevice device = result.getDevice();
				if (!mFoundDevices.contains(device.getAddress())) {
					mFoundDevices.add(device.getAddress());
					BleGoodiesUtils.log(String.format("Device found: %s, name: %s, alias: %s", device, device.getName(), device.getAlias()));
					onDeviceFound(mCallbackAddr, new BleGoodiesDevice(mActivity, device));
				}
			}
		}
		@Override
		public void onScanFailed(int errorCode) {
			onScanError("Failed to start scan: " + errorCode);
		}
	};

	public BleGoodiesManager(Activity activity, long callbackAddr) {
		mActivity = activity;
		mCallbackAddr = callbackAddr;
	}

	public boolean isBleSupported() {
		return mActivity.getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE);
	}

	public boolean isBluetoothEnabled() {
		return mBluetoothAdapter != null && mBluetoothAdapter.isEnabled();
	}

	public void setBluetoothState(boolean isEnabled) {
		if (mBluetoothAdapter == null) return;

		if (isEnabled) {
			mBluetoothAdapter.enable();
		} else {
			mBluetoothAdapter.disable();
		}
	}

	public void startScanForDevices(String[] uuids) {
		if (mBluetoothAdapter == null) return;

		mScanningForAdvertisements = false;
		BluetoothLeScanner scanner =  mBluetoothAdapter.getBluetoothLeScanner();
		try {
			if (uuids.length == 0) {
				scanner.startScan(mScanCallback);
			} else {
				List<ScanFilter> filters = new ArrayList<>();
				for (String uuid : uuids) {
					filters.add(new ScanFilter.Builder().setServiceUuid(ParcelUuid.fromString(uuid)).build());
				}

				ScanSettings settings = new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build();
				mFoundDevices.clear();
				scanner.startScan(filters, settings, mScanCallback);
			}
		}
		catch (Exception e) {
			onScanError("Failed to start scan: " +  e.toString());
		}
	}

	public void startScanForAdvertisements(String[] nameFilters) {
		if (mBluetoothAdapter == null) return;

		mScanningForAdvertisements = true;
		BluetoothLeScanner scanner =  mBluetoothAdapter.getBluetoothLeScanner();
		try {
			if (nameFilters.length == 0) {
				scanner.startScan(mScanCallback);
			} else {
				List<ScanFilter> filters = new ArrayList<>();
				for (String nameFilter : nameFilters) {
					filters.add(new ScanFilter.Builder().setDeviceName(nameFilter).build());
				}

				ScanSettings settings = new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build();
				scanner.startScan(filters, settings, mScanCallback);
			}
		}
		catch (Exception e) {
			onScanError("Failed to start scan: " +  e.toString());
		}
	}

	public void stopScan() {
		if (mBluetoothAdapter == null) return;

		BluetoothLeScanner scanner =  mBluetoothAdapter.getBluetoothLeScanner();
		scanner.stopScan(mScanCallback);
	}

}
