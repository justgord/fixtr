
all : fixtr fixspec

fixtr : fixcore.h fixcore.cpp fixtr.cpp
	g++ -Wall -I/usr/include/libxml2  -lxml2 fixcore.cpp fixtr.cpp -o fixtr

fixspec : fixcore.h fixcore.cpp fixspec.cpp
	g++ -Wall -I/usr/include/libxml2  -lxml2 fixcore.cpp fixspec.cpp -o fixspec

clean: 
	rm -f fixtr fixspec
