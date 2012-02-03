# variables
HDOBJECTS = jphide.o bf.o
SKOBJECTS = jpseek.o bf.o

## flags
JP_CFLAGS = -O2 \
	    -I./jpeg-8a \
	    -DBlowfish_Encrypt=_N_Blowfish_Encrypt \
	    -DBlowfish_Decrypt=_N_Blowfish_Decrypt
BF_CFLAGS = -O2 \
	    -I/usr/src/linux/include \
	    -I/usr/src/linux/arch/x86/include

LIBS = -ljpeg
LDFLAGS = $(LIBS)

## programs
INSTALL = install
INSTALL_DIR = $(INSTALL) -d -m 0755
INSTALL_BIN = $(INSTALL) -m 0755
INSTALL_DATA = $(INSTALL) -m 0644

## install paths
PREFIX = /usr
BINDIR = $(PREFIX)/bin

# targets
TARGETS = jphide jpseek
all: $(TARGETS)
jphide: $(HDOBJECTS)
jpseek: $(SKOBJECTS)

# object rules
bf.o:			CFLAGS=$(BF_CFLAGS)
jphide.o jpseek.o:	CFLAGS=$(JP_CFLAGS)

# dependencies
bf.c: bf.h
jphide.c: ltable.h version.h bf.h
jpseek.c: ltable.h version.h bf.h

# other targets
clean:
	$(RM) \
		$(TARGETS) \
		$(HDOBJECTS) \
		$(SKOBJECTS)

install: all
	$(INSTALL_DIR) $(DESTDIR)$(BINDIR)
	$(INSTALL_BIN) $(TARGETS) $(DESTDIR)$(BINDIR)

.PHONY: all clean install

#jphide.o: jphide.c cdjpeg.h jinclude.h jconfig.h jpeglib.h jmorecfg.h jerror.h cderror.h jversion.h ltable.h
#jpseek.o: jpseek.c cdjpeg.h jinclude.h jconfig.h jpeglib.h jmorecfg.h jerror.h cderror.h jversion.h ltable.h
