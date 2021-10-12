#include <stdint.h>

#define _TLB_BUFFER_LENGTH (16 * 1024 * 1024)
static char _tlb_buffer1[_TLB_BUFFER_LENGTH] = {0};
static char _tlb_buffer2[_TLB_BUFFER_LENGTH] = {0};
volatile char* _tlb_l1e, *_tlb_l2s;

static char* _align_page_address(char *address, size_t align)
{
	uint64_t target = (uint64_t) address;
	uint64_t aligned;

	aligned = target + (align - (target & (align - 1)));

	return (char *)aligned;
}

static void evict_l1_tlb_set(size_t set)
{
	size_t index, i;
	volatile char *eviction, *p = _tlb_l1e;

	for (i = 0; i < 4; ++i) {
		index = (set + (i * 16)) << 12;
		eviction = (char *)((size_t) p | index);
		*eviction = 0x5A;
	}
}

static void evict_l2_tlb_set(size_t set)
{
	size_t index, i;
	volatile char *eviction, *p = _tlb_l2s;

	for (i = 0; i < 4; ++i) {
		index = (set + (i * 128)) << 12;
		eviction = (char *)((size_t) p | index);
		*eviction = 0x5A;
	}
}

static void evict_l1_tlb_all(void)
{
	for (size_t set = 0; set < 128; set++) {
		evict_l1_tlb_set(set);
	}
}

void tlb_flush(void) {
	for (size_t set = 0; set < 128; set++) {
		evict_l1_tlb_all();
		evict_l2_tlb_set(set);
	}

  asm volatile("lfence");
}

void tlb_init(void) {
  memset(_tlb_buffer1, 2, _TLB_BUFFER_LENGTH);
  memset(_tlb_buffer2, 2, _TLB_BUFFER_LENGTH);

	_tlb_l1e = _align_page_address((char*) &_tlb_buffer1, 0x40000);
	_tlb_l2s = _align_page_address((char*) &_tlb_buffer2, 0x40000);
}
