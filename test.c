#include <stdio.h>
#include "cache.h"

static const uint64_t in = 1 << 20;
static const uint64_t out = 1 << 22;

void standard(struct cache * c)
{
        for (int i = 0; i < 256; ++i) {
                for (int j = 0; j < 256; ++j) {
                        load(c, in + (i * 1024) + (j * 4));
                        store(c, out + (i * 4) + (j * 1024));
                }
        }
}

void tile(struct cache * c, int tile)
{
        for (int i = 0; i < 256; i += tile) {
                for (int j = 0; j < 256; j += tile) {
                        for (int ii = 0; ii < tile; ++ii) {
                                for (int jj = 0; jj < tile; ++jj) {
                                        load(c, in + (ii * 1024) + (j * 4));
                                        store(c, out + (ii * 4) + (jj * 1024));
                                }
                        }
                }
        }
}

int main(void)
{
        struct cache c;
        struct cache_stats s;
        initialize_cache(&c, 1024, 8, 8, "LRU");

        //standard(&c);
        tile(&c, 8);

        get_stats(&c, &s);
        free_cache(&c);
        printf("Misses: %d\nHits: %d\nClean Evictions: %d\nDirty Evictions: %d\n",
                        s.miss, s.hit, s.clean_evict, s.dirty_evict);
        return 0;
}


