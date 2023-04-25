#include "WirelessConnectionManager.h"
#include <iostream> // Remove this later?

#define KEY_MGMT_WPA_PSK "wpa-psk"
#define KEY_MGMT_WPA_EAP "wpa-eap"
#define KEY_MGMT_WPA_ADHOC "wpa-none"
#define KEY_MGMT_WPA_IEE8021X "ieee8021x"

#define PROP_PSK "psk"
#define CONTINUE_IF(condition, log_message) if (condition) {logger << log_message << std::endl; continue;}
#define RETURN_IF(condition, log_message) if (condition) {logger << log_message << std::endl; return;}
#define RETURN_VAL_IF(condition, log_message, val) if (condition) {logger << log_message << std::endl; return val;}

gpointer WirelessConnectionManager::gLoopThreadFunc(gpointer thisObjData)
{
	WirelessConnectionManager* thisObj = (WirelessConnectionManager*)thisObjData;
	thisObj->gMainLoop = g_main_loop_new(thisObj->gMainContext, false);
	g_main_loop_run(thisObj->gMainLoop);
	return NULL;
}

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
	nm_client_check_connectivity_async(client, NULL, connectivityCheckReadyCallback, (gpointer)&asyncTransferUnit);
	waitForAsync();
	return asyncTransferUnit.extraData;
}

void WirelessConnectionManager::connectivityCheckReadyCallback(CALLBACK_PARAMS_TEMPLATE)
{
	AsyncTransferUnit* asyncTransferUnit = (AsyncTransferUnit*) asyncTransferUnitPtr;
	NMConnectivityState connectivityState = nm_client_check_connectivity_finish(NM_CLIENT(srcObject), result, NULL);
	asyncTransferUnit->extraData = (void*)(connectivityState == NM_CONNECTIVITY_FULL);
	asyncTransferUnit->thisObj->signalAsyncReady();
}

void WirelessConnectionManager::initConnection()
{
	bool tryHotspot = false;
	
	RETURN_IF(hasInternetAccess(), "Network connection is already present");
	
	NMDeviceWifi* device = initWifiDevice();
	RETURN_IF(device == NULL, "Device was NULL");
	NMAccessPoint* accessPoint = findAccessPointBySSID(device);
	NMConnection* connection;
	for(;;tryHotspot = true)
	{
		if (tryHotspot != true)
		{
			CONTINUE_IF(accessPoint == NULL, "Access point not present");
			
			connection = tryFindConnectionFromAP(accessPoint);
			if (connection != NULL)
			{
				logger << "Found connection from AP" << std::endl;
				if (activateAndOrAddConnection(connection, device, accessPoint, false))
					logger << "Existing connection activated" << std::endl;
				else
					logger << "Timeout when trying to activate existing connection" << std::endl;
				return;
			}
			logger << "Could not find suitable existing connection" << std::endl;
			
			CONTINUE_IF(!isAccessPointWPA(accessPoint), "Access point not WPA");
		}
		
		connection = newConnection(device);
		if (connection != NULL)
		{
			if (activateAndOrAddConnection(connection, device, accessPoint, true))
			{
				logger << "new connection added" << std::endl;
				return;
			}
			else
				logger << "timeout on newly added connection activation" << std::endl;
				
		}
	}
}

bool WirelessConnectionManager::activateAndOrAddConnection(NMConnection* connection, NMDeviceWifi* device, NMAccessPoint* accessPoint, bool add)
{
	const char* apPath = nm_object_get_path(NM_OBJECT(accessPoint));
	asyncTransferUnit.extraData = (void*)add;
	
	if (!add)
		nm_client_activate_connection_async(client, connection, NM_DEVICE(device), apPath, NULL, connectionActivateStartedCallback, (gpointer)&asyncTransferUnit);
	else
		nm_client_add_and_activate_connection_async(client, connection, NM_DEVICE(device), apPath, NULL, connectionActivateStartedCallback, (gpointer)&asyncTransferUnit);
	
	waitForAsync();
	if (asyncTransferUnit.extraData == NULL)
		return false;
	NMActiveConnection* activatingConnection = NM_ACTIVE_CONNECTION(asyncTransferUnit.extraData);
	
	if (nm_active_connection_get_state(activatingConnection) == NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
		return true;
	
	NMActiveConnectionState connectionState = (*(NMActiveConnectionState*)asyncTransferUnit.extraData);
	gulong signalHandlerId = g_signal_connect(activatingConnection, "notify::" NM_ACTIVE_CONNECTION_STATE, G_CALLBACK(connectionActivateReadyCallback), (gpointer)&asyncTransferUnit);
	timeout = 300 * G_USEC_PER_SEC;
	//GSource* gTimeoutSource = g_timeout_source_new(timeout);
	//g_source_set_callback(gTimeoutSource, connectionActivateTimeoutCallback, (gpointer)&asyncTransferUnit, NULL);
	//g_source_attach(gTimeoutSource, gMainContext);
	waitForAsync();
	g_signal_handler_disconnect(activatingConnection, signalHandlerId);
	
	bool successful = (connectionState == NM_ACTIVE_CONNECTION_STATE_ACTIVATED);
	
	//g_source_destroy(gTimeoutSource);
	//g_source_unref(gTimeoutSource);
	
	return successful;
}

void WirelessConnectionManager::connectionActivateStartedCallback(CALLBACK_PARAMS_TEMPLATE)
{
	AsyncTransferUnit* asyncTransferUnit = (AsyncTransferUnit*) asyncTransferUnitPtr;
	bool add = (bool)asyncTransferUnit->extraData;
	NMActiveConnection* connResult;
	if (!add)
		connResult = nm_client_activate_connection_finish(NM_CLIENT(srcObject), result, NULL);
	else
		connResult = nm_client_add_and_activate_connection_finish(NM_CLIENT(srcObject), result, NULL);
	asyncTransferUnitPtr->thisObj->logger << "connResult after immediate activation:" << connResult << std::endl;
	asyncTransferUnit->extraData = (void*)connResult;
	asyncTransferUnit->thisObj->signalAsyncReady();
}

void WirelessConnectionManager::connectionActivateReadyCallback(NMActiveConnection* connection, GParamSpec* paramSpec, gpointer asyncTransferUnitPtr)
{
	AsyncTransferUnit* asyncTransferUnit = (AsyncTransferUnit*) asyncTransferUnitPtr;
	NMActiveConnectionState state = nm_active_connection_get_state(connection);
	if (state == NM_ACTIVE_CONNECTION_STATE_ACTIVATED)
		asyncTransferUnit->thisObj->signalAsyncReady();
}

gboolean WirelessConnectionManager::connectionActivateTimeoutCallback(gpointer asyncTransferUnitPtr)
{
	AsyncTransferUnit* asyncTransferUnit = (AsyncTransferUnit*) asyncTransferUnitPtr;
	asyncTransferUnit->thisObj->signalAsyncReady();
	return G_SOURCE_CONTINUE;
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

NMConnection* WirelessConnectionManager::newConnection(NMDeviceWifi* device)
{	
	NMConnection* connection = NM_CONNECTION(nm_simple_connection_new());
	
	NMSettingWireless* settingWireless = NM_SETTING_WIRELESS(nm_setting_wireless_new());
	NMSettingWirelessSecurity* settingWirelessSecurity = NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());
	
	g_object_set(G_OBJECT(settingWireless), NM_SETTING_WIRELESS_SSID, ssidGBytes, NULL);
	g_object_set(G_OBJECT(settingWirelessSecurity), 
				NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, PROP_PSK, 
				NM_SETTING_WIRELESS_SECURITY_PSK, password.c_str(), 
				NULL);
	
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

NMConnection* WirelessConnectionManager::tryFindConnectionFromAP(NMAccessPoint* accessPoint)
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
	asyncTransferUnit.thisObj = this;
	setSSID(ssid);
	setPassword(password);
	g_mutex_init(&gMutex);
	g_cond_init(&gCond);
	gMainContext = g_main_context_get_thread_default();
	gLoopThread = g_thread_new(NULL, gLoopThreadFunc, (gpointer)this);
	nm_client_new_async(NULL, clientReadyCallback, (gpointer)&asyncTransferUnit);
	waitForAsync();
	initConnection();
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
	AsyncTransferUnit* asyncTransferUnit = (AsyncTransferUnit*) asyncTransferUnitPtr;
	asyncTransferUnit->thisObj->client = nm_client_new_finish(result, NULL);
	asyncTransferUnit->thisObj->signalAsyncReady();
}

void WirelessConnectionManager::waitForAsync()
{
	g_mutex_lock(&gMutex);
	lastAsyncState = false;
	while (!lastAsyncState)
		g_cond_wait(&gCond, &gMutex);
	g_mutex_unlock(&gMutex);
}

void WirelessConnectionManager::signalAsyncReady()
{
	g_mutex_lock(&gMutex);
	lastAsyncState = true;
	g_cond_signal(&gCond);
	g_mutex_unlock(&gMutex);
}

WirelessConnectionManager::~WirelessConnectionManager()
{
	g_main_loop_quit(gMainLoop);
	g_thread_join(gLoopThread);
	g_main_loop_unref(gMainLoop);
	g_mutex_clear(&gMutex);
	g_cond_clear(&gCond);
	g_bytes_unref(ssidGBytes);
}