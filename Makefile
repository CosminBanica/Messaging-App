CC=g++ -std=c++11 -Wall -Wextra

build:
	g++ -std=c++11 -Wall -Wextra server.cpp -o server
	g++ -std=c++11 -Wall -Wextra subscriber.cpp -o subscriber
clean:
	rm *.o server subscriber
