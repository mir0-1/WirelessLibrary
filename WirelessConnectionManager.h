#pragma once
#include <glib.h>
#include <NetworkManager.h>
#include <string>

#define CALLBACK_PARAMS_TEMPLATE GObject* srcObject, GAsyncResult* result, gpointer asyncTransferUnitPtr

class WirelessConnectionManager;

typedef struct
{
	WirelessConnectionManager* thisObj;
	void *extraData;
} AsyncTransferUnit;

class WirelessConnectionManager
{
	private:
		std::string ssid;
		std::string password;
		GBytes* ssidGBytes;
		NMClient* client;
		NMDevice* device;
		GMainContext* gMainContext;
		GMainLoop* gMainLoop;
		GThread* gLoopThread;
		GMutex gMutex;
		GCond gCond;
		AsyncTransferUnit asyncTransferUnit;
		bool lastAsyncState;
		
		static gpointer gLoopThreadFunc(gpointer thisObjData);
		static void clientReadyCallback(CALLBACK_PARAMS_TEMPLATE);
		static void connectivityCheckReadyCallback(CALLBACK_PARAMS_TEMPLATE);
		static void remoteConnectionSecretsReadyCallback(CALLBACK_PARAMS_TEMPLATE);
		static void scanReadyCallback(CALLBACK_PARAMS_TEMPLATE);
		
		void waitForAsync();
		void signalAsyncReady();
		NMDeviceWifi* initWifiDevice();
		bool hasInternetAccess();
		NMConnection* tryFindConnectionFromAP();
		NMConnection* newConnectionFromAP();
		void initConnection();
		gchar* getConnectionPassword(NMRemoteConnection* connection);
		NMAccessPoint* findAccessPoint();
		void setSSID(const std::string& ssid);
		void setPassword(const std::string& password);
		
	public:
		WirelessConnectionManager(const std::string& ssid, const std::string& password);
		~WirelessConnectionManager();
};
