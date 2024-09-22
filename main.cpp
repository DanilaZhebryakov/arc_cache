#include <stdio.h>

#include "cache.hpp"

const int pagesize = 512;

bool get_page(page_id_t key, void* buf) {
    printf("Get page %u\n", key);
    for (page_id_t i = 0; i < pagesize / sizeof(key); i++) {
        ((page_id_t*)buf)[i] = key;
    }
    return true;
}

int main() {
    ARC_Cache cache (pagesize, 8);
    cache.print();

    cache.lookupUpdate(1, get_page);
    cache.print();

    cache.lookupUpdate(1, get_page);
    cache.print();

    cache.lookupUpdate(2, get_page);
    cache.print();

    cache.lookupUpdate(3, get_page);
    cache.print();

    for (int i = 0; i < 1000000; i++) {
        if (rand() % 2 == 0) {
            cache.lookupUpdate(rand(), get_page);
        }
        else {
            cache_size_t cache_i = rand() % (cache.cache_size * 2 + 4);

            if (cache.pagelist[cache_i].d.type != CE_GHOST && cache.pagelist[cache_i].d.type != CE_FAKE) {
                cache.lookupUpdate(cache.pagelist[cache_i].d.key, get_page);
            }
        }

        if (!cache.check()) {
            cache.print();
            break;
        }
    }
}
