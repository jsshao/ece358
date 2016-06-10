all: addpeer removepeer addcontent removecontent lookupcontent

addpeer: addpeer.cc pickip.c
	g++ -std=c++0x -o addpeer addpeer.cc pickip.c mybind.c partial.cc

removepeer: removepeer.cc
	g++ -std=c++0x -o removepeer removepeer.cc mybind.c

addcontent: addcontent.cc
	g++ -std=c++0x -o addcontent addcontent.cc mybind.c partial.cc

removecontent: removecontent.cc
	g++ -std=c++0x -o removecontent removecontent.cc mybind.c

lookupcontent: lookupcontent.cc
	g++ -std=c++0x -o lookupcontent lookupcontent.cc mybind.c

clean: 
	$(RM) addpeer removepeer addcontent removecontent lookupcontent
