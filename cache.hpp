#pragma once

#include <unordered_map>
#include <limits>

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include "list.hpp"

#define CACHE_DEBUG

typedef unsigned int page_id_t;
typedef unsigned int cache_size_t;
typedef bool (*slow_get_page_t)(page_id_t, void*);

const page_id_t KEY_INVALID = 0;

enum cache_entry_type {
    CE_S_NORM = 1,
    CE_D_NORM = 2,
    CE_GHOST  = 4, // can be as well node containing empty page
    CE_FAKE   = 8,
    CE_S_GHOST = CE_S_NORM | CE_GHOST,
    CE_D_GHOST = CE_D_NORM | CE_GHOST
};

struct CachePageData {
    cache_size_t data_i;
    cache_size_t type;
    page_id_t key;
};

struct ARC_Cache {
    typedef SimpleListNode<CachePageData, cache_size_t> PageListNode;

    size_t page_size;
    cache_size_t cache_size;

    cache_size_t target_s_size;
    cache_size_t curr_s_size;

    static const cache_size_t insert_s_i = 0;
    static const cache_size_t insert_d_i = 1;

    static const cache_size_t evict_s_i = 2;
    static const cache_size_t evict_d_i = 3;
    static const cache_size_t num_fake_nodes = 4;

    PageListNode* pagelist;

  // page list structure
  // v insert_._i                       v evict_._i
  // /-[insertptr] <- [page] <- [page] <- [evictptr] <- [ghost] <- [ghost]
  // |-------------------------------------------------------------------^
  // new node are inserted "before" insert_i
  // node after evictptr can be evicted
  // new ghost node is then added before evictptr
  // nodes after insertptr are the ones dropped from ghost

    void* cache_data;
    std::unordered_map<page_id_t, cache_size_t> cache_map;

    ARC_Cache(size_t page_size, cache_size_t cache_size) : page_size(page_size), cache_size(cache_size) {
        assert(cache_size >= 2);
        assert(cache_size < (std::numeric_limits<cache_size_t>::max() - num_fake_nodes) / 2);

        target_s_size = cache_size / 2;
        curr_s_size = target_s_size;
        cache_data = calloc(cache_size, page_size);

	    cache_size_t list_size = cache_size * 2 + num_fake_nodes;

        pagelist = (PageListNode*)calloc(list_size, sizeof(PageListNode));

        cache_size_t s_ghost_size = (cache_size / 2);
        cache_size_t s_list_size = target_s_size + s_ghost_size;

        PageListNode::init(pagelist, num_fake_nodes, s_list_size + num_fake_nodes);
        PageListNode::init(pagelist, s_list_size + num_fake_nodes, list_size);

        cache_size_t first_s_ghost = num_fake_nodes + target_s_size;
        cache_size_t first_d_node  = num_fake_nodes + s_list_size;
        cache_size_t first_d_ghost = first_d_node + (cache_size - target_s_size);

        pagelist[insert_s_i].insertAfter(pagelist, num_fake_nodes);
        pagelist[evict_s_i ].insertAfter(pagelist, first_s_ghost);
        pagelist[insert_d_i].insertAfter(pagelist, first_d_node);
        pagelist[evict_d_i ].insertAfter(pagelist, first_d_ghost);

        for (cache_size_t i = num_fake_nodes; i < first_s_ghost; i++) {
            pagelist[i].d.data_i = i - num_fake_nodes;
        }
        for (cache_size_t i = first_d_node; i < list_size; i++) {
            pagelist[i].d.data_i = i - first_d_node;
        }

        #ifdef CACHE_DEBUG
            for (cache_size_t i = 0; i < num_fake_nodes; i++) {
                pagelist[i].d.type = CE_FAKE;
                pagelist[i].d.key = 0;
            }
            for (cache_size_t i = 0; i < list_size; i++) {
                pagelist[i].d.type = CE_GHOST;
                pagelist[i].d.key = KEY_INVALID;
            }
        #endif
    }

    ~ARC_Cache() {
	    free(cache_data);
        free(pagelist);
    }

    void* lookupUpdate(page_id_t key, slow_get_page_t slow);

    void print();
    bool check();

    private:
    cache_size_t evictPage(cache_size_t evict_i);
    cache_size_t dropPage(cache_size_t insert_i);
    bool checkListPart(cache_size_t start_i, cache_size_t end_i, long long exp_size, enum cache_entry_type type, const char* desc);
};
