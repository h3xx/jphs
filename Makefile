
HDOBJECTS = jphide.o bf.o
SKOBJECTS = jpseek.o bf.o

CFLAGS = -I./jpeg-8a -DBlowfish_Encrypt=_N_Blowfish_Encrypt -DBlowfish_Decrypt=_N_Blowfish_Decrypt -O2

all: jphide jpseek

jphide: $(HDOBJECTS)
	$(CC) $(LDFLAGS) -o jphide $(HDOBJECTS) -ljpeg $(LDLIBS)

jpseek: $(SKOBJECTS)
	$(CC) $(LDFLAGS) -o jpseek $(SKOBJECTS) -ljpeg $(LDLIBS)

bf.o: bf.c
	$(CC) -O2 -I/usr/src/linux/include -I/usr/src/linux/arch/x86/include -c -o bf.o bf.c

clean:
	$(RM) jphide jpseek *.o

#jphide.o: jphide.c cdjpeg.h jinclude.h jconfig.h jpeglib.h jmorecfg.h jerror.h cderror.h jversion.h ltable.h
#jpseek.o: jpseek.c cdjpeg.h jinclude.h jconfig.h jpeglib.h jmorecfg.h jerror.h cderror.h jversion.h ltable.h
