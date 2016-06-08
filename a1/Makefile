all: addpeer removepeer addcontent removecontent lookupcontent

addpeer: addpeer.cc
	g++ -o addpeer addpeer.cc

removepeer: removepeer.cc
	g++ -o removepeer removepeer.cc

addcontent: addcontent.cc
	g++ -o addcontent addcontent.cc

removecontent: removecontent.cc
	g++ -o removecontent removecontent.cc

lookupcontent: lookupcontent.cc
	g++ -o lookupcontent lookupcontent.cc

clean: 
	$(RM) addpeer removepeer addcontent removecontent lookupcontent
