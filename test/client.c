#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h> 

#define USE_CLIENT_FD

int sockfd;
#ifdef USE_CLIENT_FD
char client_fd[4];
#endif

void *read_handler(void *arg){
	while(1) {
		int nbytes;
		char buffer_2[526]={0};
		nbytes=read(sockfd,buffer_2,sizeof(buffer_2));
		if(nbytes==-1) 	{ 
			fprintf(stderr,"Read Error:%s\n",strerror(errno));
		}
		else if(nbytes <= 0) {
			break;
		}
#ifdef USE_CLIENT_FD
		memcpy(client_fd, buffer_2, 4);
#endif
		buffer_2[nbytes]='\0';
#ifdef USE_CLIENT_FD
		printf("receive: %s\n",buffer_2+4);
#else
		printf("receive: %s\n",buffer_2);
#endif
		usleep(1000);
	}
}

int main(int argc, char *argv[]) {
	int nbytes;
	char buffer[80];
	struct sockaddr_in server_addr;
	struct hostent *host;
	if(argc!=3) {
		fprintf(stderr,"Usage:%s hostname port\a\n",argv[0]);
		exit(1);
	}

	if((host=gethostbyname(argv[1]))==NULL) {
		fprintf(stderr,"Gethostname error\n");
		exit(1);
	}

	if((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1)	{
		fprintf(stderr,"Socket Error:%s\a\n",strerror(errno));
		exit(1);
	}

	bzero(&server_addr,sizeof(server_addr));
	server_addr.sin_family=AF_INET;
	server_addr.sin_port=htons(atoi(argv[2]));
	server_addr.sin_addr = *((struct in_addr *)host->h_addr);
	if(connect(sockfd,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr))==-1) {
		fprintf(stderr,"Connect Error:%s\a\n",strerror(errno));
		exit(1);
	}
	pthread_t main_tid;
	pthread_create(&main_tid, NULL, read_handler, NULL);

	while(1){
#ifdef USE_CLIENT_FD
		fgets(buffer+4,1020,stdin);
		memcpy(buffer, client_fd, 4);
		write(sockfd,buffer,strlen(buffer+4)+4);
#else
		fgets(buffer,1024,stdin);
		write(sockfd,buffer,strlen(buffer));
#endif
		//sleep(1);
	}

	close(sockfd);
	exit(0);
} 


