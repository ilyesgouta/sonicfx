//
// Dumps the received audio samples into a file before forwarding them to the output pin
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

#include <AudioDriver.h>
#include <AudioEffect.h>

class __declspec(dllexport) Dumper : public AudioEffect {
public:
	Dumper();
	~Dumper();

	BOOL Initialize();
	void Finalize();

	BOOL IsMultiThreadable() { return FALSE; }
	BOOL IsInPlaceProcessing() { return TRUE; }
	
	int GetPreferredPacketSize();
	void Configure(const AUDIODRIVERCAPS& caps);

	void Process(float** in, float** out, int offset, int samples);
	void Process(float** in, int offset, int samples);
	
	const char* GetDescription();

	const int GetInputPinsCount() const;
	const int GetOutputPinsCount() const;

private:
	HANDLE hFile;
	short *m_pSamples, *m_pAlignedSamples;
};

Dumper::Dumper()
{
	hFile = INVALID_HANDLE_VALUE;
	m_pSamples = m_pAlignedSamples = NULL;
}

Dumper::~Dumper()
{
}

BOOL Dumper::Initialize()
{
	SYSTEMTIME Time;
	TCHAR buffer[256];

	GetLocalTime(&Time);
	wsprintf(buffer, L"dump-%02d%02d-%04d%02d%02d.raw", Time.wHour, Time.wMinute, Time.wYear, Time.wMonth, Time.wDay);

	hFile = CreateFile(buffer, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING, NULL);

	m_pSamples = (short*)GlobalAlloc(GPTR, 512 + 15);
	m_pAlignedSamples = (short*)(((unsigned int)m_pSamples + 15L) & ~15L);

	return TRUE;
}

void Dumper::Finalize()
{
	if (hFile != INVALID_HANDLE_VALUE) {
		CloseHandle(hFile);
		hFile = INVALID_HANDLE_VALUE;
	}

	if (m_pSamples) {
		GlobalFree(m_pSamples);
		m_pSamples = NULL;
	}
}

int Dumper::GetPreferredPacketSize()
{
	return 512;
}

void Dumper::Configure(const AUDIODRIVERCAPS& caps)
{
}

void Dumper::Process(float** in, float** out, int offset, int samples)
{
}

static __inline void float2short(float* pSrc, short* pDst, int samples)
{
	while (samples--) {
		*pDst++ = (short)*pSrc++;
	}
}

void Dumper::Process(float** in, int offset, int samples)
{
	DWORD dwWBytes;

	if (hFile != INVALID_HANDLE_VALUE) {
		float2short(in[0] + offset, m_pAlignedSamples, samples);
		WriteFile(hFile, m_pAlignedSamples, samples * sizeof(short), &dwWBytes, NULL);
	}
}

const int Dumper::GetInputPinsCount() const
{
	return 1;
}

const int Dumper::GetOutputPinsCount() const
{
	return 1;
}

const char* Dumper::GetDescription()
{
	return "Dumper v0.1";
}

extern "C" __declspec(dllexport) void GetAudioEffectFactory(LPAUDIOEFFECT *lpAudioEffect)
{
	*lpAudioEffect = new Dumper();
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
