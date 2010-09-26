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
#include <Avrt.h>

#include <stdio.h>
#include <assert.h>

#include <AudioDriver.h>
#include <NiDebug.h>

#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

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
    LPWSTR m_CaptureId, m_RenderId;

    // NOTE: one device for capturing but can be another one for rendering.
    IAudioClient *m_piCaptureAudioClient, *m_piRenderAudioClient;

    IAudioCaptureClient *m_piCaptureClient;
    IAudioRenderClient *m_piRenderClient;

    HANDLE m_hCaptureEvent, m_hRenderEvent;

    GUID m_SessionGUID;
    HANDLE hTask;
};

CoreAudioDriver::CoreAudioDriver()
{
    m_pCaptureDevice = NULL;
    m_pRenderDevice = NULL;

    m_piCaptureAudioClient = NULL;
    m_piCaptureAudioClient = NULL;

    m_piCaptureClient = NULL;
    m_piRenderClient = NULL;

    m_hCaptureEvent = NULL;
    m_hRenderEvent = NULL;

    m_CaptureId = NULL;
    m_RenderId = NULL;
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

    WAVEFORMATEX wavefmtex;

    BOOL ret = TRUE;

    HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);

    if (hr != S_OK) {
        NiDebug("CoCreateInstance failed!");
        ret = FALSE; goto fini0;
    }

    // Only retrieve connected/active devices
    hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCaptureDevices);
    hr |= pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pRenderDevices);

    if (hr != S_OK) {
        NiDebug("Failed enumerating capture/render devices! error: 0x%08x", hr);
        ret = FALSE; goto fini0;
    }

    pCaptureDevices->GetCount(&nCaptureDevices);
    pRenderDevices->GetCount(&nRenderDevices);

    if (!nCaptureDevices || !nRenderDevices) {
        NiDebug("Empty capture/render devices sets!");
        ret = FALSE; goto fini0;
    }

    if ((lpCaps->iPreferredCaptureDevice >= nCaptureDevices) ||
        (lpCaps->iPreferredRenderDevice >= nRenderDevices))
    {
        NiDebug("Mismatching preferred capture/render device!");
        ret = FALSE; goto fini0;
    }

    hr = pCaptureDevices->Item(lpCaps->iPreferredCaptureDevice, &m_pCaptureDevice);
    hr |= pRenderDevices->Item(lpCaps->iPreferredRenderDevice, &m_pRenderDevice);

    if (hr != S_OK) {
        NiDebug("Failed to retrieve a capture/render device! error: 0x%08x", hr);
        ret = FALSE; goto fini0;
    }

    if (!m_pCaptureDevice || !m_pRenderDevice) {
        NiDebug("Bad capture/render device instance!");
        ret = FALSE; goto fini0;
    }

    m_pCaptureDevice->GetId(&m_CaptureId);
    m_pRenderDevice->GetId(&m_RenderId);

    NiDebug("Capture device: %s", m_CaptureId);
    NiDebug("Render device: %s", m_RenderId);

    hr = m_pCaptureDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_piCaptureAudioClient);
    hr |= m_pRenderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_piRenderAudioClient);

    if (hr != S_OK) {
        NiDebug("Failed activating capture/render audio clients! error: 0x%08x", hr);
        ret = FALSE; goto fini0;
    }

    if (!m_piCaptureAudioClient || !m_piRenderAudioClient) {
        NiDebug("Failed instantiating capture/render audio clients! error: 0x%08x", hr);
        ret = FALSE; goto fini0;
    }

    memset(&wavefmtex, 0, sizeof(wavefmtex));

    wavefmtex.cbSize = sizeof(WAVEFORMATEX);
    wavefmtex.nChannels = 1;
    wavefmtex.nBlockAlign = 2;
    wavefmtex.nSamplesPerSec = lpCaps->nSamplingRate;
    wavefmtex.nAvgBytesPerSec = wavefmtex.nSamplesPerSec * 2;
    wavefmtex.wBitsPerSample = 16;
    wavefmtex.wFormatTag = WAVE_FORMAT_PCM;

    hr = m_piCaptureAudioClient->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                            lpCaps->buffer.tBufferDurationNS, lpCaps->buffer.tBufferDurationNS, &wavefmtex, &m_SessionGUID);

    if (hr != S_OK) {
        NiDebug("Failed initializing the audio capture client! error: 0x%08x", hr);
        ret = FALSE; goto fini0;
    }

    hr = m_piRenderAudioClient->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                           AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                           lpCaps->buffer.tBufferDurationNS, lpCaps->buffer.tBufferDurationNS, &wavefmtex, &m_SessionGUID);

    if (hr != S_OK) {
        NiDebug("Failed initializing the audio render client! error: 0x%08x", hr);
        ret = FALSE; goto fini0;
    }

    m_hCaptureEvent = CreateEvent(NULL, FALSE, FALSE, TEXT("SFXCaptureEvent"));
    m_hRenderEvent = CreateEvent(NULL, FALSE, FALSE, TEXT("SFXRenderEvent"));

    if (!m_hCaptureEvent || !m_hRenderEvent) {
        NiDebug("Failed creating audio capture/render events!");
        ret = FALSE; goto fini0;
    }

    m_piCaptureAudioClient->SetEventHandle(m_hCaptureEvent);
    m_piRenderAudioClient->SetEventHandle(m_hRenderEvent);

    // from Windows SDK, calls MMCSS to boost the thread's priority
    DWORD taskIndex = 0;
    hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);

    if (!hTask)
        NiDebug("Failed boosting the thread's priority! error: 0x%08x", hr);

fini0:
    if (!ret && m_piCaptureAudioClient)
        SAFE_RELEASE(m_piCaptureAudioClient);

    if (!ret && m_piRenderAudioClient)
        SAFE_RELEASE(m_piRenderAudioClient);

    if (!ret && m_pRenderDevice)
        SAFE_RELEASE(m_pRenderDevice);

    if (!ret && m_pCaptureDevice)
        SAFE_RELEASE(m_pCaptureDevice);

    if (pCaptureDevices)
        SAFE_RELEASE(pCaptureDevices);

    if (pRenderDevices)
        SAFE_RELEASE(pRenderDevices);

    if (pEnumerator)
        SAFE_RELEASE(pEnumerator);

    return ret;
}

void CoreAudioDriver::Close(LPAUDIODRIVERCAPS lpCaps)
{
    if (m_pRenderDevice)
        SAFE_RELEASE(m_pRenderDevice);

    if (m_pCaptureDevice)
        SAFE_RELEASE(m_pCaptureDevice);

    if (m_piCaptureAudioClient)
        SAFE_RELEASE(m_piCaptureAudioClient);

    if (m_piRenderAudioClient)
        SAFE_RELEASE(m_piCaptureAudioClient);

    if (m_piCaptureClient)
        SAFE_RELEASE(m_piCaptureClient);

    if (m_piRenderClient)
        SAFE_RELEASE(m_piRenderClient);

    if (m_CaptureId) {
        CoTaskMemFree(m_CaptureId);
        m_CaptureId = NULL;
    }

    if (m_RenderId) {
        CoTaskMemFree(m_RenderId);
        m_RenderId = NULL;
    }
}

BOOL CoreAudioDriver::Start()
{
    HRESULT hr;

    if (!m_piCaptureAudioClient || !m_piRenderAudioClient) {
        NiDebug("Invalid capture/render client!");
        return FALSE;
    }

    if (!m_piCaptureClient || !m_piRenderClient) {
        hr = m_piCaptureAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_piCaptureClient);
        hr |= m_piRenderAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_piRenderClient);

        if (hr != S_OK) {
            NiDebug("Failed activating capture/render audio services! error: 0x%08x", hr);
            goto fini1;
        }
    }

    hr = m_piCaptureAudioClient->Start();
    hr |= m_piRenderAudioClient->Start();

    return (hr == S_OK);

fini1:
    if (m_piCaptureClient)
        SAFE_RELEASE(m_piCaptureClient);

    if (m_piRenderClient)
        SAFE_RELEASE(m_piRenderClient);

    return FALSE;
}

void CoreAudioDriver::Stop()
{
    HRESULT hr;

    if (!m_piCaptureAudioClient || !m_piRenderAudioClient) {
        NiDebug("Invalid capture/render client!");
        return;
    }

    hr = m_piCaptureAudioClient->Stop();
    hr |= m_piRenderAudioClient->Stop();

    if (hr != S_OK)
        NiDebug("Failed stopping capture/render audio clients!");
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
