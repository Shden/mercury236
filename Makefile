OPTIONS = -std=c99

mercury236: mercury-cli.c mercury236.c
	$(CC) $^ $(OPTIONS) -o $@

clean:
	rm mercury236
