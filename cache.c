#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define printerr(msg) fprintf(stderr, "In file %s function %s line %d: %s\n", __FILE__, __func__, __LINE__, msg)

// Update LRU callbacks
void update_lru_lru(struct cache * c, uint32_t s, uint32_t way);
void update_lru_plru(struct cache * c, uint32_t s, uint32_t way);
static void (* const up_lru[NUM_POLICIES])(struct cache *, uint32_t, uint32_t) =
                {update_lru_lru, update_lru_plru};

// Find LRU callbacks
int find_lru_lru(struct cache * c, uint32_t s);
int find_lru_plru(struct cache * c, uint32_t s);
static int (* const _find_lru[NUM_POLICIES])(struct cache *, uint32_t s) =
                        {find_lru_lru, find_lru_plru};

void initialize_cache(struct cache * c, uint32_t sets, uint32_t line_sz,
                uint32_t nways, const char * policy)
{
        if (c == NULL) {
                printerr("c must not be NULL");
                return;
        }

        if ((__builtin_popcount(sets) != 1) || (__builtin_popcount(nways) != 1)
                || (__builtin_popcount(line_sz) != 1)) {
                printerr("sets, line_sz, nways must be power of 2");
                exit(1);
        }

        c->params.setbits = ffs(sets)-1;
        c->params.offsetbits = ffs(line_sz)-1;
        c->params.ways = nways;
        if (strcmp("LRU", policy) == 0) {
                c->params.rpolicy = LRU;
        }
        else if (strcmp("PLRU", policy) == 0) {
                c->params.rpolicy = PLRU;
        }
        else {
                printerr("unknown replacement policy");
                exit(1);
        }

        c->lines = calloc(nways * sets, sizeof *(c->lines));
        if (c->lines == NULL) { perror("calloc"); exit(1);}
        if (c->params.rpolicy == PLRU) {
                c->plru = calloc(sets * nways, sizeof *(c->plru));
                if (c->plru == NULL) { perror("calloc"); exit(1);}
        }
        printf("Allocating %d lines\n", nways * sets);

        for (int i = 0; i < sets; ++i) {
                struct line * lsets = c->lines + (i * nways);
                for (int j = 0; j < nways; ++j) {
                        lsets[j].lru = j;
                }
        }

        printf("creating cache with %d sets, %d byte lines, %d ways and"
                        "%s replacement policy\n",
                        1 << c->params.setbits, 1 << c->params.offsetbits,
                        c->params.ways, policy);
}

void free_cache(struct cache * c)
{
        free(c->lines);
        if (c->params.rpolicy == PLRU)
                free(c->plru);
}

int find_lru_lru(struct cache * c, uint32_t s)
{
        struct line * set = &c->lines[s * c->params.ways];
        for (int i = 0; i < c->params.ways; ++i) {
                if (set[i].lru != c->params.ways-1)
                        continue;
                return i;
        }
        fprintf(stderr, "LRU functionality failed for set %d\n", s);
        printerr("LRU functionality failed");
        exit(1);
}

int find_lru_plru(struct cache * c, uint32_t s)
{
        int w = (c->params.ways) >> 1;
        int idx = 1;
        int retval = 0;
        char * plru = &(c->plru[s * c->params.ways]);
        
        while (idx < c->params.ways) {
                switch (plru[idx]) {
                        case 0:
                                idx = idx << 1;
                                break;
                        case 1:
                                idx = (idx << 1) + 1;
                                retval += w;
                                break;
                        default:
                                printerr("LRU functionality failed");
                                exit(1);

                }
                w >>= 1;
        }


}

void update_lru_lru(struct cache * c, uint32_t s, uint32_t way)
{
        struct line * set = &c->lines[s * c->params.ways];
        int old_lru = set[way].lru;
        set[way].lru = 0;
        for (int i = 0; i < c->params.ways; ++i) {
                if (i == way)
                        continue;
                if (set[i].lru < old_lru)
                        set[i].lru += 1;
        }
}

void update_lru_plru(struct cache * c, uint32_t s, uint32_t way)
{
        int ways = c->params.ways;
        int idx = 1;
        char * plru = &(c->plru[s * c->params.ways]);

        while (ways > 1) {
                if (way < (ways >> 1)) {
                        // traverse left
                        plru[idx] = 1;
                        idx = idx << 1;
                }
                else {
                        // traverse right
                        plru[idx] = 0;
                        idx = (idx << 1) + 1;
                }
                ways >>= 1;
        }
}

void update_lru(struct cache * c, uint32_t s, uint32_t way)
{
        up_lru[c->params.rpolicy](c, s, way);
}


int find_lru(struct cache * c, uint32_t s)
{
        return _find_lru[c->params.rpolicy](c, s);
}

void evict_line(struct cache * c, uint32_t s)
{
        struct line * set = &c->lines[s * c->params.ways];
        int lru_way = find_lru(c, s);
        if (set[lru_way].ctl_bits & DIRTY_MASK) {
                set[lru_way].stats.dirty_evict += 1;
        }
        else {
                set[lru_way].stats.clean_evict += 1;
        }
        set[lru_way].ctl_bits = 0;
}

void load(struct cache * c, uint64_t addr)
{
        uint64_t tag = addr >> (c->params.setbits + c->params.offsetbits);
        uint64_t set = (addr >> c->params.offsetbits) & 
                                ((1 << c->params.setbits) - 1);

        //printf("Loading addr %p into set %d\n", (void*)addr, (int)set);
        // Search for hit
        for (int i = 0; i < c->params.ways; ++i) {
                struct line * l = &c->lines[set * c->params.ways + i];
                if ((l->ctl_bits & VALID_MASK == 0) || (l->tag != tag))
                        continue;

                l->stats.hit += 1;
                update_lru(c, set, i);
                return;
        }

        // No hit, search for non-valid

SEARCH:
        for (int i = 0; i < c->params.ways; ++i)  {
                struct line * l = &c->lines[set * c->params.ways + i];
                if ((l->ctl_bits & VALID_MASK) == 0) {
                        // Use this way
                        l->ctl_bits = VALID_MASK;
                        l->stats.miss += 1;
                        l->tag = tag;
                        update_lru(c, set, i);
                        return;
                }
        }

        // No hit, no valid, evict a line :(
        evict_line(c, set);
        goto SEARCH;
}

void store(struct cache * c, uint64_t addr)
{
        uint64_t set = (addr >> c->params.offsetbits) & 
                                ((1 << c->params.setbits) - 1);
        load(c, addr);
        struct line * l = &c->lines[set * c->params.ways];
        uint64_t tag = addr >> (c->params.setbits + c->params.offsetbits);
        for (int i = 0; i < c->params.ways; ++i) {
                if ((l[i].tag == tag) && (l[i].ctl_bits & VALID_MASK)) {
                        l[i].ctl_bits |= DIRTY_MASK;
                        return;
                }
        }
        fprintf(stderr, "Unable to find set %lu, tag %lu\n", set, tag);
        perror("Load functionality failed before store");
        exit(1);
}

void get_stats(struct cache * restrict c, struct cache_stats * restrict s)
{
        memset(s, 0, sizeof *s);
        size_t limit = (1 << c->params.setbits) * c->params.ways;
        for (size_t i = 0; i < limit; ++i) {
                s->miss += c->lines[i].stats.miss;
                s->hit += c->lines[i].stats.hit;
                s->dirty_evict += c->lines[i].stats.dirty_evict;
                s->clean_evict += c->lines[i].stats.clean_evict;
        }
}

