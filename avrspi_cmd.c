#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <getopt.h>
#include "routines.h"
#include <inttypes.h>

#ifndef SCNu8
#define SCNu8 "hhu"
#endif
#ifndef SCNu16
#define SCNu16 "hu"
#endif
#ifndef SCNi16
#define SCNi16 "hi"
#endif

char sock_path[256] = "/dev/avrspi";
char devfile[256] = "/dev/avrspi_cmd"; 

int stop = 0;
int sock;
fd_set fds;
int max_fd;
struct timeval timeout;

int8_t debug = 0;

void print_usage() {
	printf("-f run foreground\n");
	printf("-v verbose\n");
	printf("-u [SOCKET] socket to connect to\n");
}

void catch_signal(int sig)
{
	printf("signal: %i\n",sig);
	stop = 1;
}

void processMsg(struct local_msg *m) {
	printf("Recieved c: %u, t: %u, v: %i\n",m->c,m->t,m->v);

	if (m->c == 1) {
		printf("Disconnect request.\n");
		stop = 1;
	}

}

#define MAX_BUF 64
unsigned char buf1[MAX_BUF]; //receiving buffer
char buf2[256];

void processLine(char *b) {
	struct local_msg msg;
	int len,a[3];
	int i =0;
	if (debug) printf("Processing line: %s\n",b);
	while (i<3 && sscanf( b, "%d%n", &a[i], &len) == 1 ) {
		b += len;    // step past the number we found
		i++;            // increment our count of the number of values found
	}

	unsigned char buf[LOCAL_MSG_SIZE];
	if (i==2) {
		msg.c = 0;
		msg.t = a[0];
		msg.v = a[1];
		if (debug) printf("Matched2: %u %u %i\n",msg.c,msg.t,msg.v);
		pack_lm(buf,&msg);
		write(sock,buf,LOCAL_MSG_SIZE);
	} else if (i==3) {
		msg.c = a[0];
		msg.t = a[1];
		msg.v = a[2];
		if (debug) printf("Matched3: %u %u %i\n",msg.c,msg.t,msg.v);
		pack_lm(buf,&msg);
		write(sock,buf,LOCAL_MSG_SIZE);
	}

}

void run_pipe() {
	int fd,n,i,line,ret;
	if ((fd = open(devfile, O_RDWR|O_NONBLOCK)) == -1) {
		printf("avrspi_cmd: Failed to open %s: %m\n", devfile);
		return;
	}

	n=0;
	while (!stop) {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		timeout.tv_sec = 0;
		timeout.tv_usec = 1000*1000L;
		ret = select(fd+1, &fds, NULL, NULL, &timeout);
		if (ret!=1) continue;

		ret = read(fd,buf2+n,MAX_BUF-n);
		if (ret<=0) {
			printf("AVRSPI_CMD Read error.\n");
			stop = 1;
			return;
		}
		n+=ret;

		line = 0;
		for (i=line;i<n;i++) {
			if (buf2[i]=='\n' || buf2[i]==0) {
				buf2[i] = 0;
				processLine(buf2+line);
				line += (i+1);
			}
		}	

		for (i=line;i<n;i++)
			buf2[line-i] = buf2[i];
		n -= (line);
	}
	close(fd);
}

int main(int argc, char **argv)
{
	struct sockaddr_un address;
	int len;
	struct local_msg msg;
	int b1,b2,cmd;
	int listen = 0;
	int ret;
	unsigned char buf[LOCAL_MSG_SIZE];
	msg.t = 0;
	msg.v = 0;


	int option;
	while ((option = getopt(argc, argv,"vfu:")) != -1) {
		switch (option)  {
			case 'f': listen=1; break;
			case 'u': strcpy(sock_path,optarg); break;
			case 'v': debug = 1; break;
			default:
				  print_usage();
				  return -1;
		}
	}

	/* Create socket on which to send. */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("opening socket");
		exit(1);
	}

	bzero((char *) &address, sizeof(address));
	address.sun_family = AF_UNIX;
	strcpy(address.sun_path, sock_path);
	len = strlen(address.sun_path) + sizeof(address.sun_family);
	/* Construct name of socket to send to. */
	/* Send message. */

	if (connect(sock, (struct sockaddr *) &address, len) < 0) {
		close(sock);
		printf("%s\n",sock_path);
		perror("connecting socket");
		printf("Check if avrspi is running\n");
		exit(1);
	}

	if (listen) buf[0] = 0;
	else buf[0] = 1;
	write(sock,buf,1);

	if (!listen) { //create DEV file

		unlink(devfile);
		if (mkfifo(devfile, 0666) < 0) {
			printf("avrspi_cmd: Failed to create %s: %m\n", devfile);
			return -1;
		}
		if (chmod(devfile, 0666) < 0) {
			printf("avrspi_cmd: Failed to set permissions on %s: %m\n",devfile);
			return -1;
		}

		if (daemon(0,1) < 0) { 
			perror("daemon");
			return -1;
		}

		run_pipe();

		unlink(devfile);

		close(sock);
		return 0;

	} else { //run foreground


		cmd = 0;
		b1 = 0;
		b2 = 0;
		while(!stop) {
			FD_ZERO(&fds);
			FD_SET(sock,&fds);
			FD_SET(STDIN_FILENO,&fds);
			max_fd = sock>STDIN_FILENO?sock:STDIN_FILENO;
			timeout.tv_sec = 0;
			timeout.tv_usec = 100*1000L;
			int sel = select( max_fd + 1 , &fds , NULL , NULL , &timeout);
			if ((sel<0) && (errno!=EINTR)) {
				perror("select");
				stop=1;
			}
			if (!stop && FD_ISSET(sock, &fds)) {
				ret = read(sock, buf1+b1,MAX_BUF-b1);
				if (ret<=0) {
					perror("reading");
					stop = 1;
				} else 	{
					b1+=ret;
					printf("Got %i %i\n",ret,b1);
					int msg_no = b1 / LOCAL_MSG_SIZE;
					int reminder = b1 % LOCAL_MSG_SIZE;
					int i;
					for (i=0;i<msg_no;i++) {
						unpack_lm(buf1+i*LOCAL_MSG_SIZE,&msg);
						processMsg(&msg);
					}

					if (msg_no) {
						for (i=0;i<reminder;i++)
							buf1[i] = buf1[msg_no*LOCAL_MSG_SIZE+i];
						b1 = reminder;
					}
				}
			}
			if (!stop && FD_ISSET(STDIN_FILENO,&fds)) {
				do {
					ret = read(STDIN_FILENO, buf2+b2,1);
					if (ret>0) {
						if (buf2[b2]=='\n') cmd=1;
						b2++;
					}
				} while (!cmd && ret>0);
				if (cmd) {
					ret = sscanf(buf2,"%" SCNu8 "%" SCNi16 "\n",&msg.t,&msg.v);
					if ((ret == 2) && (b2>3)) {
						msg.c = 0;
						printf("Sending c: %u, t: %u v: %i\n",msg.c,msg.t,msg.v);
						pack_lm(buf,&msg);
						if (write(sock, buf, LOCAL_MSG_SIZE) < 0)
							perror("writing");
					} else printf("Error paring. Enter 2 arguments: type and value\n");
				}
				cmd = 0;
				b2 = 0;
			}
		}

		close(sock);
	}
}

