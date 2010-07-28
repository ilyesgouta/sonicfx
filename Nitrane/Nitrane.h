//
// Nitrane's common definitions header file
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

#ifndef __NITRANE__
#define __NITRANE__

#ifdef WIN32
#if !defined(DECLSPEC) && !defined(DECLSPECEX)

#if defined(DECLHOST)
#define DECLSPEC __declspec(dllexport)
#define DECLSPECEX
#elif defined(DECLCLIENT)
#define DECLSPEC __declspec(dllimport)
#define DECLSPECEX __declspec(dllexport)
#endif

#endif

#pragma warning( disable: 4251 )
#pragma warning( disable: 4996 )
#endif

#include <vector>
#include <list>

#ifdef WIN32
#include <hash_map>
#else
#include <ext/hash_map>
#endif

using namespace std;

#ifdef LINUX
using namespace __gnu_cxx;
#endif

struct PTCTX;
typedef struct PTCTX *LPPTCTX;

class Nitrane;
typedef class Nitrane *LPNITRANE;

typedef struct SHAREDCTX {
	volatile LPNITRANE lpNitrane;
	volatile LONG nProcessingThreads;
	HANDLE hCompletionEvent;
	HANDLE hSemaphore;
	CRITICAL_SECTION m_CS;
	int nTotalSamples;
	int idxOutputBuffer;
} SHAREDCTX, *LPSHAREDCTX;

typedef struct EFFECTCTX {
	EFFECTCTX(LPAUDIOEFFECT effect)
	{
		if (effect)
		{
			lpEffect = effect;
			effect->Initialize();

			ppIn = new float*[effect->GetInputPinsCount()];
			memset(ppIn, 0, effect->GetInputPinsCount() * sizeof(float*));

			ppOut = new float*[effect->GetOutputPinsCount()];
			memset(ppOut, 0, effect->GetOutputPinsCount() * sizeof(float*));

			pInputPins = new vector<EFFECTCTX*>(effect->GetInputPinsCount());
			pOutputPins = new vector<EFFECTCTX*>(effect->GetOutputPinsCount());

			bEnabled = TRUE;
		}
	}
	EFFECTCTX(int cInPins, int cOutPins)
	{
		lpEffect = NULL;
		ppIn = ppOut = NULL;

		if (cInPins) {
			ppIn = new float*[cInPins];
            memset(ppIn, 0, cInPins * sizeof(float*));
		}

		if (cOutPins) {
			ppOut = new float*[cOutPins];
            memset(ppOut, 0, cOutPins * sizeof(float*));
		}

		pInputPins = new vector<EFFECTCTX*>(cInPins);
		pOutputPins = new vector<EFFECTCTX*>(cOutPins);

		bEnabled = TRUE;
	}
	~EFFECTCTX()
	{
		if (lpEffect) lpEffect->Finalize();

		if (ppIn) delete [] ppIn;
		if (ppOut) delete [] ppOut;

		if (pInputPins) delete pInputPins;
		if (pOutputPins) delete pOutputPins;
	}
	LPAUDIOEFFECT lpEffect;
	vector<EFFECTCTX*> *pInputPins;
	vector<EFFECTCTX*> *pOutputPins;
	float **ppIn;
	float **ppOut;
	hash_map<int, int> pinOut;
	int npInputs;
	BOOL bEnabled;
} EFFECTCTX, *LPEFFECTCTX;

template class DECLSPEC vector<LPAUDIODRIVER>;
template class DECLSPEC vector<LPAUDIOEFFECTFACTORY>;

class DECLSPEC Nitrane {
public:
	Nitrane();
	~Nitrane();

	const vector<LPAUDIODRIVER>& EnumAudioDrivers();
	const vector<LPAUDIOEFFECTFACTORY>& EnumAudioEffects();

	BOOL Initialize(char* path, int maxThreads = 1);
	void Finalize();

	void SetAudioDriver(LPAUDIODRIVER lpAudioDriver);

	BOOL LoadGraph(char* fn);
	BOOL SaveGraph(char* fn);

	BOOL Start();
	void Stop();

	BOOL ConnectEffect(LPEFFECTCTX lpFrom, LPEFFECTCTX lpTo, int idxOutPin, int idxInPin);
	
	void DisconnectEffect(LPEFFECTCTX lpEffect);
	void DisconnectEffect(LPEFFECTCTX lpEffect, int idxOutPin);

	void ConfigureEffect(LPEFFECTCTX lpEffect);
	void BypassEffect(LPEFFECTCTX lpEffect, BOOL bEnable);

	float GetProcessingLatency();

private:
	vector<LPAUDIODRIVER> m_AudioDrivers;
	vector<LPAUDIOEFFECTFACTORY> m_AudioEffects;
	vector<HMODULE> m_hModules;

	LPEFFECTCTX m_HeadEffect, m_TailEffect;

	LONG m_ThreadsCount;
	int m_IOPacketSize;

	volatile BOOL m_bProcess; // Global processing switch.
	volatile BOOL m_bSuspended;

	HANDLE m_hSuspendEvent, m_hSuspendEventACK;

	LPHANDLE m_phSlaveThread;
	HANDLE m_hMasterThread;
    LPPTCTX m_pCtx;

	SHAREDCTX m_SharedCtx;
	float* m_pfNullBuffer;

	LPAUDIODRIVER m_lpAudioDriver;
	AUDIODRIVERCAPS m_AudioCaps;

	int NegotiatePacketSize();

	hash_map<unsigned int, LPVOID> m_lpBuffers; // We're not 64bit ready.

	float* AllocateBuffer();
	BOOL AllocateBuffers();
	void ReconnectBuffers();
	void InitEffectsCtx();
	void ConfigureEffects();
	void ProcessEffects();
	void SuspendProcessing();
	void ResumeProcessing();

	float m_fLatency;
	LARGE_INTEGER m_liPerfFreq;

	friend DWORD WINAPI MasterThread(LPVOID lpData);
	friend DWORD WINAPI SlaveThread(LPVOID lpData);
};

typedef struct PTCTX {
	LPEFFECTCTX lpEffectCtx;
	LPSHAREDCTX lpSharedCtx;
	volatile BOOL *bRun;
    int idxOffset;
	int nSamples;
} PTCTX;

typedef Nitrane *LPNITRANEINSTANCE;

#endif
