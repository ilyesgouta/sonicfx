//
// Console-based testbed for Nitrane
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
#include <stdio.h>
#include <assert.h>

#include <AudioDriver.h>
#include <AudioEffect.h>
#include <Nitrane.h>

LPNITRANEINSTANCE g_NitraneInst;

extern "C" __declspec(dllimport) LPNITRANEINSTANCE CreateInstance();

int main(int argc, char** argv)
{
	g_NitraneInst = CreateInstance();
	
	g_NitraneInst->Initialize(NULL, 1);
	
	for (vector <LPAUDIODRIVER>::const_iterator it1 = g_NitraneInst->EnumAudioDrivers().begin(); it1 != g_NitraneInst->EnumAudioDrivers().end(); it1++)
		printf("%s\n", (*it1)->GetDescription());
	
	for (vector <LPAUDIOEFFECTFACTORY>::const_iterator it2 = g_NitraneInst->EnumAudioEffects().begin(); it2 != g_NitraneInst->EnumAudioEffects().end(); it2++) {
		LPAUDIOEFFECT lpEffect;
		(*it2)(&lpEffect); // Create a new instance
		printf("%s\n", lpEffect->GetDescription());
		delete lpEffect;
	}
	
	vector <LPAUDIODRIVER>::const_iterator DriversItr = g_NitraneInst->EnumAudioDrivers().begin();
	
	LPAUDIODRIVER lpAudioDriver = (*DriversItr);
	g_NitraneInst->SetAudioDriver(lpAudioDriver); // Select the first driver.

	lpAudioDriver->SetQueueSize(1); // 1 packet for prebuffering

	vector<LPAUDIOEFFECTFACTORY>::const_iterator EffectItr = g_NitraneInst->EnumAudioEffects().begin();
	
	LPAUDIOEFFECT lpEffect1, lpEffect2;

	(*EffectItr)(&lpEffect1);
	(*EffectItr)(&lpEffect2);

	LPEFFECTCTX lpCtx1 = new EFFECTCTX(lpEffect1); // Who has to manage these? Nitrane or the application?
	LPEFFECTCTX lpCtx2 = new EFFECTCTX(lpEffect2);

	g_NitraneInst->ConnectEffect(NULL, lpCtx1, 0, 0);
	//g_NitraneInst->ConnectEffect(lpCtx1, lpCtx2, 0, 0);
	//g_NitraneInst->ConnectEffect(lpCtx2, NULL, 0, 0);
	g_NitraneInst->ConnectEffect(lpCtx1, NULL, 0, 0);

	//lpEffect1->SetParameterValue(0, 10.0f);

	printf("Starting...\n");
	assert(g_NitraneInst->Start() == TRUE);

	printf("Sleeping for 30 seconds...\n");
	int g_Timer = 30;

	while (g_Timer--) {
		printf("Processing latency: %.03f ms.\n", g_NitraneInst->GetProcessingLatency() + lpAudioDriver->GetPacketLatency());
        Sleep(1000);
	}

	printf("Shutting down...\n");
	g_NitraneInst->Stop();
	
	delete g_NitraneInst;
	printf("Done.\n");

	return 0;
}
