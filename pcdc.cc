/*

 */

#include <map>
#include <queue>
#include "interface.hh"

#define NUM_ENTRIES 32768
#define PREFETCH_DEGREE 4
#define LOOKBACK_DEGREE 10

using namespace std;

typedef struct Ghb_entry{
  Ghb_entry(Addr pc_, Addr miss_address_) : pc(pc_), miss_address(miss_address_){
  }
  Addr pc;
  Addr miss_address;
  int link_ptr;  // points to an index in ring buffer
} Ghb_entry;

typedef struct Ghb{
  Ghb() : head(0), tail(0){
  }
  map<Addr, int> index_table; //Index table that maps a pc to an index in the GHB circular buffer

  int head;
  int tail;
  Ghb_entry* buf[NUM_ENTRIES];
} Ghb;

void ghb_put(Ghb* ghb, Ghb_entry* entry){
  ghb->buf[ghb->head] = entry;
  ghb->index_table[entry->pc] = ghb->head;
  ghb->head = (ghb->head+1) % NUM_ENTRIES;
  if(ghb->head == ghb->tail){
    delete ghb->buf[ghb->tail];
    ghb->tail = (ghb->tail+1) % NUM_ENTRIES;
  }
}

void issue_prefetch_check(Addr addr){
  if((addr <= MAX_PHYS_MEM_ADDR) && (current_queue_size() < MAX_QUEUE_SIZE) && (!in_cache(addr)) && (!in_mshr_queue(addr))){
    issue_prefetch(addr);
  }
}

void update_ghb(Ghb* ghb, Ghb_entry* entry){
  entry->link_ptr = ghb->index_table[entry->pc];
  ghb_put(ghb, entry);
  ghb->index_table[entry->pc] = ghb->head;
}

void prefetch_delta_correlation(Ghb* ghb, Addr pc){
  if(ghb->index_table.find(pc) == ghb->index_table.end()){
    return;
  }

  int deltas[LOOKBACK_DEGREE] = {0};
  Addr addr_to_be_prefetched[PREFETCH_DEGREE] = {0};

  Ghb_entry* current_entry = ghb->buf[ghb->index_table[pc]];
  if(current_entry->pc != pc){ // Check if index table is outdated
    ghb->index_table.erase(pc);
    return;
  }
  // Find deltas
  /*
  for(int i = LOOKBACK_DEGREE-1; i > -1; i--){
    Ghb_entry* next_entry = ghb->buf[current_entry->link_ptr];
    if(next_entry->pc == pc){
      int delta = current_entry->miss_address - next_entry->miss_address;
      deltas[i] = delta;
      current_entry = next_entry;
    }
    else{
      break;  // The nex element no longer holds a miss for the same pc
    }
  }
  */
  // Find correlation match in delta table
  /*
  int match = 0;
  int match_index;
  for(int i = 0; i < LOOKBACK_DEGREE-2; i+=2){
    if(deltas[i] == deltas[LOOKBACK_DEGREE-2] && deltas[i+1] == deltas[LOOKBACK_DEGREE-1]){
      match = 1;
      match_index = i+1;
      break;
    }
  }
  if(!match){ // Couldn't find pattern
    return;
  }
  */
  /*
  // Pick out addresses
  addr_to_be_prefetched[0] = pc + deltas[match_index];
  for(int i = 1; i < PREFETCH_DEGREE; i++){
    if(match_index + i < LOOKBACK_DEGREE && deltas[match_index+i] != 0){
      addr_to_be_prefetched[i] = addr_to_be_prefetched[i-1] + deltas[match_index+i];
    }
  }
  */
  /*
  for(int i = 0; i < PREFETCH_DEGREE; i++){
    if(addr_to_be_prefetched[i] != 0){
      issue_prefetch_check(addr_to_be_prefetched[i]);
    }
  }
  */
}

Ghb* ghb;

void prefetch_init(void){
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */
    ghb = new Ghb();
    DPRINTF(HWPrefetch, "Initialized sequential-on-access prefetcher\n");
}

void prefetch_access(AccessStat stat){
  if(stat.miss){
    Ghb_entry* insert = new Ghb_entry(stat.pc, stat.mem_addr);
    update_ghb(ghb, insert);
  }
  else if(get_prefetch_bit(stat.mem_addr)){
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
