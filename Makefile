CC = gcc
LD = gcc
PKG_CONFIG := pkg-config

CFLAGS = -Wall `$(PKG_CONFIG) --cflags glib-2.0 gtk+-3.0 gdk-3.0 x11`
LIBS   = `$(PKG_CONFIG) --libs glib-2.0 gtk+-3.0 gdk-3.0 x11` -ldvbrecorder -lsqlite3 -lpng

ifdef GSTREAMER010
	CFLAGS += `$(PKG_CONFIG) --cflags gstreamer-0.10 gstreamer-interfaces-0.10 gstreamer-base-0.10`
	LIBS   += `$(PKG_CONFIG) --libs gstreamer-0.10 gstreamer-interfaces-0.10 gstreamer-base-0.10`
else
	CFLAGS += `$(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-plugins-base-1.0 gstreamer-base-1.0`
	LIBS   += `$(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-plugins-base-1.0 gstreamer-base-1.0` -lgstvideo-1.0 -lgstaudio-1.0 -lgstapp-1.0
endif

ifdef RELEASE
	CFLAGS += -O2
else
	CFLAGS += -g
endif

RCVERSION := '$(shell git describe --tags --always) ($(shell git log --pretty=format:%cd --date=short -n1), branch \"$(shell git describe --tags --always --all | sed s:heads/::)\")'

APPNAME := dvb-recorder
PREFIX  := /usr

rc_SRC := $(wildcard *.c)
rc_OBJ := $(rc_SRC:.c=.o)
rc_HEADERS := $(wildcard *.h)

CFLAGS += -DRCVERSION=\"${RCVERSION}\"
CFLAGS += -DAPPNAME=\"${APPNAME}\"

all: $(APPNAME)

$(APPNAME): $(rc_OBJ)
	$(LD) -o $@ $^ $(CFLAGS) $(LIBS)

%.o: %.c $(rc_HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(APPNAME)
	install $(APPNAME) $(PREFIX)/bin

clean:
	rm -f $(APPNAME) $(rc_OBJ)

.PHONY: all clean install
