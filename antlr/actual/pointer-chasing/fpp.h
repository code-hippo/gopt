#define FPP_EXPENSIVE(x)	{}					// Just a hint
#define FPP_ISSET(n, i) (n & (1 << i))
#define FPP_SET(n, i) (n | (1 << i))	// Set the ith bit of n
#define FPP_ISSET(n, i) (n & (1 << i))
	
// Prefetch, Save, and Switch
#define FPP_PSS(addr, label) \
do {\
	__builtin_prefetch(addr, 0, 0); \
	batch_rips[I] = &&label; \
	I = (I + 1) & BATCH_SIZE_;	\
	goto *batch_rips[I]; \
} while(0)

#define BATCH_SIZE 8
#define BATCH_SIZE_ 7

#define foreach(i, n) for(i = 0; i < n; i ++)

long long get_cycles()
{
	unsigned low, high;
	unsigned long long val;
	asm volatile ("rdtsc" : "=a" (low), "=d" (high));
	val = high;
	val = (val << 32) | low;
	return val;
}

#define USE_PAPI 1			// Measure instructions with PAPI?
#define PAPI_MARK_OVERHEAD 264
void papi_start();
long long papi_mark();
