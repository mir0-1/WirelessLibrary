#include "WirelessConnectionManager.h"
#include <iostream> // Remove this later?

#define KEY_MGMT_WPA_PSK "wpa-psk"
#define KEY_MGMT_WPA_EAP "wpa-eap"
#define KEY_MGMT_WPA_ADHOC "wpa-none"
#define KEY_MGMT_WPA_IEE8021X "ieee8021x"

#define PROP_PSK "psk"
#define CONTINUE_IF(cond) if (cond) continue
#define RETURN_VOID_IF(cond) if (cond) return

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

void WirelessConnectionManager::remoteConnectionSecretsReadyCallback(CALLBACK_PARAMS_TEMPLATE)
{
	AsyncTransferUnit* asyncTransferUnit = (AsyncTransferUnit*) asyncTransferUnitPtr;
	*((GVariant**)asyncTransferUnit->extraData) = nm_remote_connection_get_secrets_finish(NM_REMOTE_CONNECTION(srcObject), result, NULL);
	asyncTransferUnit->thisObj->signalAsyncReady();
}

void WirelessConnectionManager::initConnection()
{
	//if (hasInternetAccess())
		//return;
	
	std::cout << "random debug message" << std::endl;
	NMDeviceWifi* device = initWifiDevice();
	NMAccessPoint* accessPoint = findAccessPointBySSID(device);
	if (accessPoint == NULL)
	{
		std::cout << "AP not present" << std::endl;
		return;
	}
	
	NMConnection* connection = tryFindConnectionFromAP(accessPoint);
	if (connection != NULL)
	{
		std::cout << "connection match" << std::endl;
		return;
	}
	
	if (!isAccessPointWPA(accessPoint))
	{
		std::cout << "AP not WPA" << std::endl;
		return;
	}
	std::cout << "before new conn" << std::endl;
	connection = newConnectionFromAP(accessPoint);
	if (connection != NULL)
	{
		std::cout << "new conn from AP!!" << std::endl;
	}
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

NMConnection* WirelessConnectionManager::newConnectionFromAP(NMAccessPoint* accessPoint)
{	
	std::cout << "creating conns" << std::endl;
	NMConnection* connection = NM_CONNECTION(nm_simple_connection_new());
	NMSettingWireless* settingWireless = NM_SETTING_WIRELESS(nm_setting_wireless_new());
	NMSettingWirelessSecurity* settingWirelessSecurity = NM_SETTING_WIRELESS_SECURITY(nm_setting_wireless_security_new());
	
	std::cout << "g_obj_sets" << std::endl;
	g_object_set(G_OBJECT(settingWireless), NM_SETTING_WIRELESS_SSID, ssidGBytes, NULL);
	g_object_set(G_OBJECT(settingWirelessSecurity), NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, PROP_PSK, NM_SETTING_WIRELESS_SECURITY_PSK, password.c_str(), NULL);
	
	std::cout << "before add setting" << std::endl;
	nm_connection_add_setting(connection, NM_SETTING(settingWireless));
	nm_connection_add_setting(connection, NM_SETTING(settingWirelessSecurity));
	
	std::cout << "before activation" << std::endl;
	nm_client_add_and_activate_connection_async(client, connection, device, (const char*)accessPoint, NULL, connectionAddAndActivateReadyCallback, (gpointer)&asyncTransferUnit);
	waitForAsync();
	bool successful = (bool)asyncTransferUnit.extraData;
	
	if (successful)
	{
		std::cout << "add and activate new connection" << std::endl;
	}
}

void WirelessConnectionManager::connectionAddAndActivateReadyCallback(CALLBACK_PARAMS_TEMPLATE)
{
	AsyncTransferUnit* asyncTransferUnit = (AsyncTransferUnit*) asyncTransferUnitPtr;
	NMActiveConnection* connResult = nm_client_add_and_activate_connection_finish(NM_CLIENT(srcObject), result, NULL);
	asyncTransferUnit->extraData = (void*)(connResult != NULL);
	asyncTransferUnit->thisObj->signalAsyncReady();
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
	NMDeviceWifi* device = initWifiDevice();
	const GPtrArray* connections = nm_client_get_connections(client);
	
	for (int i = 0; i < connections->len; i++)
	{
		NMConnection* currentConnection = NM_CONNECTION(connections->pdata[i]);
		if (nm_access_point_connection_valid(accessPoint, currentConnection))
			return currentConnection;
	}
	
	return NULL;
}

gchar* WirelessConnectionManager::getConnectionPassword(NMRemoteConnection* connection)
{
	GVariant* root = NULL;
	asyncTransferUnit.extraData = (void*)&root;
	nm_remote_connection_get_secrets_async(NM_REMOTE_CONNECTION(connection), NM_SETTING_WIRELESS_SECURITY_SETTING_NAME, NULL, remoteConnectionSecretsReadyCallback, (gpointer)&asyncTransferUnit);
	waitForAsync();

	GVariant* wirelessSecurity = g_variant_lookup_value(root, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME, G_VARIANT_TYPE_DICTIONARY);
	if (wirelessSecurity == NULL)
	{
		g_variant_unref(root);
		return NULL;
	}

	GVariant* passwordGVariant = g_variant_lookup_value(wirelessSecurity, PROP_PSK, G_VARIANT_TYPE_STRING);
	if (passwordGVariant == NULL)
	{
		g_variant_unref(wirelessSecurity);
		g_variant_unref(root);
		return NULL;
	}

	gchar* passwordStr = g_variant_dup_string(passwordGVariant, NULL);
	
	if (!g_strcmp0(passwordStr, ""))
	{
		g_free(passwordStr);
		passwordStr = NULL;
	}

	g_variant_unref(passwordGVariant);
	g_variant_unref(wirelessSecurity);
	g_variant_unref(root);
	return passwordStr;
}

WirelessConnectionManager::WirelessConnectionManager(const std::string& ssid, const std::string& password)
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