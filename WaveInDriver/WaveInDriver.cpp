//
// waveIn/waveOut driver for SonicFX
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
#include <mmsystem.h>
#include <assert.h>

#include <stdio.h>

#include <AudioDriver.h>

#define WAVEHEADERSCOUNT 16

void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);

class __declspec(dllexport) WaveInDriver : public AudioDriver {
public:
	WaveInDriver();
	~WaveInDriver();

	BOOL Open(LPAUDIODRIVERCAPS lpCaps);
	void Close(LPAUDIODRIVERCAPS lpCaps);

	BOOL Start();
	void Stop();

	void Write(int idx);

	void SetQueueSize(int cPackets);
	float GetPacketLatency();

	const char* GetDescription();

private:
	LPAUDIODRIVERCAPS lpCaps;

	HWAVEOUT m_hWaveOut;
	HWAVEIN m_hWaveIn;

	WAVEHDR m_waveInHeader[WAVEHEADERSCOUNT];
	WAVEHDR m_waveOutHeader[WAVEHEADERSCOUNT];

	LPWAVEHDR m_lpCompletedWhdr;

	HANDLE m_hThread, m_hCompletionEvent;
	BOOL m_bRun;

	short *m_lpPlaybackBuffer[WAVEHEADERSCOUNT], *m_lpAlignedPlaybackBuffer[WAVEHEADERSCOUNT];
	short *m_lpCaptureBuffer[WAVEHEADERSCOUNT], *m_lpAlignedCaptureBuffer[WAVEHEADERSCOUNT];

	friend void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
	friend void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
	friend DWORD WINAPI CompletionThread(LPVOID lpData);
};

WaveInDriver::WaveInDriver()
{
}

WaveInDriver::~WaveInDriver()
{
}

DWORD WINAPI CompletionThread(LPVOID lpData)
{
	WaveInDriver* pWaveInDriver = (WaveInDriver*)lpData;

	while (pWaveInDriver->m_bRun) {
		WaitForSingleObject(pWaveInDriver->m_hCompletionEvent, 100);
		LPWAVEHDR lpwvhdr = pWaveInDriver->m_lpCompletedWhdr;

		if (pWaveInDriver->m_bRun) {
			int idx = ((int)lpwvhdr - (int)pWaveInDriver->m_waveInHeader) / sizeof(WAVEHDR);
			SetEvent(pWaveInDriver->lpCaps->hpPacketEvent[idx]);

            lpwvhdr->dwFlags = 0;
			waveInPrepareHeader(pWaveInDriver->m_hWaveIn, lpwvhdr, sizeof(WAVEHDR));
            assert(waveInAddBuffer(pWaveInDriver->m_hWaveIn, lpwvhdr, sizeof(WAVEHDR)) == MMSYSERR_NOERROR);
		}
	}

	return 0;
}

void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	WaveInDriver* pWaveInDriver = (WaveInDriver*)dwInstance;

	switch (uMsg) {
	case WOM_DONE:
		break;
	}
}

void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	WaveInDriver* pWaveInDriver = (WaveInDriver*)dwInstance;

	switch (uMsg) {
	case WIM_DATA:
		pWaveInDriver->m_lpCompletedWhdr = (LPWAVEHDR)dwParam1;
		SetEvent(pWaveInDriver->m_hCompletionEvent);
		break;
	}
}

BOOL WaveInDriver::Open(LPAUDIODRIVERCAPS lpAudioDriverCaps)
{
	WAVEFORMATEX format;

	lpCaps = lpAudioDriverCaps;

	lpCaps->nPackets = WAVEHEADERSCOUNT;
	lpCaps->nSamplingRate = 44100;
	lpCaps->hpPacketEvent = new HANDLE[WAVEHEADERSCOUNT];
	lpCaps->lpPlaybackBuffer = new short*[WAVEHEADERSCOUNT];
	lpCaps->lpCaptureBuffer = new short*[WAVEHEADERSCOUNT];

	// waveIn/waveOut initialization

	ZeroMemory(m_waveInHeader, WAVEHEADERSCOUNT * sizeof(WAVEHDR));
	ZeroMemory(m_waveOutHeader, WAVEHEADERSCOUNT * sizeof(WAVEHDR));
	
	for (int i = 0; i < WAVEHEADERSCOUNT; i++) {
		m_lpCaptureBuffer[i] = (short*)GlobalAlloc(GPTR, lpCaps->nPacketSize * sizeof(short) + 15);
		m_lpPlaybackBuffer[i] = (short*)GlobalAlloc(GPTR, (lpCaps->nPacketSize + 15) * sizeof(short));

		m_lpAlignedCaptureBuffer[i] = (short*)(((unsigned int)m_lpCaptureBuffer[i] + 15L) & ~15L);
		m_lpAlignedPlaybackBuffer[i] = (short*)(((unsigned int)m_lpPlaybackBuffer[i] + 15L) & ~15L);

		m_waveInHeader[i].lpData = (LPSTR)m_lpAlignedCaptureBuffer[i];
		m_waveInHeader[i].dwBufferLength = lpCaps->nPacketSize * sizeof(short);

		m_waveOutHeader[i].lpData = (LPSTR)m_lpAlignedPlaybackBuffer[i];
		m_waveOutHeader[i].dwBufferLength = lpCaps->nPacketSize * sizeof(short);

		lpCaps->lpCaptureBuffer[i] = (short*)m_waveInHeader[i].lpData;
		lpCaps->lpPlaybackBuffer[i] = (short*)m_waveOutHeader[i].lpData;
		lpCaps->hpPacketEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
	}

	ZeroMemory(&format, sizeof(format));

	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = 1;
	format.nSamplesPerSec = 44100;
	format.wBitsPerSample = 16;
	format.nBlockAlign = 2;
	format.nAvgBytesPerSec = format.nBlockAlign * format.nSamplesPerSec;

	if (waveInOpen(&m_hWaveIn, WAVE_MAPPER, &format, (DWORD) waveInProc, (DWORD)this, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
		return FALSE;

	if (waveOutOpen(&m_hWaveOut, WAVE_MAPPER, &format, (DWORD)waveOutProc, (DWORD)this, CALLBACK_FUNCTION) != MMSYSERR_NOERROR)
		return FALSE;

	m_bRun = TRUE;

	m_hCompletionEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!m_hCompletionEvent) return FALSE;

	DWORD tID;

	m_hThread = CreateThread(NULL, 0, CompletionThread, this, 0, &tID);
	if (!m_hThread) return FALSE;

	SetThreadPriority(m_hThread, THREAD_PRIORITY_HIGHEST);
	return TRUE;
}

void WaveInDriver::Close(LPAUDIODRIVERCAPS lpCaps)
{
	if (m_hWaveOut) {
		waveOutClose(m_hWaveOut);
        m_hWaveOut = NULL;
	}
		
	if (m_hWaveIn) {
		waveInClose(m_hWaveIn);
        m_hWaveIn = NULL;
	}

	if (m_hThread) {
        m_bRun = FALSE;
        SetEvent(m_hCompletionEvent);

        if (WaitForSingleObject(m_hThread, 1000) == WAIT_TIMEOUT)
            TerminateThread(m_hThread, 0);

        CloseHandle(m_hCompletionEvent);
		CloseHandle(m_hThread);

		m_hThread = m_hCompletionEvent = NULL;
	}

	for (int i = 0; i < WAVEHEADERSCOUNT; i++) {
		if (m_lpCaptureBuffer[i]) {
			GlobalFree(m_lpCaptureBuffer[i]);
			m_lpCaptureBuffer[i] = NULL;
		}

		if (m_lpPlaybackBuffer[i]) {
			GlobalFree(m_lpPlaybackBuffer[i]);
			m_lpPlaybackBuffer[i] = NULL;
		}
	}

	if (lpCaps->hpPacketEvent) {
		for (int i = 0; i < WAVEHEADERSCOUNT; i++)
			CloseHandle(lpCaps->hpPacketEvent[i]);

		delete [] lpCaps->hpPacketEvent;
		lpCaps->hpPacketEvent = NULL;
	}

	if (lpCaps->lpPlaybackBuffer) {
		delete [] lpCaps->lpPlaybackBuffer;
		lpCaps->lpPlaybackBuffer = NULL;
	}

	if (lpCaps->lpCaptureBuffer) {
		delete [] lpCaps->lpCaptureBuffer;
		lpCaps->lpCaptureBuffer = NULL;
	}
}

BOOL WaveInDriver::Start()
{
	for (int i = 0; i < WAVEHEADERSCOUNT; i++)
		waveInPrepareHeader(m_hWaveIn, &m_waveInHeader[i], sizeof(WAVEHDR));

	for (int i = 0; i < WAVEHEADERSCOUNT; i++)
		waveInAddBuffer(m_hWaveIn, &m_waveInHeader[i], sizeof(WAVEHDR));

	assert(waveInStart(m_hWaveIn) == MMSYSERR_NOERROR);

	return TRUE;
}

void WaveInDriver::Stop()
{
	waveOutReset(m_hWaveOut);

	for (int i = 0; i < WAVEHEADERSCOUNT; i++)
		if (m_waveOutHeader[i].dwFlags & WHDR_PREPARED)
			waveOutUnprepareHeader(m_hWaveOut, &m_waveOutHeader[i], sizeof(WAVEHDR));

	waveInReset(m_hWaveIn);
	waveInStop(m_hWaveIn);

	for (int i = 0; i < WAVEHEADERSCOUNT; i++)
		if (m_waveInHeader[i].dwFlags & WHDR_PREPARED)
			waveInUnprepareHeader(m_hWaveIn, &m_waveInHeader[i], sizeof(WAVEHDR));
}

void WaveInDriver::Write(int idx)
{
	m_waveOutHeader[idx].dwFlags = 0;
	waveOutPrepareHeader(m_hWaveOut, &m_waveOutHeader[idx], sizeof(WAVEHDR));
	assert(waveOutWrite(m_hWaveOut, &m_waveOutHeader[idx], sizeof(WAVEHDR)) == MMSYSERR_NOERROR);
}

float WaveInDriver::GetPacketLatency()
{
	return 0;
}

void WaveInDriver::SetQueueSize(int cPackets)
{
}

const char* WaveInDriver::GetDescription()
{
	return "waveOut/waveIn Driver v0.2";
}

extern "C" __declspec(dllexport) void GetAudioDriverFactory(LPAUDIODRIVER *lpAudioDriver)
{
	*lpAudioDriver = new WaveInDriver();
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
