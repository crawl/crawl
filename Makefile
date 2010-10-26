HEADERS = $(shell git ls-files|grep "\.h$$")
SOURCES = $(shell git ls-files|grep "\.cc$$")
CXX = g++ -Wall

all:	layout

clean:
	rm -f *.o layout

%.o: %.cc $(HEADERS)
	$(CXX) -c -o $@ $<

layout:	$(SOURCES:.cc=.o)
	$(CXX) -o layout $^
