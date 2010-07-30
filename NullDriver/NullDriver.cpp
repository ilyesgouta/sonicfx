//
// Null driver for SonicFX
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

#include <stdio.h>
#include <windows.h>

#include <AudioDriver.h>

#define AUDIO_BUFFERS_COUNT           2

DWORD WINAPI DummyThread(LPVOID lpParam);

class __declspec(dllexport) NullAudioDriver : public AudioDriver {
public:
    NullAudioDriver();
    ~NullAudioDriver();

    BOOL Open(LPAUDIODRIVERCAPS lpCaps);
    void Close(LPAUDIODRIVERCAPS lpCaps);

    BOOL Start();
    void Stop();

    void Write(int idx);

    void SetQueueSize(int cPackets);
    float GetPacketLatency();

    const char* GetDescription();

private:
    HANDLE hThread;
    volatile BOOL bRun;

    short* lpPlaybackBuffer[AUDIO_BUFFERS_COUNT];
    short* lpCaptureBuffer[AUDIO_BUFFERS_COUNT];

    short* lpAlignedPlaybackBuffer[AUDIO_BUFFERS_COUNT];
    short* lpAlignedCaptureBuffer[AUDIO_BUFFERS_COUNT];
    
    LPAUDIODRIVERCAPS lpCaps;
    
    friend DWORD WINAPI DummyThread(LPVOID lpParam);
};

DWORD WINAPI DummyThread(LPVOID lpParam)
{
    NullAudioDriver *pDriver = (NullAudioDriver*)lpParam;
    
    for (int i = 0; pDriver->bRun; ++i %= AUDIO_BUFFERS_COUNT) {
        printf("NullAudioDriver: Pumping event...\n");
        SetEvent(pDriver->lpCaps->hpPacketEvent[i]);
        Sleep(150);
    }
    
    return 0;
}

NullAudioDriver::NullAudioDriver()
{
}

NullAudioDriver::~NullAudioDriver()
{
}

BOOL NullAudioDriver::Open(LPAUDIODRIVERCAPS lpAudioDriverCaps)
{
    // Fill up the remaining fields of the caps structure.
    lpAudioDriverCaps->nPackets = AUDIO_BUFFERS_COUNT;
    lpAudioDriverCaps->nSamplingRate = 0;
    lpAudioDriverCaps->lpPlaybackBuffer = new short*[AUDIO_BUFFERS_COUNT];
    lpAudioDriverCaps->lpCaptureBuffer = new short*[AUDIO_BUFFERS_COUNT];
    lpAudioDriverCaps->hpPacketEvent = new HANDLE[AUDIO_BUFFERS_COUNT];
    
    for (int i = 0; i < AUDIO_BUFFERS_COUNT; i++) {
        lpPlaybackBuffer[i] = (short*)GlobalAlloc(GPTR, (lpAudioDriverCaps->nPacketSize + 15) * sizeof(short));
        lpCaptureBuffer[i] = (short*)GlobalAlloc(GPTR, (lpAudioDriverCaps->nPacketSize + 15) * sizeof(short));

        // Align the buffers on a 16-byte boundary.
        lpAlignedPlaybackBuffer[i] = (short*)(((unsigned int)lpPlaybackBuffer[i] + 15L) & ~15L);
        lpAlignedCaptureBuffer[i] = (short*)(((unsigned int)lpCaptureBuffer[i] + 15L) & ~15L);

        lpAudioDriverCaps->hpPacketEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        lpAudioDriverCaps->lpPlaybackBuffer[i] = lpAlignedPlaybackBuffer[i];
        lpAudioDriverCaps->lpCaptureBuffer[i] = lpAlignedCaptureBuffer[i];
    }
    
    lpCaps = lpAudioDriverCaps; // We maintain an internal copy on this structure.

    printf("NullAudioDriver: Initialization done.\n");
    
    return TRUE;
}

void NullAudioDriver::Close(LPAUDIODRIVERCAPS lpCaps)
{
    for (int i = 0; i < AUDIO_BUFFERS_COUNT; i++) {
        GlobalFree(lpPlaybackBuffer[i]);
        GlobalFree(lpCaptureBuffer[i]);
    }

    delete [] lpCaps->lpPlaybackBuffer;
    delete [] lpCaps->lpCaptureBuffer;
    delete [] lpCaps->hpPacketEvent;
}

BOOL NullAudioDriver:: Start()
{
    DWORD dwThreadID;
    
    bRun = TRUE;

    printf("NullAudioDriver: Start.\n");
    
    hThread = CreateThread(NULL, 0, DummyThread, this, 0, &dwThreadID);
    return (hThread != NULL);
}

void NullAudioDriver::Stop()
{
    bRun = FALSE;
    
    if (WaitForSingleObject(hThread, 500) == WAIT_TIMEOUT)
        TerminateThread(hThread, 0);
    
    CloseHandle(hThread);

    printf("NullAudioDriver: Stop.\n");
}

void NullAudioDriver::Write(int idx)
{
}

void NullAudioDriver::SetQueueSize(int cPackets)
{
}

float NullAudioDriver::GetPacketLatency()
{
    return 0;
}

const char* NullAudioDriver::GetDescription()
{
    return "NullAudioDriver v0.1";
}

extern "C" __declspec(dllexport) void GetAudioDriverFactory(LPAUDIODRIVER *lpAudioDriver)
{
    *lpAudioDriver = new NullAudioDriver();
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
