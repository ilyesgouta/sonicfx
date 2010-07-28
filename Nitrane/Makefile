
CC = g++
LD = g++

WINE_INCLUDES = /usr/include/wine/windows
OUTPUT = libnitrane.so

OBJ = Nitrane.o AudioEffect.o AudioDriver.o Utils.o

.cpp.o:
	$(CC) -c -fPIC -I. -I$(WINE_INCLUDES) -DLINUX -DDECLSPEC= $< -o $@

all: $(OBJ)
	$(LD) -shared $(OBJ) -o $(OUTPUT)

Utils.o:
	nasm -f elf Utils.asm -o $@

clean:
	@rm -f *.o *.so
