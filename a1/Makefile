all: addpeer removepeer addcontent removecontent lookupcontent

addpeer: myaddpeer.cc
	g++ -o myaddpeer myaddpeer.cc

removepeer: removepeer.cc
	g++ -o removepeer removepeer.cc

addcontent: addcontent.cc
	g++ -o addcontent addcontent.cc

removecontent: removecontent.cc
	g++ -o removecontent removecontent.cc

lookupcontent: lookupcontent.cc
	g++ -o lookupcontent lookupcontent.cc

clean: 
	$(RM) myaddpeer removepeer addcontent removecontent lookupcontent
