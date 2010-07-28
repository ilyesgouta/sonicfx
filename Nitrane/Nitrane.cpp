//
// Nitrane class implemenation
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
// Notes: Audio is 16bit per sample, 44100Hz, mono. All samples are converted to floats
// before any processing. Once done/applied, everything is converted to signed shorts
// and then is sent for playback by the audio device.
//

#include <windows.h>

#include "AudioDriver.h"
#include "AudioEffect.h"
#include "Nitrane.h"

extern "C" void short2float(short* in, float* out, int size);
extern "C" void float2short(float* in, short* out, int size);

// C fallback
extern "C" void Cshort2float(short* pSrc, float* pDest, int size)
{
	for (int i = 0; i < size; i++)
		*pDest++ = (float)*pSrc++;
}

// C fallback
extern "C" void Cfloat2short(float* pSrc, short* pDest, int size)
{
	for (int i = 0; i < size; i++)
		*pDest++ = (short)*pSrc++;
}

DWORD WINAPI SlaveThread(LPVOID lpData)
{
	volatile LPPTCTX lpPtCtx = (LPPTCTX)lpData;
	LPEFFECTCTX lpEffectCtx;

	while (*(lpPtCtx->bRun))
	{
		WaitForSingleObject(lpPtCtx->lpSharedCtx->hSemaphore, INFINITE);
		lpEffectCtx = lpPtCtx->lpEffectCtx;

		if (!*(lpPtCtx->bRun))
			break;

        if (lpEffectCtx->lpEffect->IsInPlaceProcessing()) lpEffectCtx->lpEffect->Process(lpEffectCtx->ppIn, lpPtCtx->idxOffset, lpPtCtx->nSamples);
        else lpEffectCtx->lpEffect->Process(lpEffectCtx->ppIn, lpEffectCtx->ppOut, lpPtCtx->idxOffset, lpPtCtx->nSamples);

		LONG prev = InterlockedIncrement(&lpPtCtx->lpSharedCtx->nProcessingThreads);
			
		if (prev == lpPtCtx->lpSharedCtx->lpNitrane->m_ThreadsCount) {
			printf("Thread: 0x%x -> Firing completion event...\n", GetCurrentThreadId());
			SetEvent(lpPtCtx->lpSharedCtx->hCompletionEvent);
		}
	}

	return 0;
}

DWORD WINAPI MasterThread(LPVOID lpData)
{
	volatile LPSHAREDCTX lpSharedCtx = (LPSHAREDCTX)lpData;
	volatile LPAUDIODRIVERCAPS lpCaps;
	
	lpCaps = &lpSharedCtx->lpNitrane->m_AudioCaps;

	while (lpSharedCtx->lpNitrane->m_bProcess)
	{
		if (lpSharedCtx->lpNitrane->m_bSuspended) {
			SetEvent(lpSharedCtx->lpNitrane->m_hSuspendEventACK);
			ResetEvent(lpSharedCtx->lpNitrane->m_hSuspendEvent);
			WaitForSingleObject(lpSharedCtx->lpNitrane->m_hSuspendEvent, INFINITE);
		}

		if (!lpSharedCtx->lpNitrane->m_bProcess)
			break;

		DWORD dwResult = WaitForMultipleObjects(lpCaps->nPackets, lpCaps->hpPacketEvent, FALSE, 300);

		if (!lpSharedCtx->lpNitrane->m_bProcess)
			break;
		
		if (dwResult == WAIT_TIMEOUT) {
			printf("MasterThread: WaitForMultipleObjects timed out!\n");
			continue;
		}

		if ((dwResult >= WAIT_ABANDONED_0) && (dwResult <= (WAIT_ABANDONED_0 + lpCaps->nPackets - 1))) {
			LeaveCriticalSection(&lpSharedCtx->m_CS);
			printf("MasterThread: audio packet abandoned!\n");
			break;
		}
		
		if ((dwResult >= WAIT_OBJECT_0) && (dwResult <= (WAIT_OBJECT_0 + lpCaps->nPackets - 1))) // We got a new audio packet.
		{
			int idxObject = dwResult - WAIT_OBJECT_0;
			//printf("MasterThread: received an audio packet!\n");
			
			// Fast SSE2 16bit to float conversion.
			short2float(lpCaps->lpCaptureBuffer[idxObject], lpSharedCtx->lpNitrane->m_HeadEffect->ppOut[0], lpCaps->nPacketSize);
			
			// Processing...
			lpSharedCtx->lpNitrane->ProcessEffects();

			// Fast SSE2 float to 16bit conversion.
			float2short(lpSharedCtx->lpNitrane->m_TailEffect->ppIn[0], lpCaps->lpPlaybackBuffer[lpSharedCtx->idxOutputBuffer], lpCaps->nPacketSize);

			lpSharedCtx->lpNitrane->m_lpAudioDriver->Write(lpSharedCtx->idxOutputBuffer);

			lpSharedCtx->idxOutputBuffer++;
			lpSharedCtx->idxOutputBuffer %= lpCaps->nPackets;
		}
	}

	return 0;
}

Nitrane::Nitrane()
{
	m_ThreadsCount = 1;
	m_hMasterThread = NULL;
	m_phSlaveThread = NULL;
	m_lpAudioDriver = NULL;
	m_IOPacketSize = 0;

	m_AudioDrivers.clear();
	m_AudioEffects.clear();
	m_hModules.clear();
	m_lpBuffers.clear();
}

Nitrane::~Nitrane()
{
	Finalize();
}

const vector <LPAUDIODRIVER>& Nitrane::EnumAudioDrivers()
{
	return (m_AudioDrivers);
}

const vector <LPAUDIOEFFECTFACTORY>& Nitrane::EnumAudioEffects()
{
	return (m_AudioEffects);
}

BOOL Nitrane::Initialize(char* path, int MaxThreads)
{
	WIN32_FIND_DATA FindData;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	
	unsigned int SSE2FPUMask;

#ifdef WIN32
	__asm {
		stmxcsr SSE2FPUMask;
	}
#else
	asm volatile ("stmxcsr %0" : "=g" (SSE2FPUMask) : );
#endif
	
	// Set the rounding mode to round toward zero instead of the default round to nearest.
	SSE2FPUMask |= (3L << 13);
	
#ifdef WIN32
	__asm {
		ldmxcsr SSE2FPUMask;
	}
#else
	asm volatile ("ldmxcsr %0" : : "g" (SSE2FPUMask));
#endif

	TCHAR szFn[MAX_PATH];

	if (MaxThreads <= 0)
        return FALSE;

#ifdef WIN32
	if (path)
        wsprintf(szFn, L"%s\\*.dll", path);
#else
	if (path)
        wsprintf(szFn, "%s\\*.so", path);
#endif
	else {
		TCHAR szFolder[MAX_PATH];
		GetModuleFileName(NULL, szFolder, sizeof(szFolder) / sizeof(TCHAR));

		PTCHAR p = szFolder + lstrlen(szFolder);
		while (*p != '\\')
            p--;
        *p = 0;

#ifdef WIN32
		wsprintf(szFn, L"%s\\*.dll", szFolder);
#else
		wsprintf(szFn, "%s\\*.so", szFolder);
#endif
	}

	hFind = FindFirstFile(szFn, &FindData);
	if (hFind == INVALID_HANDLE_VALUE) return FALSE;

	do {
		HMODULE hModule = LoadLibrary(FindData.cFileName);
		if (!hModule) continue;

		LPAUDIODRIVERFACTORY lpDriverFacty = (LPAUDIODRIVERFACTORY)GetProcAddress(hModule, "GetAudioDriverFactory");
		LPAUDIOEFFECTFACTORY lpEffectFacty = (LPAUDIOEFFECTFACTORY)GetProcAddress(hModule, "GetAudioEffectFactory");
			
		if (!lpDriverFacty && !lpEffectFacty) {
			FreeLibrary(hModule);
			continue;
		}

		if (lpEffectFacty) m_AudioEffects.push_back(lpEffectFacty);
		else if (lpDriverFacty) {
			LPAUDIODRIVER lpAudioDriver = NULL;
			(*lpDriverFacty)(&lpAudioDriver); // Create an instance of the audio driver.

			m_AudioDrivers.push_back(lpAudioDriver);
		}

		m_hModules.push_back(hModule);
	} while (FindNextFile(hFind, &FindData));

	FindClose(hFind);

	if (!m_AudioDrivers.size()) return FALSE;

	m_ThreadsCount = (MaxThreads > 0) ? MaxThreads : 1;
	
	m_SharedCtx.lpNitrane = this;
	m_SharedCtx.nProcessingThreads = 0;

	m_SharedCtx.hCompletionEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_SharedCtx.hSemaphore = CreateSemaphore(NULL, 0, m_ThreadsCount, NULL);

	m_bSuspended = TRUE;

	m_hSuspendEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hSuspendEventACK = CreateEvent(NULL, TRUE, FALSE, NULL);
	
	InitializeCriticalSection(&m_SharedCtx.m_CS);

	m_phSlaveThread = new HANDLE[m_ThreadsCount];
	m_pCtx = new PTCTX[m_ThreadsCount];

	DWORD ThreadID;

	m_hMasterThread = CreateThread(NULL, 0, MasterThread, (LPVOID)&m_SharedCtx, 0, &ThreadID);
	SetThreadPriority(m_hMasterThread, THREAD_PRIORITY_HIGHEST);

	for (int i = 0; i < m_ThreadsCount; i++) {
		m_pCtx[i].bRun = (BOOL*)&m_bProcess;
		m_pCtx[i].idxOffset = 0;
		m_pCtx[i].nSamples = 0;
		m_pCtx[i].lpSharedCtx = &m_SharedCtx;

		m_phSlaveThread[i] = CreateThread(NULL, 0, SlaveThread, (LPVOID)&m_pCtx[i], 0, &ThreadID);
		SetThreadPriority(m_phSlaveThread[i], THREAD_PRIORITY_TIME_CRITICAL);
	}

	m_HeadEffect = new EFFECTCTX(0, 1);
	m_TailEffect = new EFFECTCTX(1, 0);

	return TRUE;
}

void Nitrane::Finalize()
{
	m_bProcess = FALSE;

	if (m_SharedCtx.hSemaphore)
		ReleaseSemaphore(m_SharedCtx.hSemaphore, m_ThreadsCount, NULL);

	if (m_AudioCaps.hpPacketEvent)
		SetEvent(m_AudioCaps.hpPacketEvent[0]);

	if (m_hMasterThread)
	{
		if (WaitForMultipleObjects(m_ThreadsCount, m_phSlaveThread, TRUE, 1000) == WAIT_TIMEOUT) {
			for (int i = 0; i < m_ThreadsCount; i++)
				TerminateThread(m_phSlaveThread[i], 0);
		}

		if (WaitForSingleObject(m_hMasterThread, 1000) == WAIT_TIMEOUT)
			TerminateThread(m_hMasterThread, 0);

		for (int i = 0; i < m_ThreadsCount; i++) {
			CloseHandle(m_phSlaveThread[i]);
			m_phSlaveThread[i] = NULL;
		}

		CloseHandle(m_hMasterThread);
		m_hMasterThread = NULL;
	}

	if (m_phSlaveThread)
		delete [] m_phSlaveThread;
	m_phSlaveThread = NULL;

	if (m_SharedCtx.hCompletionEvent)
	CloseHandle(m_SharedCtx.hCompletionEvent);

	m_SharedCtx.hCompletionEvent = NULL;

	if (m_SharedCtx.hSemaphore)
		CloseHandle(m_SharedCtx.hSemaphore);

	m_SharedCtx.hSemaphore = NULL;

	if (m_pCtx)
		delete [] m_pCtx;
	m_pCtx = NULL;

	DeleteCriticalSection(&m_SharedCtx.m_CS);

	m_hMasterThread = NULL;
	m_phSlaveThread = NULL;
	m_lpAudioDriver = NULL;
	m_IOPacketSize = 0;

	for (vector<LPAUDIODRIVER>::iterator it = m_AudioDrivers.begin(); it != m_AudioDrivers.end(); it++)
		delete (*it);

	for (vector<HMODULE>::iterator it = m_hModules.begin(); it != m_hModules.end(); it++)
		FreeLibrary(*it);

	for (hash_map<unsigned int, LPVOID>::iterator it = m_lpBuffers.begin(); it != m_lpBuffers.end(); it++)
		VirtualFree((*it).second, 0, MEM_RELEASE);

	m_AudioDrivers.clear();
	m_AudioEffects.clear();
	m_hModules.clear();
	m_lpBuffers.clear();

	if (m_HeadEffect)
		delete m_HeadEffect;
	m_HeadEffect = NULL;

	if (m_TailEffect)
		delete m_TailEffect;
	m_TailEffect = NULL;
}

BOOL Nitrane::LoadGraph(char* fn)
{
	return FALSE;
}

BOOL Nitrane::SaveGraph(char* fn)
{
	return FALSE;
}

void Nitrane::SetAudioDriver(LPAUDIODRIVER lpAudioDriver)
{
	m_lpAudioDriver = lpAudioDriver;
	printf("Nitrane: audio driver set! (0x%x)\n", (unsigned int)m_lpAudioDriver);
}

void Nitrane::SuspendProcessing()
{
	if (!m_bProcess) return;

	m_bSuspended = TRUE;

	ResetEvent(m_hSuspendEventACK);
	WaitForSingleObject(m_hSuspendEventACK, INFINITE);
}

void Nitrane::ResumeProcessing()
{
	if (!m_bProcess) return;

	m_bSuspended = FALSE;
	SetEvent(m_hSuspendEvent);
}

BOOL Nitrane::Start()
{
	if (!m_lpAudioDriver) return FALSE;
	
	// Ask the attached effect plugins for their preferred packet size.
	int nSamples = NegotiatePacketSize();
	printf("Nitrane: negotiated packet size == %d\n", nSamples);
	
	if (nSamples <= 0)
		return FALSE;

	nSamples &= ~7L; // nSamples must be a multiple of 8.

	m_SharedCtx.nTotalSamples = nSamples;
	m_SharedCtx.idxOutputBuffer = 0;

	if (!AllocateBuffers())
		return FALSE;

	m_pfNullBuffer = AllocateBuffer();

	if (!m_pfNullBuffer)
		return FALSE;

	memset(m_pfNullBuffer, 0, m_SharedCtx.nTotalSamples * sizeof(float));

	ReconnectBuffers();

	memset(&m_AudioCaps, 0, sizeof(m_AudioCaps));

	// Recommended capture buffers size.
	m_AudioCaps.nPacketSize = nSamples;

	if (!m_lpAudioDriver->Open(&m_AudioCaps))
		return FALSE;

	for (int i = 0; i < m_ThreadsCount; i++) {
		m_pCtx[i].idxOffset = i * (nSamples / m_ThreadsCount);
		m_pCtx[i].nSamples = nSamples / m_ThreadsCount;
	}

	m_bProcess = TRUE;
	ResumeProcessing();

	m_fLatency = 0;
	QueryPerformanceFrequency(&m_liPerfFreq);

	ConfigureEffects();
	m_lpAudioDriver->Start();

	return TRUE;
}

void Nitrane::Stop()
{
	SuspendProcessing();

	m_lpAudioDriver->Stop();
	m_lpAudioDriver->Close(&m_AudioCaps);
}

int Nitrane::NegotiatePacketSize()
{
	list<LPEFFECTCTX> stack;
	LPEFFECTCTX lpEffectCtx;

	int max = 0;
	stack.push_front(m_HeadEffect);

	for (lpEffectCtx = m_HeadEffect; stack.size();)
	{
		if ((lpEffectCtx->lpEffect) && (max < lpEffectCtx->lpEffect->GetPreferredPacketSize()))
			max = lpEffectCtx->lpEffect->GetPreferredPacketSize();

		for (int i = 0; i < (int)lpEffectCtx->pOutputPins->size(); i++)
			stack.push_back((*lpEffectCtx->pOutputPins)[i]);

		stack.pop_front();
		if (stack.size()) lpEffectCtx = stack.front();
	}

	return (max);
}

float* Nitrane::AllocateBuffer()
{
	LPVOID lpBuffer = VirtualAlloc(NULL, m_SharedCtx.nTotalSamples * sizeof(float) + 15, MEM_COMMIT, PAGE_READWRITE);
	if (lpBuffer) VirtualLock(lpBuffer, m_SharedCtx.nTotalSamples * sizeof(float) + 15); // Lock it in physical memory if possible...

	LPVOID lpAlignedBuffer = (LPVOID)((unsigned int)lpBuffer & ~15L);
	m_lpBuffers[(unsigned int)lpAlignedBuffer] = lpBuffer;

	return ((float*)lpAlignedBuffer);
}

BOOL Nitrane::AllocateBuffers()
{
	// Free any previously allocated buffers...
	for (hash_map<unsigned int, LPVOID>::iterator it = m_lpBuffers.begin(); it != m_lpBuffers.end(); it++)
		VirtualFree((*it).second, 0, MEM_RELEASE);

	m_lpBuffers.clear();

	list<LPEFFECTCTX> stack;
	LPEFFECTCTX lpEffectCtx;

	stack.push_front(m_HeadEffect);

	for (lpEffectCtx = m_HeadEffect; stack.size();)
	{
        int cIn = (int)lpEffectCtx->pInputPins->size();
		int cOut = (int)lpEffectCtx->pOutputPins->size();

		if ((cOut > 0) || (lpEffectCtx->lpEffect && !lpEffectCtx->lpEffect->IsInPlaceProcessing())) {
			for (int i = 0; i < cOut; i++) {
				if (lpEffectCtx->ppOut[i])
					VirtualFree(m_lpBuffers[(unsigned int)lpEffectCtx->ppOut[i]], 0, MEM_RELEASE);
				if (!(lpEffectCtx->ppOut[i] = AllocateBuffer())) return FALSE;
			}
		} else {
			for (int i = 0; i < cOut; i++)
				lpEffectCtx->ppOut[i] = NULL; // To make it easier to debug later...
		}

		for (int i = 0; i < cOut; i++)
			stack.push_back((*lpEffectCtx->pOutputPins)[i]);

		stack.pop_front();
		if (stack.size()) lpEffectCtx = stack.front();
	}

	return TRUE;
}

BOOL Nitrane::ConnectEffect(LPEFFECTCTX lpFromEffect, LPEFFECTCTX lpToEffect, int i, int j)
{
	if (!lpToEffect && !lpFromEffect) return FALSE;

	if (!lpFromEffect) lpFromEffect = m_HeadEffect;
	if (!lpToEffect) lpToEffect = m_TailEffect;

	if ((i >= (int)lpFromEffect->pOutputPins->size()) ||
		(j >= (int)lpToEffect->pInputPins->size())) return FALSE;

	if ((*(lpFromEffect->pOutputPins))[i])
		((*(lpFromEffect->pOutputPins))[i])->ppIn[lpFromEffect->pinOut[i]] = NULL;
	
	(*(lpFromEffect->pOutputPins))[i] = lpToEffect;
	(*(lpToEffect->pInputPins))[j] = lpFromEffect;

	lpFromEffect->pinOut[i] = j;

	ReconnectBuffers();
	return TRUE;
}

// Completely disconnect an effect
void Nitrane::DisconnectEffect(LPEFFECTCTX lpEffectCtx)
{
	if (!lpEffectCtx || !lpEffectCtx->lpEffect) return;

	int cIn = (int)lpEffectCtx->pInputPins->size();
	int cOut = (int)lpEffectCtx->pOutputPins->size();

	// Invalidate the forward effect inputs'
	for (int i = 0; i < cOut; i++) {
		LPEFFECTCTX lpForwardEffect = (*(lpEffectCtx->pOutputPins))[i];

                (lpForwardEffect->ppIn)[lpEffectCtx->pinOut[i]] = NULL;

        // I know, it's a bit ugly...
        (*lpForwardEffect->pInputPins)[lpEffectCtx->pinOut[i]] = NULL;

		(*(lpEffectCtx->pOutputPins))[i] = NULL;
        lpEffectCtx->pinOut[i] = -1; // Invalid.
	}

	// Invalidate the backward effect inputs'
	for (int i = 0; i < cIn; i++) {
		LPEFFECTCTX lpBackwardEffect = (*(lpEffectCtx->pInputPins))[i];
		int j = 0, cBEOut = lpBackwardEffect->pOutputPins->size();

		while (j++ < cBEOut) {
			if ((*lpBackwardEffect->pOutputPins)[j] == lpEffectCtx)
			{
				(*lpBackwardEffect->pOutputPins)[j] = NULL;
				lpBackwardEffect->pinOut[j] = -1; // Invalid.
				break; // Connections are 1:1
			}
		}

		(*(lpEffectCtx->pInputPins))[i] = NULL;
		(lpEffectCtx->ppIn)[i] = NULL;
	}

	ReconnectBuffers();
}

void Nitrane::DisconnectEffect(LPEFFECTCTX lpEffectCtx, int i)
{
	if (!lpEffectCtx || !lpEffectCtx->lpEffect) return;
	if (i >= (int)lpEffectCtx->pOutputPins->size()) return;

	if (!(*(lpEffectCtx->pOutputPins))[i]) return;

	((*(lpEffectCtx->pOutputPins))[i])->ppIn[lpEffectCtx->pinOut[i]] = NULL;
	(*(lpEffectCtx->pOutputPins))[i] = NULL;

	// I know, it's a bit ugly...
	(*(((*(lpEffectCtx->pOutputPins))[i])->pInputPins))[lpEffectCtx->pinOut[i]] = NULL;

	lpEffectCtx->pinOut[i] = -1; // Invalid.

	ReconnectBuffers();
}

void Nitrane::ReconnectBuffers()
{
	list<LPEFFECTCTX> stack;
	LPEFFECTCTX lpEffectCtx;

	stack.push_front(m_HeadEffect);

	for (lpEffectCtx = m_HeadEffect; stack.size();)
	{
		int cIn = (int)lpEffectCtx->pInputPins->size();
		int cOut = (int)lpEffectCtx->pOutputPins->size();
		
		for (int i = 0; i < cOut; i++) {
			if ((*(lpEffectCtx->pOutputPins))[i])
				((*(lpEffectCtx->pOutputPins))[i])->ppIn[lpEffectCtx->pinOut[i]] = lpEffectCtx->ppOut[i];
		}

		// Bypass only available for 1:1 effects...
		if ((cIn == 1) && (cOut == 1) &&
			(!lpEffectCtx->bEnabled || (lpEffectCtx->lpEffect && lpEffectCtx->lpEffect->IsInPlaceProcessing())))
				if (((*(lpEffectCtx->pOutputPins))[0]))
					((*(lpEffectCtx->pOutputPins))[0])->ppIn[lpEffectCtx->pinOut[0]] = lpEffectCtx->ppIn[0];

		for (int i = 0; i < cOut; i++)
            if ((*lpEffectCtx->pOutputPins)[i])
				stack.push_back((*lpEffectCtx->pOutputPins)[i]);

		if (lpEffectCtx->lpEffect)
			lpEffectCtx->lpEffect->Configure(m_AudioCaps);

		stack.pop_front();
		if (stack.size()) lpEffectCtx = stack.front();
	}
}

void Nitrane::ConfigureEffects()
{
	list<LPEFFECTCTX> stack;
	LPEFFECTCTX lpEffectCtx;

	stack.push_front(m_HeadEffect);

	for (lpEffectCtx = m_HeadEffect; stack.size();)
	{
		int cOut = (int)lpEffectCtx->pOutputPins->size();

		if (lpEffectCtx->lpEffect)
			lpEffectCtx->lpEffect->Configure(m_AudioCaps);

		for (int i = 0; i < cOut; i++)
            if ((*lpEffectCtx->pOutputPins)[i])
				stack.push_back((*lpEffectCtx->pOutputPins)[i]);

		stack.pop_front();
		if (stack.size()) lpEffectCtx = stack.front();
	}
}

void Nitrane::BypassEffect(LPEFFECTCTX lpEffectCtx, BOOL bEnable)
{
	int cIn = (int)lpEffectCtx->pInputPins->size();
	int cOut = (int)lpEffectCtx->pOutputPins->size();

	if ((cIn != 1) || (cOut != 1)) return;

	lpEffectCtx->bEnabled = bEnable;
	ReconnectBuffers();
}

void Nitrane::InitEffectsCtx()
{
	list<LPEFFECTCTX> stack;
	LPEFFECTCTX lpEffectCtx;

	stack.push_front(m_HeadEffect);

	for (lpEffectCtx = m_HeadEffect; stack.size();)
	{
		int cIn = (int)lpEffectCtx->pInputPins->size();
		int cOut = (int)lpEffectCtx->pOutputPins->size();

		if (cIn > 1)
			lpEffectCtx->npInputs = 0;
		
		for (int i = 0; i < cOut; i++)
            if (lpEffectCtx->bEnabled && (*lpEffectCtx->pOutputPins)[i])
				stack.push_back((*lpEffectCtx->pOutputPins)[i]);

		stack.pop_front();
		if (stack.size()) lpEffectCtx = stack.front();
	}
}

#define PROCESS_EFFECT                                                                                           \
	if (!lpEffect->IsMultiThreadable() || (m_ThreadsCount == 1))                                                 \
	{                                                                                                            \
		if (lpEffect->IsInPlaceProcessing()) lpEffect->Process(lpEffectCtx->ppIn, 0, m_SharedCtx.nTotalSamples); \
		else lpEffect->Process(lpEffectCtx->ppIn, lpEffectCtx->ppOut, 0, m_SharedCtx.nTotalSamples);             \
	}                                                                                                            \
	else {                                                                                                       \
		m_SharedCtx.nProcessingThreads = 0;                                                                      \
		for (int i = 0; i < m_ThreadsCount; i++)                                                                 \
			m_pCtx[i].lpEffectCtx = lpEffectCtx;                                                                 \
                                                                                                                 \
		ReleaseSemaphore(m_SharedCtx.hSemaphore, m_ThreadsCount, NULL);                                          \
                                                                                                                 \
		WaitForSingleObject(m_SharedCtx.hCompletionEvent, INFINITE);                                             \
	}

void Nitrane::ProcessEffects()
{
	LARGE_INTEGER a1, a2;

	QueryPerformanceCounter(&a1);

	InitEffectsCtx();

	m_SharedCtx.nProcessingThreads = 0;
	ResetEvent(m_SharedCtx.hCompletionEvent);

	list<LPEFFECTCTX> stack;
	LPEFFECTCTX lpEffectCtx;

	stack.push_front(m_HeadEffect);

	for (lpEffectCtx = m_HeadEffect; stack.size();)
	{
		LPAUDIOEFFECT lpEffect = lpEffectCtx->lpEffect;

		int cIn = (int)lpEffectCtx->pInputPins->size();
		int cOut = (int)lpEffectCtx->pOutputPins->size();

		for (int i = 0; i < cIn; i++)
			if (lpEffectCtx->ppIn[i] == NULL)
				lpEffectCtx->ppIn[i] = m_pfNullBuffer;

		if (lpEffectCtx->bEnabled && lpEffect)
		{
			if ((cIn > 1) && (lpEffectCtx->npInputs != cIn))
				stack.push_back(lpEffectCtx); // Push back on the processing stack...
			else {
				// Process the effect using multithreading when possible.
				{ PROCESS_EFFECT }

				for (int i = 0; i < cOut; i++) {
					LPEFFECTCTX lpCtx = (*lpEffectCtx->pOutputPins)[i]; // Get the next pin...
					if (lpCtx) {
						int cNextIn = (int)lpCtx->pInputPins->size();
						if (cNextIn > 1)
							lpCtx->npInputs++;
					}
				}
			}
		}

		for (int i = 0; i < cOut; i++)
			if ((*lpEffectCtx->pOutputPins)[i])
				stack.push_back((*lpEffectCtx->pOutputPins)[i]);

		stack.pop_front();
		if (stack.size()) lpEffectCtx = stack.front();
	}

	QueryPerformanceCounter(&a2);
	m_fLatency = 0.9f * (1000.0f * (((float)(a2.QuadPart - a1.QuadPart)) / (float)m_liPerfFreq.QuadPart)) + 0.1f * m_fLatency;
}

float Nitrane::GetProcessingLatency()
{
	return (m_fLatency);
}
