//
// Core Audio (MMdevice + WASAPI) driver for SonicFX
// Copyright (C) 2009-2010, Ilyes Gouta (ilyes.gouta@gmail.com)
//
// SonicFX is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SonicFX is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with SonicFX.  If not, see <http://www.gnu.org/licenses/>.
//

#include <windows.h>
#include <Mmdeviceapi.h>
#include <stdio.h>
#include <assert.h>

#include <AudioDriver.h>
#include <NiDebug.h>

class __declspec(dllexport) CoreAudioDriver : public AudioDriver {
public:
    CoreAudioDriver();
    ~CoreAudioDriver();

    BOOL Open(LPAUDIODRIVERCAPS lpCaps);
    void Close(LPAUDIODRIVERCAPS lpCaps);

    BOOL Start();
    void Stop();

    void Write(int idx);

    void SetQueueSize(int cPackets);
    float GetPacketLatency();

    const char* GetDescription();

private:

};

CoreAudioDriver::CoreAudioDriver()
{
}

CoreAudioDriver::~CoreAudioDriver()
{
}

BOOL CoreAudioDriver::Open(LPAUDIODRIVERCAPS lpCaps)
{
    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDeviceCollection *pRenderDevices = NULL, *pCaptureDevices = NULL;
    unsigned int nCaptureDevices, nRenderDevices;

    BOOL ret = TRUE;

    HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);

    if (hr != S_OK) {
        NiDebug("CoCreateInstance failed!");
        ret = FALSE; goto fini;
    }

    hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCaptureDevices);
    hr |= pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pRenderDevices);

    if (hr != S_OK) {
        NiDebug("Failed enumerating capture/render devices! error: 0x%08x", hr);
        ret = FALSE; goto fini;
    }

    pCaptureDevices->GetCount(&nCaptureDevices);
    pRenderDevices->GetCount(&nRenderDevices);

    if (!nCaptureDevices || !nRenderDevices) {
        NiDebug("Empty capture/render devices sets!");
        ret = FALSE; goto fini;
    }

    if ((lpCaps->iPreferredCaptureDevice >= nCaptureDevices) ||
        (lpCaps->iPreferredRenderDevice >= nRenderDevices))
    {
        NiDebug("Mismatching preferred capture/render device!");
        ret = FALSE; goto fini;
    }

fini:
    if (pEnumerator)
        pEnumerator->Release();

    if (pCaptureDevices)
        pCaptureDevices->Release();

    if (pRenderDevices)
        pRenderDevices->Release();

    return ret;
}

void CoreAudioDriver::Close(LPAUDIODRIVERCAPS lpCaps)
{
}

BOOL CoreAudioDriver::Start()
{
    return TRUE;
}

void CoreAudioDriver::Stop()
{
}

void CoreAudioDriver::Write(int idx)
{
}

float CoreAudioDriver::GetPacketLatency()
{
    return 0;
}

void CoreAudioDriver::SetQueueSize(int cPackets)
{
}

const char* CoreAudioDriver::GetDescription()
{
    return "CoreAudio Driver v0.1";
}

extern "C" __declspec(dllexport) void GetAudioDriverFactory(LPAUDIODRIVER *lpAudioDriver)
{
    *lpAudioDriver = new CoreAudioDriver();
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}
