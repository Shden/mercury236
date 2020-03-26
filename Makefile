OPTIONS = -std=c99 -lpthread

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  OPTIONS += -lrt
endif

$(info $(OPTIONS))

all: mercury236 mercury-mon

mercury236: mercury-cli.c mercury236.c
	$(CC) $^ $(OPTIONS) -o $@

mercury-mon: mercury-mon.c mercury236.c
	$(CC) $^ $(OPTIONS) -o $@

clean:
	rm mercury236
	rm mercury-mon
