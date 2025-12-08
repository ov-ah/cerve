cerve: cerve.c
	gcc cerve.c -o bin/cerve
	mkdir -p bin/www
	cp -r www/* bin/www/

install: cerve
	cp bin/cerve /usr/local/bin/
	mkdir -p /usr/local/share/cerve
	cp -r bin/www/* /usr/local/share/cerve/

.PHONY: install
