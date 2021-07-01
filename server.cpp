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
#include <stdlib.h>
/* cpp libraries */
#include <string>

/* Define Constants */
#define CONNECT_BUFFER 10 // Number of pending connections to hold in buffer

void setSignalHandler(int signo, void (*handler)(int)) {  
  struct sigaction act; 
  
  act.sa_handler = handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  
  sigaction(signo, &act, NULL);
}

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

  int opt=1;   
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 	
  
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

/*
 * 读取命令行（命令行必须以\r\n结束，命令和参数以空格分隔）
 * 例如：rpc:system wget www.baidu.com\r\n
 * cmd: 存放命令字符串
 * param: 存放命令参数字符串
 */
void getCommand(int sessionfd, std::string &cmd, std::string &param){
  char data[512];
  std::string str;
  fd_set sess_fdset;
  struct timeval timeout;
  int fd_ready;
  int byte_count = 0;
  FD_ZERO(&sess_fdset);
  bool cmd_got = false;
  
  while(!cmd_got){
    FD_SET(sessionfd, &sess_fdset);
    timeout.tv_sec = 5;
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
      byte_count = read(sessionfd, data, sizeof(data) - 1 - str.length());
      if (byte_count == 0){
        printf("Connection closed by peer\n");
        close(sessionfd);
        exit(0);
      }
      else{
	  	data[byte_count] = '\0';
		str += data;
      }

      auto n = str.find("\r\n");
	  if(n != std::string::npos) {
	    cmd_got = true;
	  } else if(str.length() == sizeof(data) - 1) {
	    printf("Command not ended with \\r\\n!\n");
        close(sessionfd);
        exit(1);
	  }
    }
  }

  if(cmd_got){
  	str.resize(str.length() - 2);
    auto n = str.find(" ");
	if(n == std::string::npos || str.length() < 3 || n == 0 || n == str.length() - 1) {
	  printf("Command format error: no space between cmd and param!\n");
	  close(sessionfd);
      exit(1);
	} else {
	  cmd = str.substr(0, n);
	  param = str.substr(n + 1);
	}
  }
}

void rpcSystem(int sessionfd, std::string sys_cmd) {
  setSignalHandler(SIGCHLD, SIG_DFL);
  
  int ret = system(sys_cmd.c_str());
  setSignalHandler(SIGCHLD, SIG_IGN);

  std::string str = std::to_string(ret);
  send(sessionfd, str.c_str(), str.length(), 0);
}

int Socket::handleSession(int sessionfd){
  std::string cmd, param;

  getCommand(sessionfd, cmd, param);
  printf("new cmd from client: %s, %s.\n", cmd.c_str(), param.c_str());
  if(cmd == "rpc:system") {
  	printf("rpc:system cmd reveived!\n");
	rpcSystem(sessionfd, param);
  }
  
  close(sessionfd);
  exit(0);
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
    close(sessionfd);
  }
}

int Socket::acceptConnections(void){
  int sessionfd;

  addr_size = sizeof(struct sockaddr_in);
  sessionfd = accept(sockfd, (struct sockaddr *)&remote_addr, &addr_size);
  if(sessionfd == -1) {
  	perror("accept error");
	return -1;
  }
  
  printf("Accepted new connection from %s\n", inet_ntoa(remote_addr.sin_addr));
  createSession(sessionfd);
  return 0;
}

int main(void){
  // To avoid zombie process
  setSignalHandler(SIGCHLD, SIG_IGN);

  int port = 1234;
  Socket tcp_socket(port);
  tcp_socket.create();

  while(1){
    tcp_socket.acceptConnections();
  }
  
  return 0;
}

