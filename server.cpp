/* Network Libraries */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
/* Standard Libraries */
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
/* cpp libraries */
#include <string>


/* Define Constants */
#define CONNECT_BUFFER 10 // Number of pending connections to hold in buffer

class Socket {
  public:
    int sockfd;
    std::string listen_port;
    struct addrinfo addr_info;
    struct addrinfo *server_info;
    void create ( void );
    int acceptConnections( void );
    Socket( int port );
    ~Socket( void );
  private:
    struct sockaddr_in remote_addr;
    socklen_t addr_size;
    int createSocket( void );
    int bindAndListen ( void );
    int handleSession ( int sessionfd );
    void createSession( int sessionfd );
};

Socket::Socket( int port ) {
  listen_port = std::to_string(port);
  memset(&addr_info, 0, sizeof addr_info);
  addr_info.ai_family = AF_INET;
  addr_info.ai_socktype = SOCK_STREAM;
}

Socket::~Socket(void) {
  freeaddrinfo(server_info);
  shutdown(sockfd, SHUT_RDWR);
  close(sockfd);
}

void Socket::create(void) {
  int status = 0;
  if ((status = getaddrinfo(NULL, listen_port.c_str(), &addr_info, &server_info)) != 0){
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
  sockfd = createSocket();
  bindAndListen();
}

int Socket::createSocket(void){
  sockfd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
  if (sockfd == -1){
    fprintf(stderr, "socket error: %s\n", gai_strerror(sockfd));
    exit(1);
  }
  return sockfd;
}

int Socket::bindAndListen(void){
  int res = bind(sockfd, server_info->ai_addr, server_info->ai_addrlen);
  if (res != 0){
    printf("Bind error - errno: %d\n", errno);
    exit(1);
  }
  res = listen(sockfd, CONNECT_BUFFER);
  if (res != 0){
    printf("Listen error - errno: %d\n", errno);
    exit(1);
  }
  printf("Listening for incoming connections on port %s\n", listen_port.c_str());
  return res;
}

int Socket::handleSession(int sessionfd){
  char data[512];
  fd_set sess_fdset;
  struct timeval timeout;
  int fd_ready, byte_count;
  FD_ZERO(&sess_fdset);
  while(1){
    FD_SET(sessionfd, &sess_fdset);
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;
    fd_ready = select(sessionfd+1, &sess_fdset, NULL, NULL, &timeout);
    if (fd_ready == -1){
      printf("Select error - errno: %d\n", errno);
      close(sessionfd);
      exit(1);
    }
    else if (fd_ready == 0){
      printf("Connection timed out\n");
      close(sessionfd);
      exit(1);
    }
    else if (FD_ISSET(sessionfd, &sess_fdset)){
      byte_count = read(sessionfd, data, sizeof(data));
      if (byte_count == 0){
        printf("Connection closed by peer\n");
        close(sessionfd);
        exit(0);
      }
      else{
        data[byte_count] = '\0';
        printf("Data: %s", data);
      }
    }
  }
}

void Socket::createSession(int sessionfd){
  pid_t new_fork = fork();
  
  if (new_fork == -1){
    printf("Failed to create process to handle connection from %s\n", inet_ntoa(remote_addr.sin_addr));
  }
  else if (new_fork == 0){
    close(sockfd);
    handleSession(sessionfd);
  }
  else{
    printf("Created process %d to serve %s\n", new_fork, inet_ntoa(remote_addr.sin_addr));
    close(sessionfd);
  }
}

int Socket::acceptConnections(void){
  int sessionfd;

  sessionfd = accept(sockfd, (struct sockaddr *)&remote_addr, &addr_size);
  if(sessionfd == -1) {
  	printf("accept error!\n");
	return -1;
  }
  
  printf("Accepted new connection from %s\n", inet_ntoa(remote_addr.sin_addr));
  createSession(sessionfd);
  return 0;
}

int main(void){
  struct sigaction act;   
  act.sa_handler = SIG_IGN;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  // To avoid zombie process
  sigaction(SIGCHLD, &act, NULL);

  int port = 55555;
  Socket tcp_socket(port);
  tcp_socket.create();

  while(1){
    tcp_socket.acceptConnections();
  }
  
  return 0;
}
