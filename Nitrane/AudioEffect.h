//
// AudioEffect header file
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

#ifndef __AUDIOEFFECT__
#define __AUDIOEFFECT__

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
#endif

#include <vector>

using namespace std;

typedef enum {
	PARAM_SLIDER,
	PARAM_KNOB,
	PARAM_SWITCH
} PARAMETERTYPE;

typedef struct {
	PARAMETERTYPE type;
	float fMinimum;
	float fMaximum;
	float fStep;
	float fValue;
	TCHAR name[32];
} PARAMETERDESC, *LPPARAMETERDESC;

class DECLSPEC AudioEffect {
public:
	AudioEffect();
	virtual ~AudioEffect();

	virtual BOOL Initialize() = 0;
	virtual void Finalize() = 0;

	virtual BOOL IsMultiThreadable() = 0;
	virtual BOOL IsInPlaceProcessing() = 0;
	
	virtual void Configure(const AUDIODRIVERCAPS& caps) = 0;
	virtual int GetPreferredPacketSize() = 0;

	virtual void Process(float** in, float** out, int offset, int samples) = 0;
	virtual void Process(float** in, int offset, int samples) = 0;

	virtual const int GetParametersCount() { return 0; }
	virtual void GetParameterDesc(int id, LPPARAMETERDESC lpDesc);

	virtual void SetParameterValue(int id, float val) {}
	virtual float GetPrameterValue(int id) { return 0.0f; }

	virtual const char* GetDescription() = 0;

	virtual const int GetInputPinsCount() const = 0;
	virtual const int GetOutputPinsCount() const = 0;
};

typedef AudioEffect AUDIOEFFECT;
typedef AudioEffect *LPAUDIOEFFECT;

extern "C" typedef void (*LPAUDIOEFFECTFACTORY)(LPAUDIOEFFECT *lpAudioEffect);

#endif
