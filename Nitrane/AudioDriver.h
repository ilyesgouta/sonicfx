//
// AudioDriver header file
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

#ifndef __AUDIODRIVER__
#define __AUDIODRIVER__

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
#endif

typedef struct AUDIODRIVERCAPS {
    int nPacketSize;
    int nSamplingRate;
    int nPackets;
    short **lpPlaybackBuffer;
    short **lpCaptureBuffer;
    LPHANDLE hpPacketEvent;
} AUDIODRIVERCAPS, *LPAUDIODRIVERCAPS;

class DECLSPEC AudioDriver {
public:
    AudioDriver();
    virtual ~AudioDriver();

    virtual BOOL Open(LPAUDIODRIVERCAPS lpAudioDriverCaps) = 0;
    virtual void Close(LPAUDIODRIVERCAPS lpAudioDriverCaps) = 0;

    virtual BOOL Start() = 0;
    virtual void Stop() = 0;

    virtual void Write(int idx) = 0;

    virtual void SetQueueSize(int cPackets) = 0;
    virtual float GetPacketLatency() = 0;

    virtual const char* GetDescription() = 0;
};

typedef class AudioDriver AUDIODRIVER;
typedef class AudioDriver *LPAUDIODRIVER;

typedef void (*LPAUDIODRIVERFACTORY)(LPAUDIODRIVER *lpAudioDriver);

#endif
