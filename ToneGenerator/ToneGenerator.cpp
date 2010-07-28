//
// A tone generator for SonicFX
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
#include <math.h>

#include <AudioDriver.h>
#include <AudioEffect.h>

class __declspec(dllexport) ToneGenerator : public AudioEffect {
public:
	ToneGenerator();
	~ToneGenerator();

	BOOL Initialize();
	void Finalize();

	BOOL IsMultiThreadable() { return FALSE; }
	BOOL IsInPlaceProcessing() { return FALSE; }
	
	int GetPreferredPacketSize();
	void Configure(const AUDIODRIVERCAPS& caps);

	void Process(float** in, float** out, int offset, int samples);
	void Process(float** in, int offset, int samples);
	
	const char* GetDescription();

	const int GetInputPinsCount() const;
	const int GetOutputPinsCount() const;

private:
	float m_fTime;
	float m_fFrequency;
	int m_SamplingFreq;
};

ToneGenerator::ToneGenerator()
{
	m_fTime = 0.0f;
	m_fFrequency = 600;
}

ToneGenerator::~ToneGenerator()
{
}

BOOL ToneGenerator::Initialize()
{
	return TRUE;
}

void ToneGenerator::Finalize()
{
}

int ToneGenerator::GetPreferredPacketSize()
{
	return 512;
}

void ToneGenerator::Configure(const AUDIODRIVERCAPS& caps)
{
	m_SamplingFreq = caps.nSamplingRate;
}

void ToneGenerator::Process(float** in, float** out, int offset, int samples)
{
	float *pSample = out[0];

	for (int i = 0; i < samples; i++) {
		*pSample++ = 16384.0f * sin(2 * 3.1415f * m_fFrequency * m_fTime);
		m_fFrequency = 600 + 200 * sin(2 * 3.1415f * 10 * m_fTime); // Sweep period is 5 seconds.
		m_fTime += 1.0f / m_SamplingFreq;
	}
}

void ToneGenerator::Process(float** in, int offset, int samples)
{
}

const int ToneGenerator::GetInputPinsCount() const
{
	return 1;
}

const int ToneGenerator::GetOutputPinsCount() const
{
	return 1;
}

const char* ToneGenerator::GetDescription()
{
	return "ToneGenerator v0.1";
}

extern "C" __declspec(dllexport) void GetAudioEffectFactory(LPAUDIOEFFECT *lpAudioEffect)
{
	*lpAudioEffect = new ToneGenerator();
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
