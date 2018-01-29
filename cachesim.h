#ifndef CACHESIM_H
#define CACHESIM_H

#include <inttypes.h>

struct cache_stats_t {
    uint64_t accesses;

    uint64_t reads;
    uint64_t read_misses;
    uint64_t read_misses_combined;

    uint64_t writes;
    uint64_t write_misses;
    uint64_t write_misses_combined;

    uint64_t misses;
    uint64_t write_backs;
    uint64_t vc_misses;

    uint64_t subblock_misses;

    uint64_t bytes_transferred; 
   
    double   hit_time;
    double   miss_penalty;
    double   miss_rate;
    double   avg_access_time;
};

void print_statistics(cache_stats_t* p_stats); 

struct Block {
    bool validBit;                  //indicates if block is empty
    bool dirtyBit;                  //indicates dirtiness of stored data
    uint64_t tagStore;              //stored tag for the cache block
    bool* subBlocks;                //array of subblocks' valid bits
};

struct Cache {
    Block** cacheBlocks;
    int C,B,V,S,K;
    int numBlocks,numSubBlocks,numSets,numWays;
    //int offsetLength,indexLength,tagLength; 
    struct cache_stats_t stats;
};

extern Cache cache;
extern Block* vCache;
extern int** LRU;
extern int* FIFOv;
extern int accCtr;

static const uint64_t DEFAULT_C = 15;   /* 32KB Cache */
static const uint64_t DEFAULT_B = 5;    /* 32-byte blocks */
static const uint64_t DEFAULT_S = 3;    /* 8 blocks per set */
static const uint64_t DEFAULT_V = 4;    /* 4 victim blocks */
static const uint64_t DEFAULT_K = 3;    /* 8 byte sub-blocks */

/** Argument to cache_access rw. Indicates a load */
static const char     READ = 'r';
/** Argument to cache_access rw. Indicates a store */
static const char     WRITE = 'w';


//function to print the final statistics from your simulator run. Please copy and paste
//wherever you are going to use this.

#endif /* CACHESIM_H */
