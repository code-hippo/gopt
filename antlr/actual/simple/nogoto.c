#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<papi.h>
#include<time.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<assert.h>

#include "param.h"
#include "fpp.h"

int sum = 0;

int *ht_log;
#define LOG_CAP (128 * 1024 * 1024)
#define LOG_CAP_ ((128 * 1024 * 1024) - 1)
#define LOG_SID 1

// Each packet contains a random integer. The memory address accessed
// by the packet is determined by an expensive hash of the integer.
int *pkts;
#define NUM_PKTS (16 * 1024 * 1024)

// Some compute function
// Increment 'a' by at most COMPUTE * 4: the return value is still random
int hash(int a)
{
	int ret = a;
	int i;
	for(i = 0; i < COMPUTE; i++) {
		ret = ret + ((i * ret) & 7);
	}

	return ret;
}

// batch_index must be declared outside process_pkts_in_batch
int batch_index = 0;

// Process BATCH_SIZE pkts starting from lo
int process_pkts_in_batch(int *pkt_lo)
{
	// Like a foreach loop
	foreach(batch_index, BATCH_SIZE) {

		int mem_addr = hash(pkt_lo[batch_index]) & LOG_CAP_;
		FPP_EXPENSIVE(&ht_log[mem_addr]);
		sum += ht_log[mem_addr];
	}
}

int main(int argc, char **argv)
{
	int i, retval;

	// Variables for PAPI
	float real_time, proc_time, ipc;
	long long ins;

	// Allocate a large memory area
	fprintf(stderr, "Size of ht_log = %lu\n", LOG_CAP * sizeof(int));

	int sid = shmget(LOG_SID, LOG_CAP * sizeof(int), IPC_CREAT | 0666 | SHM_HUGETLB);
	if(sid < 0) {
		fprintf(stderr, "Could not create ht_log\n");
		exit(-1);
	}

	ht_log = shmat(sid, 0, 0);
	assert(ht_log != NULL);
	for(i = 0; i < LOG_CAP; i ++) {
		ht_log[i] = i;
	}

	// Allocate the packets
	pkts = (int *) malloc(NUM_PKTS * sizeof(int));
	for(i = 0; i < NUM_PKTS; i++) {
		pkts[i] = rand() & LOG_CAP_;
	}

	fprintf(stderr, "Finished creating ht_log and packets\n");

	// Init PAPI_TOT_INS and PAPI_TOT_CYC counters
	if((retval = PAPI_ipc(&real_time, &proc_time, &ins, &ipc)) < PAPI_OK) {    
		printf("retval: %d\n", retval);
		exit(1);
	}
	
	for(i = 0; i < NUM_PKTS; i += BATCH_SIZE) {
		process_pkts_in_batch(&pkts[i]);
	}

	if((retval = PAPI_ipc(&real_time, &proc_time, &ins, &ipc)) < PAPI_OK) {    
		printf("retval: %d\n", retval);
		exit(1);
	}
	
	red_printf("Time = %f, Instructions = %lld, IPC = %f, sum = %d\n", real_time, ins, ipc, sum);
}
