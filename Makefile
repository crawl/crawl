all: ashtier

ashtier: ashtier.c
	gcc -Wall ashtier.c -lm -o ashtier

clean:
	rm -f ashtier
