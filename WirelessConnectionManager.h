#pragma once
#include <glib.h>
#include <NetworkManager.h>
#include <string>

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
		static void readyInitClientCallback(GObject* srcObject, GAsyncResult* result, gpointer thisObjData);
		static void connectivityCheckReadyCallback(GObject* srcObject, GAsyncResult* result, gpointer asyncTransferUnitPtr);
		static void remoteConnectionSecretsReadyCallback(GObject* srcObject, GAsyncResult* result, gpointer asyncTransferUnitPtr);
		
		void waitForAsync();
		void signalAsyncReady();
		void initWifiDevice();
		bool hasInternetAccess();
		bool tryFindConnectionFromSSID();
		void initConnection();
		const gchar* getConnectionPassword(NMRemoteConnection* connection);
		NMAccessPoint* findAccessPoint();
		NMConnection* findOrMakeNewConnection();
		void setSSID(const std::string& ssid);
		void setPassword(const std::string& password);
		
	public:
		WirelessConnectionManager(const std::string& ssid, const std::string& password);
		~WirelessConnectionManager();
};
