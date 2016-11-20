#ifndef MTRACE_H
#define MTRACE_H


#include <linux/types.h>
#include <linux/kernel.h>

#include <net/sock.h>           //struct sock nlmsg_put
#include <linux/netlink.h>		//

#include <linux/time.h>			//struct timespec
#include <linux/bio.h>          //struct bio

struct mtrace{
	int has_rgsted;
	struct timespec start_time;
	struct sock *nl_mtrace_fd;
    __u32 uid;
};

typedef struct meta{
    struct timespec delay;  //delay
    char RW;                //RW
    unsigned long bi_sector;
    unsigned int bytes_n;
    char comm[18];
}bio_mt_t;

#endif