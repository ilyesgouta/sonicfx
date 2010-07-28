
# Author: Ilyes Gouta
# This Makefile will invoke make for all the sub-folders.

folders = Nitrane \
          AmplitudeModulation \
          SimpleLowPass \
          Dumper \
          NullDriver \
          NullEffect \
          SimpleGain \
          ToneGenerator

all:
	@for i in $(folders) ; do make -C $$i ; done

clean:
	@for i in $(folders) ; do make -C $$i clean ; done

