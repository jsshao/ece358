all: addpeer removepeer addcontent removecontent lookupcontent
CFLAGS = -std=c++0x -Wall -Wextra 


addpeer: addpeer.cc pickip.c partial.cc
	g++ $(CFLAGS) -o addpeer addpeer.cc pickip.c mybind.c partial.cc

removepeer: removepeer.cc
	g++ $(CFLAGS) -o removepeer removepeer.cc mybind.c

addcontent: addcontent.cc partial.cc
	g++ $(CFLAGS) -o addcontent addcontent.cc mybind.c partial.cc

removecontent: removecontent.cc
	g++ $(CFLAGS) -o removecontent removecontent.cc mybind.c

lookupcontent: lookupcontent.cc
	g++ $(CFLAGS) -o lookupcontent lookupcontent.cc mybind.c

clean: 
	$(RM) addpeer removepeer addcontent removecontent lookupcontent
