#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/time.h>
#include <linux/netlink.h>
#include <signal.h>
#include <pthread.h>

#define NETLINK_MYTRACE 28

#define MSG_SEND_LEN 10
#define MSG_RECV_LEN 4096

#define ERR_NETLINK 10
#define ERR_ARGV	11
#define ERR_THREAD	12
#define ERR_FILE	13

char *file_output;
struct timeval start;
int secs, sockfd; 
unsigned long long traceNum = 0;

struct sockaddr_nl dest_addr;

static void usage(){
	printf("usage: mytrace -o <filename> -w <secs>\n");
}

static void errsys(int err, char *errstr){
	switch(err){
		case ERR_NETLINK:
			perror(errstr);
			break;
		case ERR_ARGV:
			printf("invalid parameter: %s!\n", errstr);
			usage();
			break;
		case ERR_THREAD:
			perror(errstr);
			break;
		case ERR_FILE:
			perror(errstr);
			break;
	}
	exit(-1);
}

static void arg_handle(int argc, char* argv[]){
	int i;
	if(argc != 5){
		errsys(ERR_ARGV, "bad parameters");
	}

	for(i = 1;i < argc; i++){
		if(argv[i][0] == '-'){
			switch(argv[i][1]){
				case 'o':
					file_output = argv[++i];
					break;
				case 'w':
					secs = atoi(argv[++i]);
					break;
				default:
					errsys(ERR_ARGV, argv[i]);
			}
		}
		else
			errsys(ERR_ARGV, argv[i]);
	}
}

/*
*	create netlink sock
*	bind it with local pid addr
*	set kernel addr
*/
static void netlink_setup(){
	/*create a sock*/
	sockfd = socket(PF_NETLINK, SOCK_RAW, NETLINK_MYTRACE);
	if(sockfd < 0){
		errsys(ERR_NETLINK, "create sock error");
	}
	/*no bind*/

	/*set destination*/
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0;
	dest_addr.nl_groups = 0;
}

static void send_to_kern(char* cmd){
	struct nlmsghdr *nlh = NULL;
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MSG_SEND_LEN));
	if(nlh == NULL){
		errsys(ERR_NETLINK, "nlh alloc mem error");
	}

	memset(nlh, 0, MSG_SEND_LEN);

	nlh->nlmsg_len = NLMSG_SPACE(MSG_SEND_LEN);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_type = NLMSG_NOOP;
	nlh->nlmsg_flags = 0;

	strcpy(NLMSG_DATA(nlh), cmd);

	sendto(sockfd, nlh, NLMSG_LENGTH(MSG_SEND_LEN), 0, (struct sockaddr*)(&dest_addr), sizeof(dest_addr));

	free(nlh);
}
/*
void handle_quit(int signo){
	printf("%d going to exit.\n", pthread_self());
	pthread_exit(NULL);
}
*/
int stop_flag = 0;

void worker(FILE* fp){
	/*work thread*/

	struct nlmsghdr *nlh = NULL;
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MSG_RECV_LEN));
	if(nlh == NULL){
		errsys(ERR_NETLINK, "nlh alloc mem error");
	}

	printf("thread num: %d started up.\n", pthread_self());
	/*setup SIGQUIT handler*/
/*	struct sigaction actions;  
	memset(&actions, 0, sizeof(actions));   
	sigemptyset(&actions.sa_mask);
	actions.sa_flags = 0;   
	actions.sa_handler = handle_quit;  
	sigaction(SIGUSR1,&actions,NULL); */

	/*recv traces from kernel*/
	while(1){
		memset(nlh, 0, MSG_RECV_LEN);
		recvfrom(sockfd, nlh, NLMSG_LENGTH(MSG_RECV_LEN), 0, (struct sockaddr*)(&dest_addr), NULL);//flag = 0: this will wait here until new msg come
		traceNum++;
		//printf("rc:%s\n", NLMSG_DATA(nlh));
		fprintf(fp,"%s\n",NLMSG_DATA(nlh));
		if(strcmp(NLMSG_DATA(nlh),"exit") == 0){
	    //	printf("recv - exit.\n");
            break;
		}
	}
	//free(nlh);
	printf("worker exit.\n");
}
FILE* file_setup(){
	FILE *fp;
	fp = fopen(file_output,"w+");
	if(fp == NULL){
		errsys(ERR_FILE, "fopen error");
	}
	return fp;
}



static void trace_begin(){
	
	struct timeval now;
	pthread_t worker_thread;
	int ret;
	FILE *fp;

	gettimeofday(&start, NULL);	
	
	/*setup tracer worker*/
	printf("start mytracer.\n");
	netlink_setup();
	fp = file_setup();



	pthread_create(&worker_thread, NULL, (void *)(&worker), fp);

	/*send cmd to start up tracer*/
	send_to_kern("start\0");

	while(1){
		/*here is a timer*/
		sleep(5);
		gettimeofday(&now, NULL);
		if(now.tv_sec - start.tv_sec > secs){
			printf("stop mytracer.\n");
            goto TIMEUP;
			break;
		}
	}
	/*timeout - send cmd to stop tracer*/
    stop_flag = 1;
/*	printf("kill thread\n");
	ret = pthread_kill(worker_thread, SIGUSR1);//android doesn't have pthread_cancel() API
	if(!ret){
		errsys(ERR_THREAD, "kill thread error");
	} */
	
	ret = pthread_join(worker_thread, NULL);
	if(ret != 0){
		errsys(ERR_THREAD, "wait for worker error");
	}
TIMEUP:
	send_to_kern("stopp\0");
	printf("%lld events.\n",traceNum);
    fclose(fp);
	close(sockfd);/**/
}


int main(int argc, char* argv[])
{
	arg_handle(argc, argv);
	printf("file_output: %s, wait: %d\n", file_output, secs);
	trace_begin();
    return 0;
}
