all: addpeer removepeer addcontent removecontent lookupcontent
CFLAGS = -std=c++0x -Wall -Wextra 


addpeer: addpeer.cc pickip.c partial.cc constants.h
	g++ $(CFLAGS) -o addpeer addpeer.cc pickip.c mybind.c partial.cc

removepeer: removepeer.cc
	g++ $(CFLAGS) -o removepeer removepeer.cc mybind.c

addcontent: addcontent.cc partial.cc constants.h
	g++ $(CFLAGS) -o addcontent addcontent.cc mybind.c partial.cc

removecontent: removecontent.cc constants.h
	g++ $(CFLAGS) -o removecontent removecontent.cc mybind.c

lookupcontent: lookupcontent.cc partial.cc constants.h
	g++ $(CFLAGS) -o lookupcontent lookupcontent.cc mybind.c partial.cc

clean: 
	$(RM) addpeer removepeer addcontent removecontent lookupcontent
