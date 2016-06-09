all: addpeer removepeer addcontent removecontent lookupcontent

addpeer: addpeer.cc pickip.c
	g++ -std=c++0x -o addpeer addpeer.cc pickip.c mybind.c

removepeer: removepeer.cc
	g++ -std=c++0x -o removepeer removepeer.cc

addcontent: addcontent.cc
	g++ -std=c++0x -o addcontent addcontent.cc

removecontent: removecontent.cc
	g++ -std=c++0x -o removecontent removecontent.cc

lookupcontent: lookupcontent.cc
	g++ -std=c++0x -o lookupcontent lookupcontent.cc

clean: 
	$(RM) addpeer removepeer addcontent removecontent lookupcontent
