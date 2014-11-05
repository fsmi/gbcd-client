#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

/*
 *bcd info fsi-pc3 tst/1
 *bcd scan --force fsi-pc2 tst/1
 */

typedef enum /*_OPERATION_MODE*/ {
	INFO,
	SCAN
} MODE;

struct {
	int verbosity;
	bool force_acquire;
	MODE mode;
	char* server;
	uint16_t port;
	char* name;
	bool running;
	unsigned int max_scans;
} CONF;

int usage(char* fn){
	printf("garfield-barcoded shell client\n\n");
	printf("--verbosity <n>\tSet verbosity (Default: 1, 0 supresses output)\n");
	printf("--force\tForcibly acquire barcode scanner\n");
	printf("--port <n>\tSet barcoded port\n");
	printf("--max-scan <n>\tScan n barcodes and exit\n");
	printf("info\tGet Barcodescanner information\n");
	printf("scan\tScan barcodes if scanner available\n");
	printf("<host>\tBarcoded host\n");
	printf("<username>\tUser name to acquire with\n");
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
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
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

int main(int argc, char** argv){
	unsigned i=0;
	int sockfd=-1;

	CONF.verbosity=1;
	CONF.force_acquire=false;
	CONF.mode=INFO;
	CONF.server=NULL;
	CONF.port=3974;
	CONF.name=NULL;
	CONF.running=false;
	CONF.max_scans=0;

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

	printf("YAY\n");


	//if friendly, wait for acquire, be interactive and stuff
	//get barcodes, wait for kill

	//cleanup
	close(sockfd);
	return 0;
}
