#pragma once
#include "../EventManager/EventManager.h"
#include <NetworkManager.h>
#include <string>

#define CALLBACK_PARAMS_TEMPLATE GObject* srcObject, GAsyncResult* result, gpointer eventMgrPtr

typedef guint32 (*ConnectionPropertyLengthFunc)(NMSettingWirelessSecurity*);
typedef const char* (*ConnectionPropertyIndexFunc)(NMSettingWirelessSecurity*, guint32);

class WirelessConnectionManager
{
	private:
		std::string ssid;
		std::string password;
		std::ostream& logger;
		GBytes* ssidGBytes;
		NMClient* client;
		EventManager eventMgr;
		
		static void clientReadyCallback(CALLBACK_PARAMS_TEMPLATE);
		static void connectivityCheckReadyCallback(CALLBACK_PARAMS_TEMPLATE);
		static void connectionActivateStartedCallback(CALLBACK_PARAMS_TEMPLATE);
		static void connectionDeleteReadyCallback(CALLBACK_PARAMS_TEMPLATE);
		static void connectionActivateReadyCallback(NMActiveConnection* connection, GParamSpec* paramSpec, gpointer eventMgrPtr);
		
		NMDeviceWifi* initWifiDevice();
		bool hasInternetAccess();
		NMAccessPoint* findAccessPointBySSID(NMDeviceWifi* device);
		bool activateAndOrAddConnection(NMConnection* connection, NMDeviceWifi* device, NMAccessPoint* accessPoint, bool add);
		bool isAccessPointWPA(NMAccessPoint* accessPoint);
		bool findConnectionProto(NMSettingWirelessSecurity* wirelessSecurity, const char* value);
		bool findConnectionPairwiseEncryption(NMSettingWirelessSecurity* wirelessSecurity, const char* value);
		bool findConnectionGroupEncryption(NMSettingWirelessSecurity* wirelessSecurity, const char* value);
		bool findConnectionProperty(NMSettingWirelessSecurity* wirelessSecurity, const char *value, ConnectionPropertyLengthFunc lengthFunc, ConnectionPropertyIndexFunc indexFunc);
		NMConnection* tryFindExternalConnection(NMAccessPoint* accessPoint);
		NMConnection* tryFindHotspotConnection();
		NMConnection* newConnection(NMDeviceWifi* device, bool selfHotspot);
		bool initExternalConnection(NMDeviceWifi* device);
		bool initSelfHotspot(NMDeviceWifi* device);
		NMAccessPoint* findAccessPoint();
		void setSSID(const std::string& ssid);
		void setPassword(const std::string& password);
		
	public:
		WirelessConnectionManager(const std::string& ssid, const std::string& password);
		~WirelessConnectionManager();
};
