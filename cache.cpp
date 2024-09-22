#include <stdint.h>
#include <stdio.h>

#include "cache.hpp"

static const int key_width = 3;

static char pagetToChar(cache_size_t type) {
    switch(type) {
        case CE_GHOST:
            return 'n';
        case CE_S_NORM:
            return 'S';
        case CE_S_GHOST:
            return 's';
        case CE_D_NORM:
            return 'D';
        case CE_D_GHOST:
            return 'd';
        case CE_FAKE:
            return '-';
    }
    return 'X';
}

void ARC_Cache::print() {
    cache_size_t i = pagelist[insert_s_i].next;
    printf("T");
    do {
        printf("| %*c ", key_width, pagetToChar(pagelist[i].d.type));
        i = pagelist[i].next;
    } while (i != pagelist[insert_s_i].next);
    i = insert_d_i;
    printf("||");
    do {
        printf(" %*c |", key_width, pagetToChar(pagelist[i].d.type));
        i = pagelist[i].prev;
    } while (i != insert_d_i);

    printf("\nK");

    i = pagelist[insert_s_i].next;
    do {
        printf("| %*u ", key_width, pagelist[i].d.key);
        i = pagelist[i].next;
    } while (i != pagelist[insert_s_i].next);
    i = insert_d_i;
    printf("||");
    do {
        printf(" %*u |", key_width, pagelist[i].d.key);
        i = pagelist[i].prev;
    } while (i != insert_d_i);

    printf("\nD");

    i = pagelist[insert_s_i].next;
    do {
        printf("| %*u ", key_width, pagelist[i].d.data_i);
        i = pagelist[i].next;
    } while (i != pagelist[insert_s_i].next);
    i = insert_d_i;
    printf("||");
    do {
        printf(" %*u |", key_width, pagelist[i].d.data_i);
        i = pagelist[i].prev;
    } while (i != insert_d_i);

    printf("\nI");

    i = pagelist[insert_s_i].next;
    do {
        printf("| %*u ", key_width, i);
        i = pagelist[i].next;
    } while (i != pagelist[insert_s_i].next);
    i = insert_d_i;
    printf("||");
    do {
        printf(" %*u |", key_width, i);
        i = pagelist[i].prev;
    } while (i != insert_d_i);

    printf("\n\n");

}

bool ARC_Cache::checkListPart(cache_size_t start_i, cache_size_t end_i, long long exp_size, enum cache_entry_type type, const char* desc) {
    bool res = true;

    cache_size_t list_i = pagelist[start_i].prev;
    while (list_i != end_i && exp_size > -100) {
        exp_size--;
        if (pagelist[list_i].d.type != CE_GHOST && pagelist[list_i].d.type != type) {
            printf("Page type error in %s: %c instead of %c\n",
                desc,
                pagetToChar(pagelist[list_i].d.type),
                pagetToChar(type));

            res = false;
        }
        if (!(type & CE_GHOST) && pagelist[list_i].d.data_i >= cache_size) {
            printf("Bad page data: %d\n", pagelist[list_i].d.data_i);
            return false;
        }

        list_i = pagelist[list_i].prev;
    }
    if (exp_size != 0) {
        printf("Invalid %s size: diff=%lld\n", desc, exp_size);
        res = false;
    }
    return res;
}

bool ARC_Cache::check() {
    bool res = true;

    res = checkListPart(insert_s_i, evict_s_i, curr_s_size, CE_S_NORM, "S") && res;
    res = checkListPart(evict_s_i, insert_s_i, cache_size / 2, CE_S_GHOST, "S ghost") && res;

    res = checkListPart(insert_d_i, evict_d_i, cache_size - curr_s_size, CE_D_NORM, "D") && res;
    res = checkListPart(evict_d_i, insert_d_i, (cache_size + 1) / 2, CE_D_GHOST, "D ghost") && res;

    return res;
}

cache_size_t ARC_Cache::evictPage(cache_size_t evict_i) {
    cache_size_t epage = pagelist[evict_i].next;
    pagelist[epage].remove(pagelist);
    pagelist[epage].insertBefore(pagelist, evict_i);
    pagelist[epage].d.type |= CE_GHOST;

    cache_size_t data_i = pagelist[epage].d.data_i;

    #ifdef CACHE_DEBUG
        if (data_i >= cache_size) {
            printf("Invalid page at %d\n", epage);
            print();
            abort();
        }

        pagelist[epage].d.data_i = cache_size;
    #endif

    return data_i;
}

cache_size_t ARC_Cache::dropPage(cache_size_t insert_i){
    cache_size_t dpage = pagelist[insert_i].next;
    pagelist[dpage].remove(pagelist);

    cache_map.erase(pagelist[dpage].d.key);

    #ifdef CACHE_DEBUG
        pagelist[dpage].d.type = CE_GHOST;
        pagelist[dpage].d.key = KEY_INVALID;
    #endif

    return dpage;
}

void* ARC_Cache::lookupUpdate(page_id_t key, slow_get_page_t slow) {
    #ifdef CACHE_DEBUG
        if (!check()) {
            print();
        }
    #endif
    auto page_it = cache_map.find(key);
    if (page_it == cache_map.end()) {
        cache_size_t new_data_i;
        cache_size_t new_page_i;

        if (target_s_size > curr_s_size) {
            #ifdef CACHE_DEBUG
                printf("drop D\n");
            #endif
            new_data_i = evictPage(evict_d_i);
            new_page_i = dropPage(insert_d_i);
            curr_s_size++;
        }
        else {
            #ifdef CACHE_DEBUG
                printf("drop S\n");
            #endif
            new_data_i = evictPage(evict_s_i);
            new_page_i = dropPage(insert_s_i);
        }

        PageListNode* page_node = pagelist + new_page_i;

        page_node->d.key = key;
        page_node->d.data_i = new_data_i;
        page_node->d.type = CE_S_NORM;

        page_node->insertBefore(pagelist, insert_s_i);

        bool ok = slow(key, ((uint8_t*) cache_data) + (new_data_i * page_size));
        if (!ok) {
            return nullptr;
        }

        cache_map.insert({key, new_page_i});
        return ((uint8_t*) cache_data) + (new_data_i * page_size);
    }

    cache_size_t page_i = page_it->second;

    PageListNode* page_node = pagelist + page_i;

    #ifdef CACHE_DEBUG
        printf("Hit in %c\n", pagetToChar(page_node->d.type));
    #endif

    // hits in S move the page to D without evicting anything

    page_node->remove(pagelist);
    page_node->insertBefore(pagelist, insert_d_i);

    if (page_node->d.type == CE_S_NORM) {
        curr_s_size--;
    }

    if (page_node->d.type & CE_GHOST) {
        cache_size_t data_i;

        // hit in S_GHOST increases S, evicting entry from D, and back.
        if (page_node->d.type == CE_S_GHOST) {

            data_i = evictPage(evict_d_i);

            if (target_s_size < cache_size - 1)
                target_s_size++;
            // moving page from ghost S to D causes ghost S decrease. One page is dropped from ghost D and added to ghost S to mitigate that.

            cache_size_t page_drop_i = pagelist[insert_d_i].next;
            pagelist[page_drop_i].remove(pagelist);
            pagelist[page_drop_i].d.type = CE_S_GHOST;
            pagelist[page_drop_i].insertAfter(pagelist, insert_s_i);

        }
        else {
            if (curr_s_size == 0) {
                data_i = evictPage(evict_d_i);
            }
            else {
                data_i = evictPage(evict_s_i);
                cache_size_t page_drop_i = dropPage(insert_s_i);

                pagelist[page_drop_i].insertAfter(pagelist, insert_d_i);
                curr_s_size--;
            }

            if (target_s_size > 1)
                target_s_size--;
        }

        page_node->d.data_i = data_i;

        bool ok = slow(key, ((uint8_t*) cache_data) + (data_i * page_size));
        if (!ok) return nullptr;
    }

    page_node->d.type = CE_D_NORM;

    return ((uint8_t*) cache_data) + (page_node->d.data_i * page_size);
}
