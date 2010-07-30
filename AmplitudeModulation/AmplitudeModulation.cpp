//
// A simple sin(wt) amplitude modulation effect for SonicFX
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

class __declspec(dllexport) AmplitudeModulation : public AudioEffect {
public:
    AmplitudeModulation();
    ~AmplitudeModulation();

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
    float m_Time;
    int m_ModulationFreq;
    float m_SamplingFreq;
};

AmplitudeModulation::AmplitudeModulation()
{
    m_ModulationFreq = 30; // 30 Hz
    m_Time = 0;
}

AmplitudeModulation::~AmplitudeModulation()
{
}

BOOL AmplitudeModulation::Initialize()
{
    return TRUE;
}

void AmplitudeModulation::Finalize()
{
}

int AmplitudeModulation::GetPreferredPacketSize()
{
    return 512;
}

void AmplitudeModulation::Configure(const AUDIODRIVERCAPS& caps)
{
    m_SamplingFreq = 1.0f / caps.nSamplingRate;
    printf("AmplitudeModulation: delta == %f\n", m_SamplingFreq);
}

void AmplitudeModulation::Process(float** in, float** out, int offset, int samples)
{
}

void AmplitudeModulation::Process(float** in, int offset, int samples)
{
    float* pSrc = in[0] + offset;

    for (int i = 0; i < samples; i++) {
        pSrc[i] *= sin(2 * 3.1415926f * m_ModulationFreq * m_Time);
        m_Time += m_SamplingFreq;
    }
}

const int AmplitudeModulation::GetInputPinsCount() const
{
    return 1;
}

const int AmplitudeModulation::GetOutputPinsCount() const
{
    return 1;
}

const char* AmplitudeModulation::GetDescription()
{
    return "AmplitudeModulation v0.1";
}

extern "C" __declspec(dllexport) void GetAudioEffectFactory(LPAUDIOEFFECT *lpAudioEffect)
{
    *lpAudioEffect = new AmplitudeModulation();
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
