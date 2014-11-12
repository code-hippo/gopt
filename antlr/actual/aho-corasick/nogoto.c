#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>
#include<unistd.h>
#include<papi.h>

#include "aho.h"
#include "util.h"
#include "fpp.h"

#define PATTERN_FILE "/home/akalia/fastpp/data_dump/snort_content_strings"
#define NUM_PKTS (32 * 1024)
#define PKT_SIZE 1500

struct pkt {
	uint8_t content[PKT_SIZE];
};

/**< Generate NUM_PKTS packets for testing. Each test packet is constructed
  *  by concatenating patterns that were inserted into the AC engine. */
struct pkt *gen_packets(struct aho_pattern *patterns, int num_patterns)
{
	int i;
	struct pkt *test_pkts = malloc(NUM_PKTS * sizeof(struct pkt));
	assert(test_pkts != NULL);
	memset(test_pkts, 0, NUM_PKTS * sizeof(struct pkt));

	for(i = 0; i < NUM_PKTS; i ++) {
		int index = 0;
		while(index < PKT_SIZE) {
			test_pkts[i].content[index] = rand() % 256;
			index ++;
		}

		/** Code for generating workload with concatenated content strings
		int tries = 0;
		while(tries < 10) {
			int pattern_i = rand() % num_patterns;

			if(index + patterns[pattern_i].len <= PKT_SIZE) {
				memcpy((char *) &(test_pkts[i].content[index]), 
					patterns[pattern_i].content, patterns[pattern_i].len);
				index += patterns[pattern_i].len;
				break;
			} else {
				tries ++;
			}
		} */
	}

	return test_pkts;
}

int final_state_sum = 0;
int batch_index = 0;

void process_batch(struct aho_state *dfa, struct pkt *test_pkts)
{
	foreach(batch_index, BATCH_SIZE) {
		int j;
		int state = 0;

		for(j = 0; j < PKT_SIZE; j ++) {
			int inp = test_pkts[batch_index].content[j];
			while(dfa[state].G[inp] == AHO_FAIL) {
				state = dfa[state].F;
			}

			state = dfa[state].G[inp];
			FPP_EXPENSIVE(&dfa[state]);
		}

		final_state_sum += state;
	}
}


int main(int argc, char *argv[])
{
	int num_patterns, i, j;
	int *count;

	struct aho_state *dfa;
	aho_init(&dfa);

	/**< Get the patterns */
	struct aho_pattern *patterns = aho_get_patterns(PATTERN_FILE, 
		&num_patterns);

	/**< Build the DFA */
	red_printf("Building AC goto function: \n");
	for(i = 0; i < num_patterns; i ++) {
		aho_add_pattern(dfa, patterns[i].content, i);
	}

	red_printf("Building AC failure function\n");
	aho_build_ff(dfa);

	/**< Generate the workload packets */
	red_printf("Generating packets\n");
	struct pkt *test_pkts = gen_packets(patterns, num_patterns);

	red_printf("Starting lookups\n");
	assert(NUM_PKTS % BATCH_SIZE == 0);

	/** < Variables for PAPI */
	float real_time, proc_time, ipc;
	long long ins;
	int retval;

	/** < Init PAPI_TOT_INS and PAPI_TOT_CYC counters */
	if((retval = PAPI_ipc(&real_time, &proc_time, &ins, &ipc)) < PAPI_OK) {
		printf("PAPI error: retval: %d\n", retval);
		exit(1);
	}

	for(i = 0; i < NUM_PKTS; i += BATCH_SIZE) {
		process_batch(dfa, &test_pkts[i]);
	}

	if((retval = PAPI_ipc(&real_time, &proc_time, &ins, &ipc)) < PAPI_OK) {
		printf("PAPI error: retval: %d\n", retval);
		exit(1);
	}

	red_printf("Time = %.4f s, Instructions = %lld, IPC = %f, sum = %d.\n",
		real_time, ins, ipc, final_state_sum);
	double ns = real_time * 1000000000;
	red_printf("Rate = %.2f Gbps.\n", ((double) NUM_PKTS * PKT_SIZE * 8) / ns);

	/**< Clean up */
	for(i = 0; i < num_patterns; i ++) {
		free(patterns[i].content);
	}

	free(patterns);
	free(test_pkts);
	free(dfa);

	return 0;
}
