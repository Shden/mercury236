OPTIONS = -std=c99

all: mercury236 mercury-mon

mercury236: mercury-cli.c mercury236.c
	$(CC) $^ $(OPTIONS) -o $@

mercury-mon: mercury-mon.c mercury236.c
	$(CC) $^ $(OPTIONS) -o $@

clean:
	rm mercury236
	rm mercury-mon
