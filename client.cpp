#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <string>

#define PORT 1234
#define ADDRESS "127.0.0.1"

int createSocket(void){
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1){
    fprintf(stderr, "socket error: %s\n", gai_strerror(sockfd));
    exit(1);
  }
  
  return sockfd;
}

int connectToServer(int sockfd, struct sockaddr_in server_address){
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(PORT);
  if (inet_pton(AF_INET, ADDRESS, &server_address.sin_addr)<=0){
    printf("Invalid server IP address provided - exiting\n");
    return -1;
  }
  int status = connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address));
  if (status < 0){
    printf("Failed to connect to server - exiting\n");
    printf("Errno: %d\n", errno);
    return -1;
  }
  return 0;
}

int talk2Server(int sockfd, const char *message){
  send(sockfd, message, strlen(message), 0);
  return 0;
}

int ipc_system(const char * cmd){
  struct sockaddr_in server_address;
  memset(&server_address, 0, sizeof(server_address));
  int sockfd = createSocket();
  if (connectToServer(sockfd, server_address) < 0){
    return -1;
  }

  std::string cmd_str = std::string("rpc:system ") + cmd + "\r\n";
  talk2Server(sockfd, cmd_str.c_str());

  char buf[100];
  int recv_sum;
  if((recv_sum = recv(sockfd, buf, sizeof(buf) - 1, 0)) <= 0) {
    return 88;
  }
  buf[recv_sum] = '\0';
  int ret_code = atoi(buf);
  return ret_code;
}

int main(int argc, char ** argv){
//  struct sockaddr_in server_address;
//  memset(&server_address, 0, sizeof(server_address));
//  int sockfd = createSocket();
//  if (connectToServer(sockfd, server_address) < 0){
//    exit(1);
//  }

  if(argc < 2) {
    printf("Usage: %s cmd_string\n", argv[0]);
	exit(1);
  }

//  std::string cmd = argv[1];
//  cmd += "\r\n";  
//  printf("param is:%s.", cmd.c_str());
//  talk2Server(sockfd, cmd.c_str());

  int ret = ipc_system(argv[1]);
  printf("system ret code is %d\n", ret);
}
