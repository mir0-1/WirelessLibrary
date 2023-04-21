#include "WirelessConnectionManager.h"
#include <iostream> // Remove this later?

#define KEY_MGMT_WPA_PSK "wpa-psk"
#define KEY_MGMT_WPA_EAP "wpa-eap"
#define KEY_MGMT_WPA_ADHOC "wpa-none"
#define KEY_MGMT_WPA_IEE8021X "ieee8021x"

#define PROP_PSK "psk"

gpointer WirelessConnectionManager::gLoopThreadFunc(gpointer thisObjData)
{
	WirelessConnectionManager* thisObj = (WirelessConnectionManager*)thisObjData;
	thisObj->gMainLoop = g_main_loop_new(thisObj->gMainContext, false);
	g_main_loop_run(thisObj->gMainLoop);
	return NULL;
}

void WirelessConnectionManager::initWifiDevice()
{
	const GPtrArray* devices = nm_client_get_devices(client);
	
	for (int i = 0; i < devices->len; i++)
	{
		if (NM_IS_DEVICE_WIFI(devices->pdata[i]))
		{
			device = NM_DEVICE(devices->pdata[i]);
			return;
		}
	}
	
	device = NULL;
}

bool WirelessConnectionManager::hasInternetAccess()
{
	bool connected = false;

	asyncTransferUnit.extraData = (void*)&connected;
	nm_client_check_connectivity_async(client, NULL, connectivityCheckReadyCallback, (gpointer)&asyncTransferUnit);
	waitForAsync();
	return connected;
}

void WirelessConnectionManager::connectivityCheckReadyCallback(GObject* srcObject, GAsyncResult* result, gpointer asyncTransferUnitPtr)
{
	AsyncTransferUnit* asyncTransferUnit = (AsyncTransferUnit*) asyncTransferUnitPtr;
	NMConnectivityState connectivityState = nm_client_check_connectivity_finish(asyncTransferUnit->thisObj->client, result, NULL);
	*((bool*)asyncTransferUnit->extraData) = (connectivityState == NM_CONNECTIVITY_FULL);
	asyncTransferUnit->thisObj->signalAsyncReady();
}

void WirelessConnectionManager::remoteConnectionSecretsReadyCallback(GObject* srcObject, GAsyncResult* result, gpointer asyncTransferUnitPtr)
{
	AsyncTransferUnit* asyncTransferUnit = (AsyncTransferUnit*) asyncTransferUnitPtr;
	*((GVariant**)asyncTransferUnit->extraData) = nm_remote_connection_get_secrets_finish(NM_REMOTE_CONNECTION(srcObject), result, NULL);
	asyncTransferUnit->thisObj->signalAsyncReady();
}

void WirelessConnectionManager::initConnection()
{
	//if (hasInternetAccess())
		//return;

	NMConnection* connection = tryFindConnectionFromSSID();
	if (connection != NULL)
	{
		std::cout << "Found Connection for the WiFi!!" << std::endl;
	}

}


const gchar* WirelessConnectionManager::getConnectionPassword(NMRemoteConnection* connection)
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

	const gchar* passwordStr = g_variant_dup_string(passwordGVariant, NULL);
	
	if (!g_strcmp0(passwordStr, ""))
	{
		g_variant_unref(passwordGVariant);
		g_variant_unref(wirelessSecurity);
		g_variant_unref(root);
		return NULL;
	}

	g_variant_unref(passwordGVariant);
	g_variant_unref(wirelessSecurity);
	g_variant_unref(root);
	return passwordStr;
}

NMConnection* WirelessConnectionManager::tryFindConnectionFromSSID()
{
	const GPtrArray* connections = nm_client_get_connections(client);

	for (int i = 0; i < connections->len; i++)
	{
		NMConnection* currentConnection = NM_CONNECTION(connections->pdata[i]);

		if (g_strcmp0(nm_connection_get_connection_type(currentConnection), NM_SETTING_WIRELESS_SETTING_NAME))
			continue;

		NMSettingWireless* settingWireless = nm_connection_get_setting_wireless(currentConnection);
		NMSettingWirelessSecurity* settingWirelessSecurity = nm_connection_get_setting_wireless_security(currentConnection);

		if (settingWireless == NULL || settingWirelessSecurity == NULL)
			continue;

		const char* keyMgmt = nm_setting_wireless_security_get_key_mgmt(settingWirelessSecurity);

		if (g_strcmp0(keyMgmt, KEY_MGMT_WPA_PSK))
			continue;

		GBytes* ssidGBytesCandidate = nm_setting_wireless_get_ssid(settingWireless);

		const gchar* passwordCandidate = getConnectionPassword(NM_REMOTE_CONNECTION(currentConnection));

		if (passwordCandidate == NULL)
			continue;

		if (g_bytes_equal(ssidGBytes, ssidGBytesCandidate) && !g_strcmp0(password.c_str(), passwordCandidate))
			return currentConnection;

	}

	return NULL;
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
	nm_client_new_async(NULL, readyInitClientCallback, (gpointer)this);
	waitForAsync();
	initWifiDevice();
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

void WirelessConnectionManager::readyInitClientCallback(GObject* srcObject, GAsyncResult* result, gpointer thisObjData)
{
	WirelessConnectionManager* thisObj = (WirelessConnectionManager*) thisObjData;
	thisObj->client = nm_client_new_finish(result, NULL);
	thisObj->signalAsyncReady();
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