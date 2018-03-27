/*
 Implementation of a prefetcher using a Global History Buffer and delta correlation
 */

#include <map>
#include <queue>
#include "interface.hh"

#define NUM_ENTRIES 256
#define INDEX_TABLE_SIZE 256
#define NUM_BLOCKS_PREFETCH 4
#define MATCH_DEGREE 3
#define LOOKBACK_DEGREE 12

using namespace std;

typedef struct Ghb_entry{
  Ghb_entry(Addr pc_, Addr miss_address_) : pc(pc_), miss_address(miss_address_), link_ptr(-1){
  }
  Addr pc;
  Addr miss_address;
  int link_ptr;  // points to an index in ring buffer
} Ghb_entry;

typedef struct Ghb{
  Ghb() : head(0), current_indexes(0), buf(){  // Had to initialize buf with Null pointers to avoid weird crash
  }
  map<Addr, int> index_table; //Index table that maps a pc to an index in the GHB circular buffer
  queue<Addr> index_queue;

  int head;
  int current_indexes;
  Ghb_entry* buf[NUM_ENTRIES];
} Ghb;

void issue_prefetch_check(Addr addr){
  if((addr <= MAX_PHYS_MEM_ADDR) && (current_queue_size() < MAX_QUEUE_SIZE) && (!in_cache(addr)) && (!in_mshr_queue(addr))){
    issue_prefetch(addr);
  }
}

void update_ghb(Ghb* ghb, Ghb_entry* entry){
  if(ghb->index_table.find(entry->pc) == ghb->index_table.end()){  // Not found in index table
    switch(ghb->current_indexes){
      case INDEX_TABLE_SIZE:
        ghb->index_table.erase(ghb->index_queue.front());
        ghb->index_queue.pop();
        break;
      default:
        ghb->current_indexes++;
        break;
    }
  }

  ghb->index_queue.push(entry->pc);
  entry->link_ptr = ghb->index_table[entry->pc];
  delete ghb->buf[ghb->head];
  ghb->index_table[entry->pc] = ghb->head;
  ghb->buf[ghb->head] = entry;
  ghb->head = (ghb->head+1) % NUM_ENTRIES;
}

void prefetch_delta_correlation(Ghb* ghb, Addr pc){
  if(ghb->index_table.find(pc) == ghb->index_table.end()){ // Not entered pc in index table
    return;
  }

  int deltas[LOOKBACK_DEGREE] = {0};
  Addr addr_to_be_prefetched[NUM_BLOCKS_PREFETCH] = {0};

  Ghb_entry* current_entry = ghb->buf[ghb->index_table[pc]];
  if(current_entry->pc != pc){ // Check if index table entry is outdated
    return;
  }
  Ghb_entry* next_entry;

  // Find deltas
  for(int i = LOOKBACK_DEGREE-1; i > -1; i--){
    next_entry = ghb->buf[current_entry->link_ptr];
    if(next_entry->pc == pc && next_entry!= current_entry){
      int delta = current_entry->miss_address - next_entry->miss_address;
      deltas[i] = delta;
      current_entry = next_entry;
    }
    else{
      break;  // The nex element no longer holds a miss for the same pc
    }
  }

  // Find correlation match in delta table
  int match_index = -1;
  for(int i = 0; i < LOOKBACK_DEGREE-MATCH_DEGREE; i++){
    if(deltas[i] == deltas[LOOKBACK_DEGREE-2] && deltas[i+1] == deltas[LOOKBACK_DEGREE-1]){
      match_index = i+1;
      break;
    }
  }
  if(match_index == -1){ // Couldn't find pattern
    return;
  }

  // Pick out addresses
  addr_to_be_prefetched[0] = ghb->buf[ghb->index_table[pc]]->miss_address + deltas[match_index];
  for(int i = 1; i < NUM_BLOCKS_PREFETCH; i++){
    if(match_index + i < LOOKBACK_DEGREE && deltas[match_index+i] != 0){
      addr_to_be_prefetched[i] = addr_to_be_prefetched[i-1] + deltas[match_index+i];
    }
  }
  for(int i = 0; i < NUM_BLOCKS_PREFETCH; i++){
    if(addr_to_be_prefetched[i]){
      issue_prefetch_check(addr_to_be_prefetched[i]);
    }
  }
}

Ghb* ghb;

void prefetch_init(void){
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */
    ghb = new Ghb();
    DPRINTF(HWPrefetch, "Initialized global history buffer pcdc prefetcher\n");
}

void prefetch_access(AccessStat stat){

  if(stat.miss){
    Ghb_entry* insert = new Ghb_entry(stat.pc, stat.mem_addr);
    update_ghb(ghb, insert);
  }
  else if(!stat.miss && get_prefetch_bit(stat.mem_addr)){
    clear_prefetch_bit(stat.mem_addr);
    Ghb_entry* insert = new Ghb_entry(stat.pc, stat.mem_addr);
    update_ghb(ghb, insert);
  }
  prefetch_delta_correlation(ghb, stat.pc);
}

void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
     set_prefetch_bit(addr);
}
