
mq: mq.o repr.o
	g++ -o mq mq.o repr.o -lrt -lpthread

mq.o: mq.cpp repr.h
	g++ -c -I. -O2 -o mq.o mq.cpp

mq.cpp: mq-template.cpp README.md splice-readme
	./splice-readme mq-template.cpp README.md > mq.cpp

repr.o: repr.cpp repr.h
	g++ -c -I. -O2 -o repr.o repr.cpp

splice-readme: splice-readme.cpp repr.o
	g++ -I. -O2 -o splice-readme splice-readme.cpp repr.o

.PHONY: clean
clean:
	rm -f mq mq.cpp *.o splice-readme
