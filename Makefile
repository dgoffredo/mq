
mq: mq.cpp
	g++ -O2 -o mq mq.cpp -lrt -lpthread

mq.cpp: mq_template.cpp README.md
	python splice-readme.py mq_template.cpp README.md > mq.cpp
