.PHONY:all
all: tcp_server tcp_client

tcp_server: server.cpp tcp_client
		g++ server.cpp -Wall -std=c++11 -o tcp_server -pthread -lstdc++
		
tcp_client: client.cpp
		g++ client.cpp -Wall -std=c++11 -o tcp_client -lstdc++


.PHONY:clean
clean:
	-rm -f tcp_server tcp_client

		
