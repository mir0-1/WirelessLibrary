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
		GMainContext* gMainContext;
		GMainLoop* gMainLoop;
		GThread* gLoopThread;
		GMutex gMutex;
		GCond gCond;
		AsyncTransferUnit asyncTransferUnit;
		bool lastAsyncState;
		
		static gpointer gLoopThreadFunc(gpointer thisObjData);
		static gpointer activateConnectionTimeoutThreadFunc(gpointer asyncTransferUnitPtr);
		static void clientReadyCallback(CALLBACK_PARAMS_TEMPLATE);
		static void connectivityCheckReadyCallback(CALLBACK_PARAMS_TEMPLATE);
		static void connectionActivateStartedCallback(CALLBACK_PARAMS_TEMPLATE);
		static void connectionActivateReadyCallback(NMActiveConnection* connection, GParamSpec* paramSpec, gpointer asyncTransferUnitPtr);
		
		void waitForAsync();
		void signalAsyncReady();
		NMDeviceWifi* initWifiDevice();
		bool hasInternetAccess();
		NMAccessPoint* findAccessPointBySSID(NMDeviceWifi* device);
		void activateAndOrAddConnection(NMConnection* connection, NMDeviceWifi* device, NMAccessPoint* accessPoint, bool add);
		bool isAccessPointWPA(NMAccessPoint* accessPoint);
		NMConnection* tryFindConnectionFromAP(NMAccessPoint* accessPoint);
		NMConnection* newConnectionFromAP(NMAccessPoint* accessPoint, NMDeviceWifi* device);
		void initConnection();
		NMAccessPoint* findAccessPoint();
		void setSSID(const std::string& ssid);
		void setPassword(const std::string& password);
		
	public:
		WirelessConnectionManager(const std::string& ssid, const std::string& password);
		~WirelessConnectionManager();
};
