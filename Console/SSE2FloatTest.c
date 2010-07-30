
#include <windows.h>
#include <stdio.h>

float *floats;
short *shorts;

char buffer[128];

void short2float(short* in, float* out, int size);
void float2short(float* in, short* out, int size);

int main(int argc, char** argv)
{
    int i;
    unsigned int mask = 0x1F80;
    
    __asm {
        ldmxcsr mask;
        stmxcsr mask;
    }
    
    // set the rounding mode to round toward zero instead of the default round to nearest.
    mask |= (3L << 13);
    
    __asm {
        ldmxcsr mask;
    }
    
    floats = (float*) GlobalAlloc(GPTR, 32 * sizeof(float));
    shorts = (short*) GlobalAlloc(GPTR, 32 * sizeof(short));
    
    floats = (float*) (((unsigned int)floats + 15L) & ~15L);
    shorts = (short*) (((unsigned int)shorts + 15L) & ~15L);
        
    for (i = 0; i < 16; i++) {
        shorts[i] = (short)(i * 3.1415f);
        printf("reference: %f\n", (float)shorts[i]);
    }
    
    short2float(shorts, floats, 16);
    
    for (i = 0; i < 16; i++)
        printf("%f\n", floats[i]);
    
    printf("-----------------------\n");
    
    for (i = 0; i < 16; i++) {
        floats[i] = i * 3.1415f;
        printf("reference: %d\n", (short)floats[i]);
    }
    
    float2short(floats, shorts, 16);
    
    for (i = 0; i < 16; i++)
        printf("%d\n", shorts[i]);
    
    return 0;
}
