#ifndef NBLEIER3_CACHE_H
#define NBLEIER3_CACHE_H

#include <stddef.h>
#include <stdint.h>

#define VALID_MASK 1
#define DIRTY_MASK 2

struct cache_stats {
        uint32_t clean_evict;
        uint32_t dirty_evict;
        uint32_t miss;
        uint32_t hit;
};

struct line {
        uint64_t tag;
        uint8_t lru;
        uint8_t ctl_bits;
        /**********************
         * 7 6 5 4 3 2 1 0    
         * | | | | | | | |
         * | | | | | | | > Valid
         * | | | | | | > Dirty
         * ----------> Unused
         **********************/
        struct cache_stats stats;
};

struct cache {
        struct {
                uint8_t setbits;
                uint8_t offsetbits;
                uint8_t ways;
                enum { LRU=0, PLRU=1, NUM_POLICIES=2 } rpolicy;
        } params;
        struct line * lines;
        uint64_t * plru;
};

/*
 * Initializees cache, c
 * sets --- number of sets in cache
 * line_sz --- size of cache data lines in bytes
 * nways --- number of ways in cache
 * policy --- "LRU" for LRU, "PLRU" for PpseudoLRU
 */
void initialize_cache(struct cache * c, uint32_t sets, uint32_t line_sz,
                uint32_t nways, const char * policy);

void free_cache(struct cache * c);

void load(struct cache * c, uint64_t addr);
void store(struct cache * c, uint64_t addr);
void get_stats(struct cache * c, struct cache_stats * s);

#endif
