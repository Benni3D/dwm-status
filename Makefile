CC ?= cc
CFLAGS += -O3 -Wall -Wextra -pthread

#CFLAGS += -I/opt/cuda/include -DHAVE_NVIDIA_GPU=1
#LIBS += -L/opt/cuda/lib64 -lnvidia-ml

PROG=dwm-status

all: $(PROG)

$(PROG): $(PROG).c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS)

clean:
	rm -f $(PROG)

install: $(PROG)
	install -m755 $< /usr/local/bin/dwm-status

.PHONY: all clean install
