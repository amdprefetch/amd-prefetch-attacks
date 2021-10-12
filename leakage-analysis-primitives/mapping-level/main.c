#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "ptedit_header.h"
#include "cacheutils.h"

#define TRIES 10000000
#define AVG 1 //50000

#define LEVELS 4  // 4 = all

/* Alternative metrics: min, max */
#define METRIC (sum / TRIES) // min 

inline __attribute__((always_inline)) void prefetch(size_t p) {
  asm volatile("mfence");
  asm volatile ("prefetcht2 (%0)" : : "r" (p));
  asm volatile("mfence");
}

size_t measurements[TRIES];

size_t mean(size_t* val, size_t len) {
    size_t sum = 0;
    for(size_t i = 0; i < len; i++) {
        sum += val[i];
    }
    return sum / len;
}

double standard_error(size_t* val, size_t len) {
    size_t m = mean(val, len);
    size_t err_sum = 0;
    for(size_t i = 0; i < len; i++) {
        err_sum += (val[i] - m) * (val[i] - m);
    }
    double stddev = sqrt((double)err_sum / (double)(len - 1));
    return stddev / sqrt(len);
}


size_t measure(void* addr, double* err) {
  size_t address = (size_t) addr;
  uint64_t begin = 0, end = 0, min = 0, sum = 0, max = 0;
  ptedit_invalidate_tlb((void*) address);
    
  for (size_t i = 0; i < TRIES; i++) {
    /* Begin measurement */
    begin = rdtsc();

    /* Prefetch kernel address */
    for(size_t j = 0; j < AVG; j++) {
        prefetch(address);
    }

    end = rdtsc();

    /* Update min/max/average power consumption */
    if(end - begin < min || !min) min = end - begin;
    if(end - begin > max) max = end - begin;
    sum += (end - begin);
    measurements[i] = (end - begin) / (AVG);
  }
  
  min /= (AVG);
  max /= (AVG);
  sum /= (AVG);

  *err = standard_error(measurements, TRIES);
  printf("\nPrefetch time: %5zd +/-%1.f\n", METRIC, *err);

  return METRIC;
}


void make_kernel(void* address) {
  ptedit_entry_t entry = ptedit_resolve(address, 0);
  ptedit_cast(entry.pte, ptedit_pte_t).user_access = 0;
  ptedit_update(address, 0, &entry);
}

size_t page_level_offset(int level) {
    if(level <= 0) return 0; 
    if(level <= 1) return 4096;
    else return 512 * page_level_offset(level - 1);
}

void ptedit_print_present_levels(ptedit_entry_t entry) {
    if (!(entry.valid & PTEDIT_VALID_MASK_PGD)) return; 
    printf("PGD of address\n");
    ptedit_print_entry(entry.pgd);
    
    if(ptedit_cast(entry.pgd, ptedit_pgd_t).present) {
        printf("P4D of address\n");
        ptedit_print_entry(entry.p4d);
        if(ptedit_cast(entry.p4d, ptedit_p4d_t).present) {
            printf("PUD of address\n");
            ptedit_print_entry(entry.pud);
            if(ptedit_cast(entry.pud, ptedit_pud_t).present) {
                printf("PMD of address\n");
                ptedit_print_entry(entry.pmd);
                if(ptedit_cast(entry.pmd, ptedit_pmd_t).present) {
                    printf("PTE of address\n");
                    ptedit_print_entry(entry.pte);
                    if(ptedit_cast(entry.pte, ptedit_pte_t).present) {
                        printf("Page of address available\n");
                    }
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    /* Setup */
    if (ptedit_init()) {
        printf("Error: Could not initalize PTEditor, did you load the kernel module?\n");
        return 1;
    }

    /* Find unused PML4 entry */
    size_t start = 0;
    
    for(size_t i = 1; i < 512; i++) {
        size_t current = start + i * 512ull * 1024ull * 1024ull * 1024ull;
        printf("Checking %p...\n", current);
        ptedit_entry_t entry = ptedit_resolve(current, 0);
        ptedit_print_entry_t(entry);
        int first = 0;
        if(!ptedit_cast(entry.pte, ptedit_pgd_t).present) {
            first = 1;
        }
        current = start + (i + 1) * 512ull * 1024ull * 1024ull * 1024ull;
        printf("Checking next %p...\n", current);
        entry = ptedit_resolve(current, 0);
        ptedit_print_entry_t(entry);
        if(!ptedit_cast(entry.pte, ptedit_pgd_t).present && first) {
            start = start + i * 512ull * 1024ull * 1024ull * 1024ull;
            break;
        }
    }
    printf("Found unused PML4 entry at %p\n", start);
    
    char* buffer = (char*)mmap(start, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    memset(buffer, 1, 4096);
    
    make_kernel(start);
    
    FILE* f = fopen("timing.csv", "w");
    fprintf(f, "level,tlb,errtlb\n");
    
    for(int i = 0; i < LEVELS; i++) {
        size_t target = start + page_level_offset(i);
        printf("\n\nResolving address %p (level %d)\n", target, i);
        ptedit_entry_t page_entry = ptedit_resolve(target, 0);
        ptedit_print_present_levels(page_entry);
        
        double cached_err;
        size_t cached = measure(target, &cached_err);
        fprintf(f, "%d,%zd,%f\n", i, cached, cached_err);
    }
    
    fclose(f);


  /* Clean-up */
  ptedit_cleanup();

  return 0;
}

