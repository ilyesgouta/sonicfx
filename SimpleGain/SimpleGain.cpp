//
// A simple variable gain effect for SonicFX
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

class __declspec(dllexport) SimpleGain : public AudioEffect {
public:
    SimpleGain();
    ~SimpleGain();

    BOOL Initialize();
    void Finalize();

    BOOL IsMultiThreadable() { return TRUE; }
    BOOL IsInPlaceProcessing() { return TRUE; }
    
    int GetPreferredPacketSize();
    void Configure(const AUDIODRIVERCAPS& caps);

    const int GetParametersCount();
    void GetParameterDesc(int id, LPPARAMETERDESC lpDesc);

    void SetParameterValue(int id, float val);
    float GetPrameterValue(int id);

    void Process(float** in, float** out, int offset, int samples);
    void Process(float** in, int offset, int samples);
    
    const char* GetDescription();

    const int GetInputPinsCount() const;
    const int GetOutputPinsCount() const;

private:
    float m_fGain;
};

SimpleGain::SimpleGain()
{
    m_fGain = 1.0f;
}

SimpleGain::~SimpleGain()
{
}

BOOL SimpleGain::Initialize()
{
    return TRUE;
}

void SimpleGain::Finalize()
{
}

int SimpleGain::GetPreferredPacketSize()
{
    return 512;
}

const int SimpleGain::GetParametersCount()
{
    return 1;
}

void SimpleGain::GetParameterDesc(int id, LPPARAMETERDESC lpDesc)
{
    lpDesc->fMinimum = 0.0f;
    lpDesc->fMaximum = 256.0f;
    lpDesc->fStep = 1.0f;
    lpDesc->fValue = m_fGain;
    lstrcpy(lpDesc->name, L"Gain");
    lpDesc->type = PARAM_KNOB;
}

void SimpleGain::SetParameterValue(int id, float val)
{
    m_fGain = val;
}

float SimpleGain::GetPrameterValue(int id)
{
    return (m_fGain);
}

void SimpleGain::Configure(const AUDIODRIVERCAPS& caps)
{
}

void SimpleGain::Process(float** in, float** out, int offset, int samples)
{
}

void SimpleGain::Process(float** in, int offset, int samples)
{
    float *pSample = in[0] + offset;

    for (int i = 0; i < samples; i++)
        *pSample++ *= m_fGain;
}

const int SimpleGain::GetInputPinsCount() const
{
    return 1;
}

const int SimpleGain::GetOutputPinsCount() const
{
    return 1;
}

const char* SimpleGain::GetDescription()
{
    return "Simple Gain Effect v0.1";
}

extern "C" __declspec(dllexport) void GetAudioEffectFactory(LPAUDIOEFFECT *lpAudioEffect)
{
    *lpAudioEffect = new SimpleGain();
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
