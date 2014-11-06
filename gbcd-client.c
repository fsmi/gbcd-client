#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <termios.h>

#define NET_BUFFER_LEN 1024

/*
 *TODO: max-scan
 */

typedef enum /*_OPERATION_MODE*/ {
	INFO,
	SCAN
} MODE;

struct {
	int verbosity;
	bool force_acquire;
	bool keep;
	MODE mode;
	char* server;
	uint16_t port;
	char* name;
	unsigned int max_scans;
} CONF;

int usage(char* fn){
	printf("garfield-barcoded shell client\n\n");
	printf("Known parameters:\n");
	printf("\t--verbosity <n>\t\tSet verbosity (Default: 1, 0 supresses output)\n");
	printf("\t--force\t\t\tForcibly acquire barcode scanner\n");
	printf("\t--port <n>\t\tSet barcoded port\n");
	printf("\t--max-scan <n>\t\tScan n barcodes and exit\n");
	printf("\t--keep\t\t\tRegrab the scanner should someone else grab it\n");
	printf("\tinfo\t\t\tGet Barcodescanner information\n");
	printf("\tscan\t\t\tScan barcodes if scanner available\n");
	printf("\t<host>\t\t\tBarcoded host\n");
	printf("\t<username>\t\tUser name to acquire with\n");
	return 1;
}

int sock_connect(char* host, uint16_t port){
	int sockfd=-1, error;
	char port_str[20];
	struct addrinfo hints;
	struct addrinfo* head;
	struct addrinfo* iter;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(port_str, sizeof(port_str), "%d", port);

	error=getaddrinfo(host, port_str, &hints, &head);
	if(error){
		fprintf(stderr, "getaddrinfo: %s\r\n", gai_strerror(error));
		return -1;
	}

	for(iter=head;iter;iter=iter->ai_next){
		sockfd=socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);
		if(sockfd<0){
			continue;
		}

		error=connect(sockfd, iter->ai_addr, iter->ai_addrlen);
		if(error!=0){
			close(sockfd);
			continue;
		}

		break;
	}

	freeaddrinfo(head);
	iter=NULL;

	if(sockfd<0){
		perror("socket");
		return -1;
	}

	if(error!=0){
		perror("connect");
		return -1;
	}

	return sockfd;
}

int handle_input(int fd, char* data){
	unsigned i;
	char send_buf[NET_BUFFER_LEN+1];
	send_buf[0]=0;

	if(CONF.verbosity>1){
		fprintf(stderr, "Received data: %s\r\n", data);
	}

	if(!strncmp(data, "CONNECT", 7)){
		//if info, print barcode scanner name
		if(CONF.mode==INFO){
			printf("%s", data+strlen("CONNECT 1 "));
		}
		//connect, respond with connect
		snprintf(send_buf, sizeof(send_buf)-1, "CONNECT 1 %s\nQUERY\n", CONF.name);
	}
	else if(!strncmp(data, "OK", 2)){
		//logged in or grab success
		//running QUERY here interferes with --force --keep
		//snprintf(send_buf, sizeof(send_buf)-1, "QUERY\n");
	}
	else if(!strncmp(data, "OWNER", 5)){
		if(CONF.mode==INFO){
			data+=strlen("OWNER ");
			for(i=0;data[i]!=' ';i++){
			}
			data[i]=0;
			printf(" - %s\r\n", data);
			snprintf(send_buf, sizeof(send_buf)-1, "QUIT\n");
		}
		else{
			//still owned
			if(CONF.force_acquire){
				snprintf(send_buf, sizeof(send_buf)-1, "GRAB\n");
			}
			else if (CONF.verbosity>0){
				fprintf(stderr, "Scanner is currently busy, (g)rab or (q)uit?\r\n");
			}
		}
	}
	else if(!strncmp(data, "RELEASED", 8)){
		if(CONF.mode==INFO){
			printf(" - Free\r\n");
			snprintf(send_buf, sizeof(send_buf)-1, "QUIT\n");
		}
		else{
			//free to grab, acquire if nice
			snprintf(send_buf, sizeof(send_buf)-1, "ACQUIRE\n");
		}
	}
	else if(!strncmp(data, "REVOKED", 7)){
		//regrab if keep
		if(CONF.mode==SCAN){
			if(CONF.keep){
				snprintf(send_buf, sizeof(send_buf)-1, "GRAB\n");
			}
			else if(CONF.verbosity>0){
				fprintf(stderr, "Scanner was grabbed, re-(g)rab or (q)uit?\r\n");
			}
		}
	}
	else if(!strncmp(data, "BARCODE", 7)){
		//barcode input
		printf("%s\r\n", data+strlen("BARCODE "));
	}
	else if(!strncmp(data, "ERROR", 5)){
		close(fd);
	}
	else{
		if(CONF.verbosity>0){
			fprintf(stderr, "Incoming data not recognized: %s\r\n", data);
		}
		return 1;
	}
	
	if(send_buf[0]){
		if(CONF.verbosity>1){
			fprintf(stderr, "Sent data: %s\r", send_buf);
		}
		send(fd, send_buf, strlen(send_buf), 0);
	}

	return 0;
}

int loop_select(int fd){
	fd_set readfds;
	unsigned i,c;
	struct timeval tv;
	int error, offset=0;
	char stdin_buf[10];
	char sock_buf[NET_BUFFER_LEN+1];

	memset(sock_buf, 0, sizeof(sock_buf));

	while(true){
		FD_ZERO(&readfds);
		FD_SET(fileno(stdin), &readfds);
		FD_SET(fd, &readfds);

		tv.tv_sec=10;
		tv.tv_usec=0;

		error=select(fd+1, &readfds, NULL, NULL, &tv);
		if(error<0){
			perror("select");
			return 1;
		}

		if(error==0&&CONF.mode==INFO){
			if(CONF.verbosity>0){
				fprintf(stderr, "Timeout\r\n");
			}
			return 2;
		}

		if(FD_ISSET(fd, &readfds)){
			//read data
			error=recv(fd, sock_buf, sizeof(sock_buf)-1-offset, 0);
			if(error<0){
				perror("recv");
				return -1;
			}
			else if(error>0){
				//scan for newline
				for(i=0;i<error;i++){
					if(sock_buf[offset+i]=='\n'){
						sock_buf[offset+i]=0;
						handle_input(fd, sock_buf);
						//copy back
						for(c=0;c+i+1<error;c++){
							sock_buf[c]=sock_buf[offset+c+i+1];
						}
						offset=c;
						error=c;
						i=0;
					}
				}
			}
			else{
				//FIXME
				//server disconnected (or buffer overrun)
				break;
			}
		}

		if(FD_ISSET(fileno(stdin), &readfds)){
			error=read(fileno(stdin), stdin_buf, sizeof(stdin_buf));
			if(error<0){
				perror("read/stdin");
				return 1;
			}
			else if(error>0){
				switch(stdin_buf[0]){
					case 'g':
						send(fd, "GRAB\n", 5, 0);
						break;
					case 'q':
						send(fd, "QUIT\n", 5, 0);
						break;
				}
			}
			else{
				break;
			}
		}
	}
	return 0;
}

int main(int argc, char** argv){
	unsigned i=0;
	int sockfd=-1;
	struct termios term_normal, term_raw;

	CONF.verbosity=1;
	CONF.force_acquire=false;
	CONF.mode=INFO;
	CONF.server=NULL;
	CONF.port=3974;
	CONF.name=NULL;
	CONF.max_scans=0;
	CONF.keep=false;

	//parse commandline options
	for(i=1;i<argc;i++){
		if(!strcmp(argv[i], "--verbosity")){
			if(!argv[i+1]){
				fprintf(stderr,"Missing parameter for verbosity\n\n");
				exit(usage(argv[0]));
			}
			CONF.verbosity=strtoul(argv[++i], NULL, 10);
		}
		else if(!strcmp(argv[i], "--force")){
			CONF.force_acquire=true;
		}
		else if(!strcmp(argv[i], "--keep")){
			CONF.keep=true;
		}
		else if(!strcmp(argv[i], "--port")){
			if(!argv[i+1]){
				fprintf(stderr,"Missing parameter for port\n\n");
				exit(usage(argv[0]));
			}
			CONF.port=strtoul(argv[++i], NULL, 10);
		}
		else if(!strcmp(argv[i], "--max-scan")){
			if(!argv[i+1]){
				fprintf(stderr,"Missing parameter for max scans\n\n");
				exit(usage(argv[0]));
			}
			CONF.max_scans=strtoul(argv[++i], NULL, 10);
		}
		else if(!strcmp(argv[i], "info")){
			CONF.mode=INFO;
		}
		else if(!strcmp(argv[i], "scan")){
			CONF.mode=SCAN;
		}
		else{
			if(!CONF.server){
				CONF.server=argv[i];
			}
			else{
				CONF.name=argv[i];
			}
		}
	}

	if(!CONF.server||!CONF.name){
		fprintf(stderr, "Missing vital connection information.\n\n");
		exit(usage(argv[0]));
	}

	if(CONF.verbosity>1){
		fprintf(stderr, "Configuration Summary\n");
		fprintf(stderr, "Verbosity level %d\n", CONF.verbosity);
		fprintf(stderr, "Operation mode: %s\n", (CONF.mode==INFO)?"INFO":"SCAN");
		fprintf(stderr, "Host: %s:%d\n", CONF.server, CONF.port);
		fprintf(stderr, "Acquirer name: %s\n", CONF.name);
		fprintf(stderr, "Forcibly acquire scanner: %s\n", (CONF.force_acquire)?"yes":"no");
		fprintf(stderr, "Limit to %d scans\n", CONF.max_scans);
	}

	//connect to server
	sockfd=sock_connect(CONF.server, CONF.port);
	if(sockfd<0){
		exit(2);
	}

	if(CONF.mode!=INFO&&CONF.verbosity>0){
		fprintf(stderr, "Connected to barcode scanner\nHit q to disconnect\n");
	}

	//set up terminal
	if(tcgetattr(0, &term_normal)<0){
		perror("tcgetattr");
	}

	memcpy(&term_raw, &term_normal, sizeof(term_raw));
	
	cfmakeraw(&term_raw);

	if(tcsetattr(0, TCSANOW, &term_raw)<0){
		perror("tcsetattr");
	}

	if(loop_select(sockfd)<0){
		//error'd
	}

	//cleanup
	if(tcsetattr(0, TCSANOW, &term_normal)<0){
		perror("tcsetattr");
	}
	close(sockfd);
	return 0;
}
