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

// TODO: We should call into the Multimedia Class Scheduler Service to
// temporarly boost the thread priority of the driver. This would allow
// for a lower capture/render latencies when the device is used
// in AUDCLNT_SHAREMODE_EXCLUSIVE mode.

#include <windows.h>
#include <Mmdeviceapi.h>
#include <Audioclient.h>
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
    IMMDevice *m_pCaptureDevice, *m_pRenderDevice;
    LPWSTR m_CaptureId, m_RenderId; // WARN: the application has to free these strings

    // NOTE: one device for capturing but can be another one for rendering.
    IAudioClient *m_piCaptureAudioClient, *m_piRenderAudioClient;

    IAudioCaptureClient *m_piCaptureClient;
    IAudioRenderClient *m_piRenderClient;
};

CoreAudioDriver::CoreAudioDriver()
{
    m_pCaptureDevice = m_pRenderDevice = NULL;
    m_piCaptureAudioClient = m_piCaptureAudioClient = NULL;
    m_piCaptureClient = NULL;
    m_piRenderClient = NULL;
    m_CaptureId = m_RenderId = NULL;
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

    // Only retrieve connected/active devices
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

    hr = pCaptureDevices->Item(lpCaps->iPreferredCaptureDevice, &m_pCaptureDevice);
    hr |= pRenderDevices->Item(lpCaps->iPreferredRenderDevice, &m_pRenderDevice);

    if (hr != S_OK) {
        NiDebug("Failed to retrieve a capture/render device! error: 0x%08x", hr);
        ret = FALSE; goto fini;
    }

    if (!m_pCaptureDevice || !m_pRenderDevice) {
        NiDebug("Bad capture/render device instance!");
        ret = FALSE; goto fini;
    }

    m_pCaptureDevice->GetId(&m_CaptureId);
    m_pRenderDevice->GetId(&m_RenderId);

    NiDebug("Capture device: %s", m_CaptureId);
    NiDebug("Render device: %s", m_RenderId);

    hr = m_pCaptureDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_piCaptureAudioClient);
    hr |= m_pRenderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_piRenderAudioClient);

    if (hr != S_OK) {
        NiDebug("Failed activating capture/render audio clients! error: 0x%08x", hr);
        ret = FALSE; goto fini;
    }

    if (!m_piCaptureAudioClient || !m_piRenderAudioClient) {
        NiDebug("Failed instantiating capture/render audio clients! error: 0x%08x", hr);
        ret = FALSE; goto fini;
    }

    // We have both clients, now get the real work done.

    // TODO: set the capture and render WAVEFORMATEX, buffer size and notification handlers

    // Once configured, get the capture and render services from the clients and start pumping data
    hr = m_piCaptureAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_piCaptureClient);
    hr |= m_piRenderAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_piRenderClient);

    if (hr != S_OK) {
        NiDebug("Failed activating capture/render audio services! error: 0x%08x", hr);
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
    if (m_pRenderDevice)
        m_pRenderDevice->Release();

    if (m_pCaptureDevice)
        m_pCaptureDevice->Release();

    if (m_piCaptureAudioClient)
        m_piCaptureAudioClient->Release();

    if (m_piRenderAudioClient)
        m_piCaptureAudioClient->Release();

    if (m_piCaptureClient)
        m_piCaptureClient->Release();

    if (m_piRenderClient)
        m_piRenderClient->Release();

    m_pRenderDevice = NULL;
    m_pCaptureDevice = NULL;
    m_piCaptureAudioClient = NULL;
    m_piRenderAudioClient = NULL;
    m_piCaptureClient = NULL;
    m_piRenderClient = NULL;
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
