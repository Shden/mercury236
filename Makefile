OPTIONS = -std=c99

mercury236: mercury.c mercury236.c
	$(CC) $^ $(OPTIONS) -o $@

clean:
	rm mercury236
