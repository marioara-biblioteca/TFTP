#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct packet{
    char data[1024];
}Packet;

typedef struct frame{
    int frame_kind; //ACK:0, SEQ:1 FIN:2
    int sq_no;
    int ack;
    Packet packet;
}Frame;
Frame frame_send,frame_recv;

void send_to(int sock_fd,struct timeval timeout,struct sockaddr_in new_addr)
{

  setsockopt (sock_fd,
              SOL_SOCKET, SO_SNDTIMEO,
              &timeout,
              sizeof(timeout));
  sendto(sock_fd,
          &frame_send,
          sizeof(frame_send),
          0,
          (struct sockaddr*)&new_addr,
          sizeof(new_addr));
}
int receive_from(int sock_fd,struct timeval timeout,struct sockaddr_in new_addr,socklen_t addr_size)
{

  setsockopt (sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  return recvfrom(sock_fd,
                        &frame_recv,
                        sizeof(frame_recv),
                        0,
                        (struct sockaddr*)&new_addr,
                        &addr_size);

}

int main(int argc, char **argv)
{
  if(argc != 2){
    printf("[-] Usage: %s <port> \n",argv[0] );
    exit(1);
  }
  int port=atoi(argv[1]);

  int sock_fd;
  struct sockaddr_in client_addr;
  socklen_t addr_size;
  addr_size=sizeof(client_addr);
  sock_fd=socket(AF_INET,SOCK_DGRAM,0);
  memset(&client_addr,0,sizeof(client_addr));
  client_addr.sin_family=AF_INET;
  client_addr.sin_port=htons(port);
  client_addr.sin_addr.s_addr=inet_addr("127.0.0.1");

  struct timeval timeout;
  timeout.tv_sec = 10;
  timeout.tv_usec = 0;

  int frame_id = 0;

  char filename[1024];
  printf("Enter desired file name: \n");
  scanf("%s",filename);

  setsockopt (sock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  sendto(sock_fd,
        filename,
        1024,
        0,
        (struct sockaddr*)&client_addr,
        sizeof(client_addr));
  strcat(filename,"_client");printf("\n%s\n",filename);
  int fd=open(filename,O_RDWR | O_CREAT | O_TRUNC, 0644);
  while(1) {
    int received=receive_from(sock_fd,timeout,client_addr,addr_size);
    if(received>0  ){
          if(frame_recv.frame_kind == 1){
              if(frame_recv.sq_no == frame_id){
              printf("[+]Received the %dth seq : (%s)\n",frame_id,frame_recv.packet.data );
              write(fd,frame_recv.packet.data,strlen(frame_recv.packet.data));
              //pregatim frame-ul de trimitere;
              frame_send.sq_no=0;
              frame_send.frame_kind = 0;
              frame_send.ack = frame_recv.sq_no + 1;

              send_to(sock_fd,timeout,client_addr);
              printf("[+]Ack Send. Frame: %d\n",frame_id);
              }else{

                printf("[+]Frame Not Received. Frame: %d\n",frame_id);
              }

          }else if(frame_recv.frame_kind == 2){
              printf("[+]Finished getting the file. Closing connection...\n" );
              frame_send.sq_no=0;
              frame_send.frame_kind = 2;
              frame_send.ack = frame_recv.sq_no + 1;

              send_to(sock_fd,timeout,client_addr);
              printf("[+]Ack Send. Frame: %d\n",frame_id);
              break;
          }
      }else{
      printf("[-]Connection timed out. Frame: %d\n",frame_id);
    }
    frame_id++;
  }
  close(sock_fd);
  close(fd);
  return 0;
}
