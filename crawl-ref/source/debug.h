#define ASSERT(x)	do if (!(x)) {fprintf(stderr, "ASSERT FAILED: %s\n", #x); exit(1);} while(0)
#define DEBUGSTR(x)	printf("%s\n", (x))
