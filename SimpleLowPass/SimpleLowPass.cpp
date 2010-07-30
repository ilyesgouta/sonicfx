//
// A simple low pass filter for SonicFX
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

class __declspec(dllexport) SimpleLowPass : public AudioEffect {
public:
    SimpleLowPass();
    ~SimpleLowPass();

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
    float m_fAlpha;
};

SimpleLowPass::SimpleLowPass()
{
    m_fAlpha = 0.5f;
}

SimpleLowPass::~SimpleLowPass()
{
}

BOOL SimpleLowPass::Initialize()
{
    return TRUE;
}

void SimpleLowPass::Finalize()
{
}

int SimpleLowPass::GetPreferredPacketSize()
{
    return 512;
}

void SimpleLowPass::Configure(const AUDIODRIVERCAPS& caps)
{
}

void SimpleLowPass::Process(float** in, float** out, int offset, int samples)
{
}

void SimpleLowPass::Process(float** in, int offset, int samples)
{
    static float fPrev = in[0][0];
    float *pSrc = in[0] + offset;

    for (int i = 0; i < samples; i++) {
        pSrc[i] = m_fAlpha * pSrc[i] + (1.0f - m_fAlpha) * fPrev;
        fPrev = pSrc[i];
    }
}

const int SimpleLowPass::GetInputPinsCount() const
{
    return 1;
}

const int SimpleLowPass::GetOutputPinsCount() const
{
    return 1;
}

const char* SimpleLowPass::GetDescription()
{
    return "SimpleLowPass v0.1";
}

extern "C" __declspec(dllexport) void GetAudioEffectFactory(LPAUDIOEFFECT *lpAudioEffect)
{
    *lpAudioEffect = new SimpleLowPass();
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
