#include "WirelessConnectionManager.h"
#include <iostream> // Remove this later?

#define KEY_MGMT_WPA_PSK "wpa-psk"
#define KEY_MGMT_WPA_EAP "wpa-eap"
#define KEY_MGMT_WPA_IEE8021X "ieee8021x"
#define PSK "psk"
#define MODE_AP "ap"
#define BAND_BG "bg"
#define PROTO_RSN "rsn"
#define PAIRWISE_CCMP "ccmp"
#define GROUP_CCMP "ccmp"
#define METHOD_SHARED "shared"

NMDeviceWifi* WirelessConnectionManager::initWifiDevice()
{
	const GPtrArray* devices = nm_client_get_devices(client);
	
	for (int i = 0; i < devices->len; i++)
	{
		if (NM_IS_DEVICE_WIFI(devices->pdata[i]))
			return NM_DEVICE_WIFI(devices->pdata[i]);
	}
	
	return NULL;
}

bool WirelessConnectionManager::hasInternetAccess()
{
	nm_client_check_connectivity_async(client, NULL, connectivityCheckReadyCallback, (gpointer)&eventMgr);
	eventMgr.waitForAsync();
	return (bool)eventMgr.getEventData();
}

void WirelessConnectionManager::connectivityCheckReadyCallback(CALLBACK_PARAMS_TEMPLATE)
{
	EventManager* eventMgr = (EventManager*) eventMgrPtr;
	NMConnectivityState connectivityState = nm_client_check_connectivity_finish(NM_CLIENT(srcObject), result, NULL);
	eventMgr->setEventData((void*)(connectivityState == NM_CONNECTIVITY_FULL));
	eventMgr->signalAsyncReady();
}

bool WirelessConnectionManager::initExternalConnection(NMDeviceWifi* device)
{
	if (hasInternetAccess())
	{
		logger << "Connection with Internet access already active" << std::endl;
		return true;
	}
	
	if (device == NULL)
	{
		logger << "Device is NULL" << std::endl;
		return false;
	}
	
	NMAccessPoint* accessPoint = findAccessPointBySSID(device);
	NMConnection* connection;
	
	if (accessPoint == NULL)
		logger << "Access point not present" << std::endl;
	if (!isAccessPointWPA(accessPoint))
	{
		accessPoint = NULL;
		logger << "Access point not WPA" << std::endl;
		return false;
	}

	connection = tryFindExternalConnection(accessPoint);
	if (connection != NULL)
	{
		logger << "Found connection from AP" << std::endl;
		if (activateAndOrAddConnection(connection, device, accessPoint, false))
		{
			logger << "Existing connection activated" << std::endl;
			return true;
		}
		logger << "Failed trying to activate existing connection" << std::endl;
	}
	logger << "Could not find suitable existing connection" << std::endl;
	
	connection = newConnection(device, false);
	if (connection == NULL)
	{
		logger << "Failed added connection activation" << std::endl;
		return false;
	}
	if (accessPoint != NULL && activateAndOrAddConnection(connection, device, accessPoint, true))
	{
		logger << "New connection added and activated" << std::endl;
		return true;
	}
	logger << "Could not activate connection" << std::endl;
	return false;
}

bool WirelessConnectionManager::initSelfHotspot(NMDeviceWifi* device)
{
	logger << "Trying to find existing self hotspot config" << std::endl;
	NMConnection* connection = tryFindHotspotConnection();
	if (connection != NULL)
	{
		if (activateAndOrAddConnection(connection, device, NULL, false))
		{
			logger << "Successful existing hotspot activation" << std::endl;
			return true;
		}
		logger << "Existing hotspot activation failed" << std::endl;
	}
	logger << "Trying to create a new hotspot" << std::endl;
	connection = newConnection(device, true);
	if (connection != NULL)
	{
		if (activateAndOrAddConnection(connection, device, NULL, true))
		{
			logger << "Successful new hotspot activation" << std::endl;
			return true;
		}
		logger << "New hotspot activation failure" << std::endl;
	}
	
	logger << "Could not activate hotspot connection" << std::endl;
	return false;
}

NMConnection* WirelessConnectionManager::tryFindHotspotConnection()
{
	const GPtrArray* connections = nm_client_get_connections(client);
	
	for (int i = 0; i < connections->len; i++)
	{
		NMConnection* currentConnection = NM_CONNECTION(connections->pdata[i]);
		
		if (!nm_connection_is_type(currentConnection, NM_SETTING_WIRELESS_SETTING_NAME))
			continue;
		
		NMSettingWireless* settingWireless = nm_connection_get_setting_wireless(currentConnection);
		if (settingWireless == NULL)
			continue;
		
		if (g_strcmp0(nm_setting_wireless_get_mode(settingWireless), MODE_AP))
			continue;
		
		if (g_strcmp0(nm_setting_wireless_get_band(settingWireless), BAND_BG))
			continue;
		
		NMSettingWirelessSecurity* settingWirelessSecurity = nm_connection_get_setting_wireless_security(currentConnection);
		
		if (settingWirelessSecurity == NULL)
			continue;
		
		if (g_strcmp0(nm_setting_wireless_security_get_key_mgmt(settingWirelessSecurity), KEY_MGMT_WPA_PSK))
			continue;
		
		if (!findConnectionProto(settingWirelessSecurity, PROTO_RSN))
			continue;
		
		if (!findConnectionPairwiseEncryption(settingWirelessSecurity, PAIRWISE_CCMP))
			continue;
		
		if (!findConnectionGroupEncryption(settingWirelessSecurity, GROUP_CCMP))
			continue;
		
		NMSettingIPConfig* settingIP = nm_connection_get_setting_ip4_config(currentConnection);
		
		if (settingIP == NULL)
			continue;
		
		if (g_strcmp0(nm_setting_ip_config_get_method(settingIP), METHOD_SHARED))
			continue;
		
		return currentConnection;
	}
	
	return NULL;
}

bool WirelessConnectionManager::findConnectionProto(NMSettingWirelessSecurity* wirelessSecurity, const char* value)
{
	return findConnectionProperty(wirelessSecurity, value, nm_setting_wireless_security_get_num_protos, nm_setting_wireless_security_get_proto);
}

bool WirelessConnectionManager::findConnectionPairwiseEncryption(NMSettingWirelessSecurity* wirelessSecurity, const char* value)
{	
	return findConnectionProperty(wirelessSecurity, value, nm_setting_wireless_security_get_num_pairwise, nm_setting_wireless_security_get_pairwise);
}

bool WirelessConnectionManager::findConnectionGroupEncryption(NMSettingWirelessSecurity* wirelessSecurity, const char* value)
{
	return findConnectionProperty(wirelessSecurity, value, nm_setting_wireless_security_get_num_groups, nm_setting_wireless_security_get_group);
}

bool WirelessConnectionManager::findConnectionProperty(NMSettingWirelessSecurity* wirelessSecurity, const char *value, ConnectionPropertyLengthFunc lengthFunc, ConnectionPropertyIndexFunc indexFunc)
{
	int length = lengthFunc(wirelessSecurity);
	for (int i = 0; i < length; i++)
	{
		if (!g_strcmp0(indexFunc(wirelessSecurity, i), value))
			return true;
	}
	
	return false;
}

bool WirelessConnectionManager::activateAndOrAddConnection(NMConnection* connection, NMDeviceWifi* device, NMAccessPoint* accessPoint, bool add)
{
	const char* apPath = (accessPoint == NULL) ? NULL : nm_object_get_path(NM_OBJECT(accessPoint));
	eventMgr.setEventData((void*)add);
	
	if (!add)
		nm_client_activate_connection_async(client, connection, NM_DEVICE(device), apPath, NULL, connectionActivateStartedCallback, (gpointer)&eventMgr);
	else
		nm_client_add_and_activate_connection_async(client, connection, NM_DEVICE(device), apPath, NULL, connectionActivateStartedCallback, (gpointer)&eventMgr);
	
	eventMgr.waitForAsync();
	void *connActivateResult = eventMgr.getEventData();
	if (connActivateResult == NULL)
	{
		logger << "Connection activation yielded NULL result" << std::endl;
		return false;
	}
	NMActiveConnection* activatingConnection = NM_ACTIVE_CONNECTION(connActivateResult);
	
	NMActiveConnectionState connectionState = nm_active_connection_get_state(activatingConnection);
	if (connectionState == NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
		return true;
	
	gulong id = eventMgr.registerConnection(activatingConnection, G_CALLBACK(connectionActivateReadyCallback));
	eventMgr.waitForAsync();
	eventMgr.unregisterConnection(activatingConnection, &id);
	connectionState = nm_active_connection_get_state(activatingConnection);
	
	if (add && connectionState != NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
	{
		logger << "Connection state expected to be activated; in reality it is not" << std::endl;
		nm_remote_connection_delete_async(nm_active_connection_get_connection(activatingConnection), NULL, connectionDeleteReadyCallback, (gpointer)&eventMgr);
		eventMgr.waitForAsync();
		return false;
	}
	
	return true;
}

void WirelessConnectionManager::connectionDeleteReadyCallback(CALLBACK_PARAMS_TEMPLATE)
{
	EventManager* eventMgr = (EventManager*) eventMgrPtr;
	nm_remote_connection_delete_finish(NM_REMOTE_CONNECTION(srcObject), result, NULL);
	eventMgr->signalAsyncReady();
}

void WirelessConnectionManager::connectionActivateStartedCallback(CALLBACK_PARAMS_TEMPLATE)
{
	EventManager* eventMgr = (EventManager*) eventMgrPtr;
	bool add = (bool)eventMgr->getEventData();
	NMActiveConnection* connResult;
	if (!add)
		connResult = nm_client_activate_connection_finish(NM_CLIENT(srcObject), result, NULL);
	else
		connResult = nm_client_add_and_activate_connection_finish(NM_CLIENT(srcObject), result, NULL);
	eventMgr->setEventData((void*)connResult);
	eventMgr->signalAsyncReady();
}

void WirelessConnectionManager::connectionActivateReadyCallback(NMActiveConnection* connection, GParamSpec* paramSpec, gpointer eventMgrPtr)
{
	EventManager* eventMgr = (EventManager*) eventMgrPtr;
	NMActiveConnectionState state = nm_active_connection_get_state(connection);
	if (state != NM_ACTIVE_CONNECTION_STATE_ACTIVATING)
		eventMgr->signalAsyncReady();
}

bool WirelessConnectionManager::isAccessPointWPA(NMAccessPoint* accessPoint)
{
	NM80211ApSecurityFlags flags = nm_access_point_get_wpa_flags(accessPoint);
	if (flags & NM_802_11_AP_SEC_KEY_MGMT_PSK)
		return true;
	
	flags = nm_access_point_get_rsn_flags(accessPoint);
	if (flags & NM_802_11_AP_SEC_KEY_MGMT_PSK)
		return true;
	
	return false;
}

NMConnection* WirelessConnectionManager::newConnection(NMDeviceWifi* device, bool selfHotspot)
{	
	NMConnection* connection = NM_CONNECTION(nm_simple_connection_new());
	
	NMSettingWireless* settingWireless = NM_SETTING_WIRELESS(nm_setting_wireless_new());
	NMSettingWirelessSecurity* settingWirelessSecurity = NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());
	NMSettingIPConfig* settingIP = NULL;
	
	g_object_set(G_OBJECT(settingWireless), NM_SETTING_WIRELESS_SSID, ssidGBytes, NULL);
	g_object_set(G_OBJECT(settingWirelessSecurity), 
				NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, KEY_MGMT_WPA_PSK, 
				NM_SETTING_WIRELESS_SECURITY_PSK, password.c_str(), 
				NULL);
	
	if (selfHotspot)
	{
		g_object_set(G_OBJECT(settingWireless), NM_SETTING_WIRELESS_MODE, MODE_AP,
												NM_SETTING_WIRELESS_BAND, BAND_BG,
												NULL);
		
		nm_setting_wireless_security_add_proto(settingWirelessSecurity, PROTO_RSN);
		nm_setting_wireless_security_add_pairwise(settingWirelessSecurity, PAIRWISE_CCMP);
		nm_setting_wireless_security_add_group(settingWirelessSecurity, GROUP_CCMP);
		
		settingIP = NM_SETTING_IP_CONFIG(nm_setting_ip4_config_new());
		
		g_object_set(G_OBJECT(settingIP), NM_SETTING_IP_CONFIG_METHOD, METHOD_SHARED, NULL);
		nm_connection_add_setting(connection, NM_SETTING(settingIP));
	}
	
	nm_connection_add_setting(connection, NM_SETTING(settingWireless));
	nm_connection_add_setting(connection, NM_SETTING(settingWirelessSecurity));
	
	return connection;
}

NMAccessPoint* WirelessConnectionManager::findAccessPointBySSID(NMDeviceWifi* device)
{
	const GPtrArray* accessPoints = nm_device_wifi_get_access_points(device);
	
	for (int i = 0; i < accessPoints->len; i++)
	{
		NMAccessPoint* currentAccessPoint = NM_ACCESS_POINT(accessPoints->pdata[i]);
		GBytes* ssidGBytesCandidate = nm_access_point_get_ssid(currentAccessPoint);
		if (ssidGBytesCandidate == NULL)
			continue;
		if (g_bytes_equal(ssidGBytes, ssidGBytesCandidate))
			return currentAccessPoint;
	}
	
	return NULL;
}

NMConnection* WirelessConnectionManager::tryFindExternalConnection(NMAccessPoint* accessPoint)
{
	const GPtrArray* connections = nm_client_get_connections(client);
	
	for (int i = 0; i < connections->len; i++)
	{
		NMConnection* currentConnection = NM_CONNECTION(connections->pdata[i]);
		if (nm_access_point_connection_valid(accessPoint, currentConnection))
			return currentConnection;
	}
	
	return NULL;
}

WirelessConnectionManager::WirelessConnectionManager(const std::string& ssid, const std::string& password)
: logger(std::cout)
{
	setSSID(ssid);
	setPassword(password);
	nm_client_new_async(NULL, clientReadyCallback, (gpointer)&eventMgr);
	eventMgr.waitForAsync();
	client = NM_CLIENT(eventMgr.getEventData());
	NMDeviceWifi* device = initWifiDevice();
	if (!initExternalConnection(device))
		initSelfHotspot(device);
}

void WirelessConnectionManager::setSSID(const std::string& ssid)
{
	this->ssid = ssid;
	ssidGBytes = g_bytes_new(this->ssid.c_str(), this->ssid.size());
}

void WirelessConnectionManager::setPassword(const std::string& password)
{
	this->password = password;
}

void WirelessConnectionManager::clientReadyCallback(CALLBACK_PARAMS_TEMPLATE)
{
	EventManager* eventMgr = (EventManager*) eventMgrPtr;
	eventMgr->setEventData((void*)nm_client_new_finish(result, NULL));
	eventMgr->signalAsyncReady();
}

WirelessConnectionManager::~WirelessConnectionManager()
{
	g_bytes_unref(ssidGBytes);
}
