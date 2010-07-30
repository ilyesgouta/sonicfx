//
// Null effect for SonicFX
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

class __declspec(dllexport) NullAudioEffect : public AudioEffect {
public:
    NullAudioEffect();
    ~NullAudioEffect();

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
};

NullAudioEffect::NullAudioEffect()
{
}

NullAudioEffect::~NullAudioEffect()
{
}

BOOL NullAudioEffect::Initialize()
{
    return TRUE;
}

void NullAudioEffect::Finalize()
{
}

int NullAudioEffect::GetPreferredPacketSize()
{
    return 1024;
}

void NullAudioEffect::Configure(const AUDIODRIVERCAPS& caps)
{
}

void NullAudioEffect::Process(float** in, float** out, int offset, int samples)
{
    // Do nothing since we're doing a null in-place transformation.
}

void NullAudioEffect::Process(float** in, int offset, int samples)
{
    // Do nothing since we're doing a null in-place transformation.
}

const int NullAudioEffect::GetInputPinsCount() const
{
    return 1;
}

const int NullAudioEffect::GetOutputPinsCount() const
{
    return 1;
}

const char* NullAudioEffect::GetDescription()
{
    return "NullAudioEffect v0.1";
}

extern "C" __declspec(dllexport) void GetAudioEffectFactory(LPAUDIOEFFECT *lpAudioEffect)
{
    *lpAudioEffect = new NullAudioEffect();
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
