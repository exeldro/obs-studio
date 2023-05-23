#include "system-info.h"

#include <dxgi.h>
#include <cinttypes>

#include <util/platform.h>
#include <util/windows/ComPtr.hpp>

OBSDataArrayAutoRelease system_gpu_data()
{
	ComPtr<IDXGIFactory1> factory;
	ComPtr<IDXGIAdapter1> adapter;
	HRESULT hr;
	UINT i;

	hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
	if (FAILED(hr))
		return nullptr;

	OBSDataArrayAutoRelease adapter_info = obs_data_array_create();
	for (i = 0; factory->EnumAdapters1(i, adapter.Assign()) == S_OK; ++i) {
		DXGI_ADAPTER_DESC desc;
		char name[512] = "";
		char driver_version[512] = "";

		hr = adapter->GetDesc(&desc);
		if (FAILED(hr))
			continue;

		/* ignore Microsoft's 'basic' renderer' */
		if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c)
			continue;

		os_wcs_to_utf8(desc.Description, 0, name, sizeof(name));

		obs_data_t *data = obs_data_create();
		obs_data_set_string(data, "model", name);

		/* driver version */
		LARGE_INTEGER umd;
		hr = adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice),
						    &umd);
		if (SUCCEEDED(hr)) {
			const uint64_t version = umd.QuadPart;
			const uint16_t aa = (version >> 48) & 0xffff;
			const uint16_t bb = (version >> 32) & 0xffff;
			const uint16_t ccccc = (version >> 16) & 0xffff;
			const uint16_t ddddd = version & 0xffff;
			sprintf(driver_version,
				"%" PRIu16 ".%" PRIu16 ".%" PRIu16 ".%" PRIu16,
				aa, bb, ccccc, ddddd);
			obs_data_set_string(data, "driver_version",
					    driver_version);
		}

		obs_data_array_push_back(adapter_info, data);
	}

	return adapter_info;
}
