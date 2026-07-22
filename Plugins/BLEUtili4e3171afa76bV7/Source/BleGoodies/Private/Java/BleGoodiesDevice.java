// Copyright (c) 2020 Nineva Studios
package com.ninevastudios.ble;

import android.app.Activity;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

public class BleGoodiesDevice {

	public static native void onConnect(long callbackAddr);
	public static native void onDisconnect(long callbackAddr);
	public static native void onConnectError(long callbackAddr, String errorMessage);
	public static native void onDisconnectError(long callbackAddr, String errorMessage);
	public static native void onRead(long callbackAddr, String service, String characteristic, byte[] data);
	public static native void onWrite(long callbackAddr, String service, String characteristic);
	public static native void onNotification(long callbackAddr, String service, String characteristic, byte[] data);
	public static native void onUnsubscribe(long callbackAddr, String service, String characteristic);
	public static native void onError(long callbackAddr, String service, String characteristic, String errorMessage);

	private Activity mActivity = null;
	private BluetoothDevice mDevice = null;
	private BluetoothGatt mConnection = null;
	private int mState = BluetoothGatt.STATE_DISCONNECTED;
	private boolean mServicesDiscovered = false;
	private long mCallbackAddr = 0;

	public BleGoodiesDevice(Activity activity, BluetoothDevice device) {
		mActivity = activity;
		mDevice = device;
	}

	public void initCallbacks(long callbackAddr) { mCallbackAddr = callbackAddr; }

	public void connect() {
		mConnection = mDevice.connectGatt(mActivity, false, getGattCallback());
	}

	public void disconnect() {
		if (mConnection == null) return;

		mConnection.close();
		mConnection.disconnect();
		mConnection = null;

		mState = BluetoothGatt.STATE_DISCONNECTED;
		mServicesDiscovered = false;

		onDisconnect(mCallbackAddr);
	}

	public boolean isConnected() {
		return mState == BluetoothGatt.STATE_CONNECTED;
	}

	public String getName() {
		return mDevice.getName();
	}

	public String getId() {
		return mDevice.getAddress();
	}

	public void discoverService() {
		if (mState == BluetoothGatt.STATE_CONNECTED) {
			mConnection.discoverServices();
		}
	}

	public String[] getServicesUUIDs() {
		if (!mServicesDiscovered) return new String[0];

		List<BluetoothGattService> serviceList = mConnection.getServices();
		ArrayList<String> serviceUUIDs = new ArrayList<String>();

		for (BluetoothGattService service : serviceList)
			serviceUUIDs.add(service.getUuid().toString());

		return serviceUUIDs.toArray(new String[0]);
	}

	public boolean hasService(String serviceUUIDString) {
		if (!mServicesDiscovered) return false;

		BluetoothGattService service = null;
		try {
			UUID serviceUUID = UUID.fromString(serviceUUIDString);
			service = mConnection.getService(serviceUUID);
		}
		catch(Exception e) {
			Log.d("BleGoodies", e.toString());
		}

		return service != null;
	}

	private BluetoothGattCharacteristic getCharacteristic(String serviceUUIDString, String characteristicUUIDString) {
		if (!isConnected()) {
			onError(mCallbackAddr, serviceUUIDString, characteristicUUIDString, "Cannot perform operation. Device is not connected.");
			return null;
		}

		if (!mServicesDiscovered) {
			onError(mCallbackAddr, serviceUUIDString, characteristicUUIDString, "Service unavailable");
			return null;
		}

		UUID serviceUUID = UUID.fromString(serviceUUIDString);
		UUID characteristicUuid = UUID.fromString(characteristicUUIDString);

		BluetoothGattService service = mConnection.getService(serviceUUID);
		if (service == null) {
			onError(mCallbackAddr, serviceUUIDString, characteristicUUIDString, "Service unavailable");
			return null;
		}

		BluetoothGattCharacteristic characteristic = service.getCharacteristic(characteristicUuid);
		if (characteristic == null) {
			onError(mCallbackAddr, serviceUUIDString, characteristicUUIDString, "Characteristic unavailable");
			return null;
		}

		return characteristic;
	}

	public void readCharacteristic(String serviceUUIDString, String characteristicUUIDString) {
		try {
			BluetoothGattCharacteristic characteristic = getCharacteristic(serviceUUIDString, characteristicUUIDString);
			if (characteristic != null) {
				mConnection.readCharacteristic(characteristic);
			}
		}
		catch (Exception e) {
			onError(mCallbackAddr, serviceUUIDString, characteristicUUIDString, "Failed to read characteristic: " + e.toString());
		}
	}

	public void writeCharacteristic(String serviceUUIDString, String characteristicUUIDString, byte[] data) {
		try {
			BluetoothGattCharacteristic characteristic = getCharacteristic(serviceUUIDString, characteristicUUIDString);
			if (characteristic != null) {
				characteristic.setValue(data);
				mConnection.writeCharacteristic(characteristic);
			}
		}
		catch (Exception e) {
			onError(mCallbackAddr, serviceUUIDString, characteristicUUIDString, "Failed to write characteristic: " + e.toString());
		}
	}

	public void subscribeToCharacteristic(String serviceUUIDString, String characteristicUUIDString, boolean isIndication) {
		try {
			BluetoothGattCharacteristic characteristic = getCharacteristic(serviceUUIDString, characteristicUUIDString);
			if (characteristic != null) {
				mConnection.setCharacteristicNotification(characteristic, true);
				BluetoothGattDescriptor descriptor = characteristic.getDescriptor(clientCharacteristicConfigurationUUID());
				if (descriptor != null) {
					descriptor.setValue(isIndication ?
							BluetoothGattDescriptor.ENABLE_INDICATION_VALUE :
							BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
					mConnection.writeDescriptor(descriptor);
				} else {
					onError(mCallbackAddr, serviceUUIDString, characteristicUUIDString, "Failed to subscribe");
				}
			}
		}
		catch (Exception e) {
			onError(mCallbackAddr, serviceUUIDString, characteristicUUIDString, "Failed to subscribe: " + e.toString());
		}
	}

	public void unsubscribeFromCharacteristic(String serviceUUIDString, String characteristicUUIDString) {
		try {
			BluetoothGattCharacteristic characteristic = getCharacteristic(serviceUUIDString, characteristicUUIDString);
			if (characteristic != null) {
				mConnection.setCharacteristicNotification(characteristic, true);
				BluetoothGattDescriptor descriptor = characteristic.getDescriptor(clientCharacteristicConfigurationUUID());
				if (descriptor != null) {
					descriptor.setValue(BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE);
					mConnection.writeDescriptor(descriptor);
					onUnsubscribe(mCallbackAddr, serviceUUIDString, characteristicUUIDString);
				} else {
					onError(mCallbackAddr, serviceUUIDString, characteristicUUIDString, "Failed to unsubscribe");
				}
			}
		}
		catch (Exception e) {
			onError(mCallbackAddr, serviceUUIDString, characteristicUUIDString, "Failed to unsubscribe: " + e.toString());
		}
	}

	private BluetoothGattCallback getGattCallback() {
		return new BluetoothGattCallback() {
			@Override
			public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
				if (status == BluetoothGatt.GATT_SUCCESS) {
					mState = newState;
					if (mState == BluetoothGatt.STATE_CONNECTED) {
						discoverService();
					}
				}
				// TODO figure out how to properly detect connection/disconnection failures
				// simply checking status other than SUCCESS gives false positives
			}

			@Override
			public void onServicesDiscovered(BluetoothGatt gatt, int status) {
				if (status == BluetoothGatt.GATT_SUCCESS) {
					mServicesDiscovered = true;
					onConnect(mCallbackAddr);
				}
			}

			@Override
			public void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
				if (status == BluetoothGatt.GATT_SUCCESS) {
					onRead(mCallbackAddr, characteristic.getService().getUuid().toString(), characteristic.getUuid().toString(), characteristic.getValue());
				}
				else {
					onError(mCallbackAddr, characteristic.getService().getUuid().toString(), characteristic.getUuid().toString(), "Read failed. Error code: " + status);
				}
			}

			@Override
			public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
				if (status == BluetoothGatt.GATT_SUCCESS) {
					onWrite(mCallbackAddr, characteristic.getService().getUuid().toString(), characteristic.getUuid().toString());
				}
				else {
					onError(mCallbackAddr, characteristic.getService().getUuid().toString(), characteristic.getUuid().toString(), "Write failed. Error code: " + status);
				}
			}

			@Override
			public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
				onNotification(mCallbackAddr, characteristic.getService().getUuid().toString(), characteristic.getUuid().toString(), characteristic.getValue());
			}
		};
	}

	private UUID clientCharacteristicConfigurationUUID() {
		return BleGoodiesUtils.intToUUID(0x2902);
	}

}
