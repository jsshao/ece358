all: myaddpeer removepeer addcontent removecontent lookupcontent

myaddpeer: myaddpeer.cc pickip.c
	g++ -o myaddpeer myaddpeer.cc pickip.c

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
