cerve: cerve.c
	gcc cerve.c -o bin/cerve

install: cerve
	cp bin/cerve /usr/local/bin/

.PHONY: install
