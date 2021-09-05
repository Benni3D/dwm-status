CC ?= cc
CFLAGS += -O3 -Wall -Wextra -pthread -D_XOPEN_SOURCE=700 -std=c99 -Wno-unused-function

LIBS = -lX11

ifeq ($(NVIDIA_GPU),1)
CFLAGS += -I/opt/cuda/include -DHAVE_NVIDIA_GPU=1
LIBS += -L/opt/cuda/lib64 -lnvidia-ml
endif

PROG=dwm-status

all: $(PROG)

$(PROG): $(PROG).c config.h
	$(CC) -o $@ $< $(CFLAGS) $(LIBS)

clean:
	rm -f $(PROG)

install: $(PROG)
	install -m755 $< /usr/local/bin/dwm-status

.PHONY: all clean install
