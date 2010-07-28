//
// AudioEffect class implementation
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
// Notes: Each AudioEffect has m input pins and n output pins. These are defined in each derived class.
// The method Process takes a float** as input which points to a float buffer per input pin.
// That method process the data and writes back the result in output buffers which are pointed to by
// the second parameter. Audio buffers are allocated and managed by Nitrane.
//

#include <windows.h>

#include "AudioDriver.h"
#include "AudioEffect.h"

AudioEffect::AudioEffect()
{
}

AudioEffect::~AudioEffect()
{
}

void AudioEffect::GetParameterDesc(int id, LPPARAMETERDESC lpDesc)
{
}
