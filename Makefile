all: ashtier

ashtier:
	gcc -Wall ashtier.c -lm -o ashtier

clean:
	rm -f ashtier
