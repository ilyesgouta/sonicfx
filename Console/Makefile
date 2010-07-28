
CC = g++
LD = g++

WINE_INCLUDES = /usr/include/wine/windows
WINE_LIBS = /usr/lib/wine

OUTPUT = Console

OBJ = Main.o

.cpp.o:
	$(CC) -c -fPIC -I. -I../Nitrane -I$(WINE_INCLUDES) -DLINUX -DDECLSPEC= $< -o $@

all: $(OBJ)
	$(LD) -L../Nitrane -L$(WINE_LIBS) -lnitrane -lwine /usr/lib/wine/kernel32.dll.so /usr/lib/wine/user32.dll.so $(OBJ) -o $(OUTPUT)
clean:
	@rm -f *.o Console
