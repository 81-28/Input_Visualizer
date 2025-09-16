#include "SerialUtils.h"
#include <Windows.h>
#include <SetupAPI.h>
#include <devguid.h>
#include <RegStr.h>

std::vector<SerialUtils::SerialPortInfo> SerialUtils::AvailablePorts() {
	std::vector<SerialPortInfo> ports;

	// シリアルポートの情報を取得
	HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
		&GUID_DEVCLASS_PORTS,
		NULL,
		NULL,
		DIGCF_PRESENT
	);


	if (deviceInfoSet == INVALID_HANDLE_VALUE) {
		return ports;
	}

	// デバイス情報を列挙
	SP_DEVINFO_DATA deviceInfoData;
	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); ++i) {
		char portName[256];
		char description[256];
		// ポート名の取得
		HKEY hKey = SetupDiOpenDevRegKey(
			deviceInfoSet,
			&deviceInfoData,
			DICS_FLAG_GLOBAL,
			0,
			DIREG_DEV,
			KEY_READ
		);
		if (hKey != INVALID_HANDLE_VALUE) {
			DWORD portNameSize = sizeof(portName);
			DWORD type;
			if (RegQueryValueExA(hKey, "PortName", NULL, &type, (LPBYTE)portName, &portNameSize) == ERROR_SUCCESS && type == REG_SZ) {
				// 説明の取得
				if (SetupDiGetDeviceRegistryPropertyA(
					deviceInfoSet,
					&deviceInfoData,
					SPDRP_FRIENDLYNAME,
					NULL,
					(PBYTE)description,
					sizeof(description),
					NULL
				)) {
					SerialPortInfo info;
					info.port = portName;
					info.description = description;
					ports.push_back(info);
				}
			}
			RegCloseKey(hKey);
		}
	}
	SetupDiDestroyDeviceInfoList(deviceInfoSet);

	return ports;
}
