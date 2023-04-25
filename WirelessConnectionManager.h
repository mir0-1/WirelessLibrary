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
		std::ostream& logger;
		GBytes* ssidGBytes = NULL;
		NMClient* client = NULL;
		GMainContext* gMainContext = NULL;
		GMainLoop* gMainLoop = NULL;
		GThread* gLoopThread = NULL;
		GMutex gMutex;
		GCond gCond;
		AsyncTransferUnit asyncTransferUnit;
		guint timeout = 30;
		bool lastAsyncState;
		
		static gpointer gLoopThreadFunc(gpointer thisObjData);
		static void clientReadyCallback(CALLBACK_PARAMS_TEMPLATE);
		static void connectivityCheckReadyCallback(CALLBACK_PARAMS_TEMPLATE);
		static void connectionActivateStartedCallback(CALLBACK_PARAMS_TEMPLATE);
		static void connectionActivateReadyCallback(NMActiveConnection* connection, GParamSpec* paramSpec, gpointer asyncTransferUnitPtr);
		
		void waitForAsync();
		void signalAsyncReady();
		NMDeviceWifi* initWifiDevice();
		bool hasInternetAccess();
		NMAccessPoint* findAccessPointBySSID(NMDeviceWifi* device);
		bool activateAndOrAddConnection(NMConnection* connection, NMDeviceWifi* device, NMAccessPoint* accessPoint, bool add);
		bool isAccessPointWPA(NMAccessPoint* accessPoint);
		NMConnection* tryFindExternalConnection(NMAccessPoint* accessPoint);
		NMConnection* newExternalConnection(NMDeviceWifi* device);
		void initExternalConnection();
		NMAccessPoint* findAccessPoint();
		void setSSID(const std::string& ssid);
		void setPassword(const std::string& password);
		
	public:
		WirelessConnectionManager(const std::string& ssid, const std::string& password);
		~WirelessConnectionManager();
};
