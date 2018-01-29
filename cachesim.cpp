//cachesim.cpp
//Dillon J. Notz - ECE 4100 @ Georgia Tech (2/13/17)
#include "cachesim.h"
#include <fstream>
#include <iostream>
#include <string>
#include <cmath>
#include <unistd.h>    /* for getopt */
#include <stdlib.h>
#include <stdio.h>
using namespace std;

//helper functions
void doOperation(uint64_t, uint64_t, uint64_t, char);
void blockMiss(uint64_t, uint64_t, uint64_t, char);
void blockMissVC(uint64_t, uint64_t, uint64_t, char);
void subBlockMissR(uint64_t, uint64_t, uint64_t);
void subBlockMissW(uint64_t, uint64_t, uint64_t);
bool checkVC(uint64_t, uint64_t&);
void replaceVictim(uint64_t, uint64_t, uint64_t);
void evictBlock(Block&, uint64_t, char);
uint64_t createVictimTagStore(uint64_t, uint64_t);

//global variables
struct Cache cache;
Block* vCache;
int** LRU;
int* FIFOv;
int accCtr = 0;

int main(int argc, char* argv[]) {

	//handle command line parameters 
	char c;
	int C = DEFAULT_C;
	int B = DEFAULT_B;
	int S = DEFAULT_S;
	int V = DEFAULT_V;
	int K = DEFAULT_K;
	char* inputFileName;

    while ( (c = getopt(argc, argv, "C:B:V:S:K:i:")) != -1) {
        switch (c) {
	        case 'C':
	        	C = atoi(optarg);
	        	break;
	        case 'B':
	        	B = atoi(optarg);
	        	break;
	        case 'V':
	        	V = atoi(optarg);
	        	break;
	        case 'S':
	        	S = atoi(optarg);
	        	break;
	        case 'K':
	        	K = atoi(optarg);
	        	break;
	        case 'i':
	        	inputFileName = optarg;
	        	break;
        }
    }

	//create trace file object
	ifstream inputTrace;
	inputTrace.open(inputFileName); 

	//initialize cache
	cache.C = C; cache.B = B; cache.S = S; cache.V = V; cache.K = K;
	cache.numBlocks = pow(2,C-B); cache.numSubBlocks = pow(2,B-K);
	cache.numWays = pow(2,S); cache.numSets = (cache.numBlocks)/(cache.numWays);
	cache.cacheBlocks = new Block*[cache.numSets];
	LRU = new int*[cache.numSets];
	for(int i = 0; i < cache.numSets; i++) {
		cache.cacheBlocks[i] = new Block[cache.numWays];
		LRU[i] = new int[cache.numWays]; //initialize to 0;
		for(int j = 0; j < cache.numWays; j++) {
			LRU[i][j] = 0;
		}
	}
	for(int i = 0; i < cache.numSets; i++) {
		for(int j = 0; j < cache.numWays; j++) {
			cache.cacheBlocks[i][j].validBit = false;
			cache.cacheBlocks[i][j].dirtyBit = false;
			cache.cacheBlocks[i][j].tagStore = 0;
			cache.cacheBlocks[i][j].subBlocks = new bool[cache.numSubBlocks];
			for(int b = 0; b < cache.numSubBlocks; b ++) {
				cache.cacheBlocks[i][j].subBlocks[b] = false;
			}
		}
	}

	//initialize victim cache
	vCache = new Block[V];         //create victim cache with V blocks
	FIFOv  = new int[V];
	for(int i = 0; i < V; i++) {
		vCache[i].validBit = false;
		vCache[i].dirtyBit = false;
		vCache[i].tagStore = 0;
		vCache[i].subBlocks = new bool[cache.numSubBlocks];
		FIFOv[i] = 0;
		for(int b = 0; b < cache.numSubBlocks; b++) {
			vCache[i].subBlocks[b] = false;
		}
	}

	//initialize cache stats to 0
	cache.stats.accesses              = 0;
	cache.stats.reads                 = 0;
	cache.stats.read_misses           = 0;
	cache.stats.read_misses_combined  = 0;
	cache.stats.writes                = 0;
	cache.stats.write_misses          = 0;
	cache.stats.write_misses_combined = 0;
	cache.stats.misses                = 0;
	cache.stats.write_backs           = 0;
	cache.stats.vc_misses             = 0;
	cache.stats.subblock_misses       = 0;
	cache.stats.bytes_transferred     = 0;
	cache.stats.miss_penalty          = 100.0;

	uint64_t oMask, iMask, tMask;
	uint64_t offset, index, tag;
	oMask  = (1 << B) - 1;                //mask lowest B bits
	iMask  = ((1 << (C-B-S)) - 1) << B;   //mask next (C-B-S) bits
	tMask  = ~(oMask | iMask);            //mask remaining bits

	char readOrWrite;
	string rawAddress;
	long unsigned int accAddress;
	while(true) {                         //trace through input file
        //get r/w character
		inputTrace >> readOrWrite;

		//get access address
		inputTrace >> rawAddress;
		if( inputTrace.eof() ) break;     //finished tracing?
		accAddress = stoul(rawAddress,NULL,16);

		//parse access address
		offset = accAddress & oMask;                       //get offset
		index  = (accAddress & iMask) >> B;                //get index
		tag    = (accAddress & tMask) >> (C-S);            //get tag

		doOperation(offset,index,tag,readOrWrite);         //execute access
	}

	//calculate total number of accesses
	cache.stats.accesses = cache.stats.reads + cache.stats.writes;

	//calculate performance statistics
	double missRate;
	cache.stats.hit_time = 2.0 + (.1*(1<<cache.S)); 
	if(cache.V == 0) { //no victim cache
		double misses   = cache.stats.misses + cache.stats.subblock_misses;
		missRate = (misses/cache.stats.accesses);
	}
	else {             //victim cache
		double misses = cache.stats.misses;
		double missRateL1 = (misses/cache.stats.accesses);
		double missRateVC = ((cache.stats.vc_misses + cache.stats.subblock_misses)/misses);
		missRate = missRateL1 * missRateVC;
	}
	
	cache.stats.miss_rate = missRate;
	cache.stats.avg_access_time = cache.stats.hit_time + (missRate * cache.stats.miss_penalty);

	print_statistics(&cache.stats);

	//delete dynamically allocated memory
	for(int i = 0; i < cache.numSets; i++) {
		for(int j = 0; j < cache.numWays; j++) {
			cache.cacheBlocks[i][j].subBlocks = new bool[cache.numSubBlocks];
		}
		delete[] cache.cacheBlocks[i];
		delete[] LRU[i];
	}

	delete[] cache.cacheBlocks;
	delete[] LRU;

	for(int i = 0; i < V; i++) {
		delete[] vCache[i].subBlocks;
	}
	delete[] vCache;
	delete[] FIFOv;
}

void doOperation(uint64_t offset, uint64_t index, uint64_t tag, char accType) {
	//increment global LRU access counter
	accCtr += 1;

	if(accType == 'r') {  //operation is a read
		cache.stats.reads += 1;
	}
	else {                //operation is a write
		cache.stats.writes += 1;
	}

	//check tags of all ways to see if there's a block hit
	bool blockHit = false;
	uint64_t wayIndex;
	for(int i = 0; i < cache.numWays; i++) {
		if(cache.cacheBlocks[index][i].validBit == true) {
			if(cache.cacheBlocks[index][i].tagStore == tag) {
				blockHit = true; //block hit
				wayIndex = i;
				break;
			}
		}
	}
	if(blockHit) { //there was a block hit (requested block is in L1 cache)
		//check valid bit of subBlock to see if there's a subBlock hit
		uint64_t subBlockNum = offset/(pow(2,cache.K)); //determine which subBlock is being accessed
		if(cache.cacheBlocks[index][wayIndex].subBlocks[subBlockNum] == true) { //there was a subBlock hit
			if(accType == 'w') {
				cache.cacheBlocks[index][wayIndex].dirtyBit = true;
			}	
		}
		else { //there was subBlock miss
			cache.stats.subblock_misses += 1;
			if(accType == 'r') { //was a read
				subBlockMissR(subBlockNum,wayIndex,index);
			}
			else {               //was a write
				subBlockMissW(subBlockNum,wayIndex,index);
			}
		}
		LRU[index][wayIndex] = accCtr;
	}
	else { //there was a block miss (requested block is not in L1 cache)
		cache.stats.misses += 1;
		if(accType == 'r') { //was a read miss
			cache.stats.read_misses += 1;
		}
		else {               //was a write miss
			cache.stats.write_misses += 1;
		}

		//first check victim cache
		uint64_t vcIndex;
		uint64_t tagToStore = createVictimTagStore(tag, index); //get potential tagStore to check VC
		bool inVC = checkVC(tagToStore, vcIndex); //check victim cache

		if(inVC) { //found block in victim cache
			replaceVictim(tag, index, vcIndex);
		}
		else {     //didn't find block in victim cache
			
			if(cache.V == 0) { //no victim cache
				blockMiss(tag, index, offset, accType);
			}
			else {            //victim cache present
				cache.stats.vc_misses += 1;
				blockMissVC(tag, index, offset, accType);
			}
			if(accType == 'r') {
				cache.stats.read_misses_combined += 1;
			}
			else {
				cache.stats.write_misses_combined += 1;
			}						
		}	
	}
}

void blockMiss(uint64_t tag, uint64_t index, uint64_t offset, char accType) {
	//needed for later calculations
	uint64_t subBlockNum = offset/(1 << cache.K);
	int bytesPerTransfer = (1 << cache.K);

	//load data into cache
	bool spotOpen = false;
	for(int i = 0; i < cache.numWays; i++) { //check entire cache line (all ways)
		if(cache.cacheBlocks[index][i].validBit == false) { //found completely empty block
			spotOpen = true;
			LRU[index][i] = accCtr; //set soon-to-be filled block's access number for LRU purposes

			for(int sB = subBlockNum; sB < cache.numSubBlocks; sB++) {
				cache.cacheBlocks[index][i].subBlocks[sB] = true; //validate accessed subBlock and above
				cache.stats.bytes_transferred += bytesPerTransfer;//fetch/prefetch data from memory
			}
			cache.cacheBlocks[index][i].tagStore = tag;  //update stored tag
			cache.cacheBlocks[index][i].validBit = true; //update valid bit of block
			if(accType == 'w') { //was a write, make dirty
				cache.cacheBlocks[index][i].dirtyBit = true;
			}
			i = cache.numWays; //break out of for loop
		}
	}
	if(!spotOpen) { //block eviction because all blocks are full
		int LRused = 0;
		int temp = LRU[index][LRused];           //guess first entry to be oldest
		for(int i = 1; i < cache.numWays; i++) { //finds entry with oldest data
			if(LRU[index][i] < temp) {           //which is the block entry to replace
				temp = LRU[index][i]; 
				LRused = i;            
			}
		}
		LRU[index][LRused] = accCtr;   //set soon to be filled block's access number in LRU array
		
		//replace LRused block's subblocks
		for(int sB = 0; sB < cache.numSubBlocks; sB++) { //write back any dirty subBlocks
			if((cache.cacheBlocks[index][LRused].dirtyBit == true) && (cache.cacheBlocks[index][LRused].subBlocks[sB] == true)) {
				cache.stats.bytes_transferred += bytesPerTransfer;
			}
			if(sB < subBlockNum) {
				cache.cacheBlocks[index][LRused].subBlocks[sB] = false;	
			}
			else {
				cache.cacheBlocks[index][LRused].subBlocks[sB] = true;
				cache.stats.bytes_transferred += bytesPerTransfer;      //replace with new data from memory
			}	
		}
		if(cache.cacheBlocks[index][LRused].dirtyBit == true) {
			cache.stats.write_backs += 1;
		}

		cache.cacheBlocks[index][LRused].tagStore = tag;   //update stored tag
		
		if(accType == 'w') { //was a write, make dirty
			cache.cacheBlocks[index][LRused].dirtyBit = true;
		}		
		else { //was a read, make clean
			cache.cacheBlocks[index][LRused].dirtyBit = false;
		}
	}
}

void blockMissVC(uint64_t tag, uint64_t index, uint64_t offset, char accType) {
	//needed for later calculations
	uint64_t subBlockNum = offset/(1 << cache.K);
	int bytesPerTransfer = (1 << cache.K);

	//load data into cache
	bool spotOpen = false;
	for(int i = 0; i < cache.numWays; i++) { //check entire cache line (all ways)
		if(cache.cacheBlocks[index][i].validBit == false) { //found empty block
			spotOpen = true;
			LRU[index][i] = accCtr; //set soon-to-be filled block's access number for LRU purposes

			for(int sB = subBlockNum; sB < cache.numSubBlocks; sB++) {
				cache.cacheBlocks[index][i].subBlocks[sB] = true; //validate accessed subBlock and above
				cache.stats.bytes_transferred += bytesPerTransfer;//fetch/prefetch data from memory
			}
			cache.cacheBlocks[index][i].tagStore = tag;  //update stored tag
			cache.cacheBlocks[index][i].validBit = true; //update valid bit of block
			if(accType == 'w') { //was a write, make dirty
				cache.cacheBlocks[index][i].dirtyBit = true;
			}
			i = cache.numWays; //break out of for loop
		}
	}
	if(spotOpen == false) { //evict LRU block to victim cache because all ways are full
		int LRused = 0;                          //find least recently used block to replace
		int temp = LRU[index][LRused];           //guess first entry to be oldest
		for(int i = 1; i < cache.numWays; i++) { //finds entry with oldest data
			if(LRU[index][i] < temp) {           //which is the block entry to replace
				temp = LRU[index][i]; 
				LRused = i;            
			}
		}
		LRU[index][LRused] = accCtr;   //set soon to be filled block's access number in LRU array
		
		//Description: evict block to victim cache
		//try to find empty block in VC
		bool spotOpenVC = false;
		int emptyVCBlock;
		for(int i = 0; i < cache.V; i++) {
			if(vCache[i].validBit == false) {
				spotOpenVC = true; //found empty spot in VC
				emptyVCBlock = i;
				FIFOv[i] = accCtr;
				i = cache.V; //break from loop
			}
		}

		if(spotOpenVC) { //found an empty spot
			//put evicted block in VC
			//cout << "Filling empty block in VC..." << endl;
			vCache[emptyVCBlock].validBit = true;
			vCache[emptyVCBlock].dirtyBit = cache.cacheBlocks[index][LRused].dirtyBit;
			uint64_t tagToStore = createVictimTagStore(cache.cacheBlocks[index][LRused].tagStore, index);
			vCache[emptyVCBlock].tagStore = tagToStore;
			for(int i = 0; i < cache.numSubBlocks; i++) {
				vCache[emptyVCBlock].subBlocks[i] = cache.cacheBlocks[index][LRused].subBlocks[i];
			}
		}
		else { //there were no empty spots in the VC
			//find victim cache block to replace
			//cout << "Finding empty block to replace..." << endl;
			int FIFO = 0;
			int temp = FIFOv[0];
			for(int i = 1; i < cache.V; i++) {
				if(FIFOv[i] < temp) {
					temp = FIFOv[i];
					FIFO = i;
				}
			}

			//record access counter for FIFO policy
			FIFOv[FIFO] = accCtr;

			//evict block from VC
			evictBlock(vCache[FIFO], subBlockNum, accType);

			//put evicted block from L1 cache into VC
			vCache[FIFO].validBit = true;
			vCache[FIFO].dirtyBit = cache.cacheBlocks[index][LRused].dirtyBit;
			//cout << "Attemping to create long tagStore..." << endl;
			uint64_t tagToStore = createVictimTagStore(cache.cacheBlocks[index][LRused].tagStore, index);
			//cout << "Successfully created long tagStore..." << endl;
			vCache[FIFO].tagStore = tagToStore;
			for(int i = 0; i < cache.numSubBlocks; i++) {
				vCache[FIFO].subBlocks[i] = cache.cacheBlocks[index][LRused].subBlocks[i];
			}
		}
		//load block from memory into newly freed L1 block/way
		for(int sB = 0; sB < cache.numSubBlocks; sB++) { 
			if(sB < subBlockNum) {
				cache.cacheBlocks[index][LRused].subBlocks[sB] = false;	
			}
			else {
				cache.cacheBlocks[index][LRused].subBlocks[sB] = true;
				cache.stats.bytes_transferred += bytesPerTransfer;      //replace with new data from memory
			}	
		}

		cache.cacheBlocks[index][LRused].tagStore = tag;   //update stored tag

		if(accType == 'w') { //was a write, make dirty
			cache.cacheBlocks[index][LRused].dirtyBit = true;
		}		
		else {               //was a read, make clean
			cache.cacheBlocks[index][LRused].dirtyBit = false;
		}		
	}
}

bool checkVC(uint64_t tagStoreFA, uint64_t& vcIndex) {
	for(int i = 0; i < cache.V; i++) {
		if(vCache[i].validBit) {
			if(vCache[i].tagStore == tagStoreFA) {
				vcIndex = i;
				return true;
			}
		}
	}
	return false;
}

void replaceVictim(uint64_t tag, uint64_t index, uint64_t vcIndex) {
	//Description: swaps LRU block of cache line with vCache[vcIndex]

	//find "victim" LRU block of cache line to swap out
	int LRused = 0;
	int temp = LRU[index][LRused];
	for(int i = 1; i < cache.numWays; i++) {
		if(LRU[index][i] < temp) {
			temp = LRU[index][i];
			LRused = i;
		}
	}

	LRU[index][LRused] = accCtr;

	//create copy of LRU block to store in VC with (tag+index) tagStore
	Block tempBlock;
	tempBlock.validBit = true;
	tempBlock.dirtyBit = cache.cacheBlocks[index][LRused].dirtyBit;
	uint64_t tagToStore = createVictimTagStore(cache.cacheBlocks[index][LRused].tagStore, index); //get longer tagStore
	tempBlock.tagStore = tagToStore;
	tempBlock.subBlocks = new bool[cache.numSubBlocks];
	for(int i = 0; i < cache.numSubBlocks; i++) {
		tempBlock.subBlocks[i] = cache.cacheBlocks[index][LRused].subBlocks[i];
	}

	//replace LRU block with vCache[vcIndex] block
	cache.cacheBlocks[index][LRused].validBit = true;
	cache.cacheBlocks[index][LRused].dirtyBit = vCache[vcIndex].dirtyBit;
	cache.cacheBlocks[index][LRused].tagStore = ((vCache[vcIndex].tagStore) >> (cache.C-cache.B-cache.S)); //maybe this needs to extract old tag out? >> length of index
	for(int i = 0; i < cache.numSubBlocks; i++) {
		cache.cacheBlocks[index][LRused].subBlocks[i] = vCache[vcIndex].subBlocks[i];
	}

	//place "victim" LRU block in vCache[vcIndex]
	vCache[vcIndex].validBit = true;
	vCache[vcIndex].dirtyBit = tempBlock.dirtyBit;
	vCache[vcIndex].tagStore = tempBlock.tagStore;
	for(int i = 0; i < cache.numSubBlocks; i++) {
		vCache[vcIndex].subBlocks[i] = tempBlock.subBlocks[i];
	}

	//record access counter for FIFO policy
	FIFOv[vcIndex] = accCtr;

	//delete dynamically allocated memory
	delete[] tempBlock.subBlocks;

}

void evictBlock(Block& evicted, uint64_t subBlockNum, char accType) {
	//evict block from victim cache, writing back dirty subblocks to memory
	int bytesPerTransfer = (1 << cache.K);

	if(evicted.dirtyBit == true) {    //was a writeback
		cache.stats.write_backs += 1;

		for(int sB = 0; sB < cache.numSubBlocks; sB++) { //write back any dirty subBlocks
			if(evicted.subBlocks[sB] == true) {
				cache.stats.bytes_transferred += bytesPerTransfer;
			}
		}
	}
}

void subBlockMissR(uint64_t subBlockNum, uint64_t wayIndex, uint64_t index) {
	//replace invalid subBlocks
	int bytesPerTransfer = (1 << cache.K);
	for(int sB = subBlockNum; sB < cache.numSubBlocks; ++sB) {
		if(cache.cacheBlocks[index][wayIndex].subBlocks[sB] == false) {
			cache.cacheBlocks[index][wayIndex].subBlocks[sB] = true;    //make invalid subBlocks valid
			cache.stats.bytes_transferred += bytesPerTransfer;          //transfer data in from memory
		}
	}
}

void subBlockMissW(uint64_t subBlockNum, uint64_t wayIndex, uint64_t index) {
	//replace invalid subBlocks, writing back valid subBlocks if dirtyBit == true
	int bytesPerTransfer = (1 << cache.K);
	for(int sB = subBlockNum; sB < cache.numSubBlocks; ++sB) {
		if(cache.cacheBlocks[index][wayIndex].subBlocks[sB] == false) {
			cache.cacheBlocks[index][wayIndex].subBlocks[sB] = true;    //make invalid subBlocks valid
			cache.stats.bytes_transferred += bytesPerTransfer;          //transfer data in from memory
		}
	}
	cache.cacheBlocks[index][wayIndex].dirtyBit = true;       //newly written data is only in cache
}

uint64_t createVictimTagStore(uint64_t tag, uint64_t index) {
	return (tag << (cache.C-cache.B-cache.S)) | index;
}

void print_statistics(cache_stats_t* p_stats) {
    printf("Cache Statistics\n");
    printf("Accesses: %" PRIu64 "\n", p_stats->accesses);
    printf("Reads: %" PRIu64 "\n", p_stats->reads);
    printf("Read misses: %" PRIu64 "\n", p_stats->read_misses);
    printf("Read misses combined: %" PRIu64 "\n", p_stats->read_misses_combined);
    printf("Writes: %" PRIu64 "\n", p_stats->writes);
    printf("Write misses: %" PRIu64 "\n", p_stats->write_misses);
    printf("Write misses combined: %" PRIu64 "\n", p_stats->write_misses_combined);
    printf("Misses: %" PRIu64 "\n", p_stats->misses);
    printf("Writebacks: %" PRIu64 "\n", p_stats->write_backs);
    printf("Victim cache misses: %" PRIu64 "\n", p_stats->vc_misses);
    printf("Sub-block misses: %" PRIu64 "\n", p_stats->subblock_misses);
    printf("Bytes transferred to/from memory: %" PRIu64 "\n", p_stats->bytes_transferred);
    printf("Hit Time: %f\n", p_stats->hit_time);
    printf("Miss Penalty: %f\n", p_stats->miss_penalty);
    printf("Miss rate: %f\n", p_stats->miss_rate);
    printf("Average access time (AAT): %f\n", p_stats->avg_access_time);
}
