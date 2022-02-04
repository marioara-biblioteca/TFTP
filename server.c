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
#include <errno.h>

#define COUNT 1024

typedef struct packet{
    char data[COUNT];
}Packet;

typedef struct frame{
    int frame_kind; //ACK:0, SEQ:1 FIN:2
    int sq_no;
    int ack;
    Packet packet;
}Frame;
Frame frame_recv,frame_send;

ssize_t xread(int fd, void *buf, size_t count)
{
    size_t bytes_read = 0;
    while (bytes_read < count) {
        ssize_t bytes_read_now = read(fd, buf + bytes_read,
                                      count - bytes_read);

        if (bytes_read_now == 0) /* EOF */
            return bytes_read;

        if (bytes_read_now < 0) /* I/O error */
            return -1;

        bytes_read += bytes_read_now;
    }
    return bytes_read;
}

void send_to(int sock_fd,struct timeval timeout,struct sockaddr_in new_addr)
{
  socklen_t len=sizeof(timeout);
  setsockopt (sock_fd,
              SOL_SOCKET, SO_SNDTIMEO,
              &timeout,
              len);
  sendto(sock_fd,
          &frame_send,
          sizeof(frame_send),
          0,
          (struct sockaddr*)&new_addr,
          sizeof(new_addr));
  if(getsockopt(sock_fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,&len)<0){
    if(errno != EAGAIN  || errno != EWOULDBLOCK){
      printf("(Packet not sent, timeout has been reached)\n" );
    }
  }
}

int receive_from(int sock_fd,struct timeval timeout,struct sockaddr_in new_addr,socklen_t addr_size)
{
  socklen_t len=sizeof(timeout);
  setsockopt (sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, len);
  return recvfrom(sock_fd,
                        &frame_recv,
                        sizeof(frame_recv),
                        0,
                        (struct sockaddr*)&new_addr,
                        &addr_size);

  if(getsockopt(sock_fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,&len)<0){
    if(errno != EAGAIN  || errno != EWOULDBLOCK){
        printf("(Packet not sent, timeout has been reached)\n" );
    }
  }
}

int main(int argc, char **argv)
{
  if(argc != 2 ){
    printf("[-]Usage: %s <port>\n",argv[0]);
    exit(1);
  }
  int port=atoi(argv[1]);

  int sock_fd;
  struct sockaddr_in server_addr, new_addr;
  char buffer[COUNT];
  char filename[COUNT];
  socklen_t addr_size;

  sock_fd=socket(AF_INET,SOCK_DGRAM,0);

  memset(&server_addr,0,sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port=htons(port);
  server_addr.sin_addr.s_addr=inet_addr("127.0.0.1");

   struct timeval timeout;
   timeout.tv_sec = 10;
   timeout.tv_usec = 0;

   int frame_id=0;
   //Frame frame_recv,frame_send;
   int ack_recv=1;

  bind(sock_fd, (struct sockaddr*)&server_addr,sizeof(server_addr));
  addr_size=sizeof(new_addr);

  int received = recvfrom(sock_fd,
                          &filename,
                          sizeof(filename),
                          0,
                          (struct sockaddr*)&new_addr,
                          &addr_size);

  if(received<0)
    exit(1);

  int fd=open(filename,O_RDONLY);
  if(fd<0){
    printf("[-]File doesn't exist on server...(%s)\n",filename );
    exit(1);
  }

  int bytes_read=0;
  while(1){

      if(ack_recv==1){

        frame_send.sq_no=frame_id;
        frame_send.frame_kind=1;
        frame_send.ack=0;


        bytes_read=xread(fd,buffer,COUNT);
        buffer[bytes_read]='\0';
        if(bytes_read == 0 && frame_id > 0){
          frame_send.frame_kind=2;
          send_to(sock_fd,timeout,new_addr);
          printf("[+]Last frame sent. Connection is closing...\n");
          break;
        }


    		strcpy(frame_send.packet.data, buffer);
        send_to(sock_fd,timeout,new_addr);
        printf("[+]Frame Send. Frame: %d. Waiting for ack...\n",frame_id);
      }
      int received=receive_from(sock_fd,timeout,new_addr,addr_size);
      if(received > 0 && frame_recv.sq_no == 0){
        if(frame_recv.ack == frame_id + 1){
          printf("[+]Ack Received. Frame: %d\n",frame_id);
    			ack_recv = 1;
        }else{
          printf("[-]Waited ACK not received. Frame: %d. Sending packet again...\n",frame_id);
          send_to(sock_fd,timeout,new_addr);
          ack_recv=0;
          frame_id--;
        }
      }else{
        printf("[-]Ack not received. Frame: %d \n",frame_id );
        ack_recv=0;
      }

      frame_id++;
  }
  close(sock_fd);
  return 0;
}
