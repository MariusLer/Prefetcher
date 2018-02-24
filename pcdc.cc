/*

 */

#include <map>
#include <queue>
#include "interface.hh"


#include <iostream> // remove this

#define NUM_ENTRIES 1024
#define INDEX_TABLE_SIZE 256
#define PREFETCH_DEGREE 4
#define LOOKBACK_DEGREE 12

using namespace std;

int NUM_MATCHES = 0;

typedef struct Ghb_entry{
  Ghb_entry(Addr pc_, Addr miss_address_) : pc(pc_), miss_address(miss_address_), link_ptr(-1){
  }
  Addr pc;
  Addr miss_address;
  int link_ptr;  // points to an index in ring buffer
} Ghb_entry;

typedef struct Ghb{
  Ghb() : head(0), current_indexes(0){
  }
  map<Addr, int> index_table; //Index table that maps a pc to an index in the GHB circular buffer
  queue<Addr> index_queue;

  int head;
  //int tail;
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
  entry->link_ptr = ghb->index_table[entry->pc];
  ghb->index_queue.push(entry->pc);

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
  Addr addr_to_be_prefetched[PREFETCH_DEGREE] = {0};

  Ghb_entry* current_entry = ghb->buf[ghb->index_table[pc]];

  /*
  cout <<"Cur:" << current_entry->miss_address;
  Ghb_entry* next_entry = ghb->buf[current_entry->link_ptr];
  if(next_entry->pc == pc){
    cout << "Next " << next_entry->miss_address << endl;
  }
  */


  if(current_entry->pc != pc){ // Check if index table entry is outdated
    return;
  }
  Ghb_entry* next_entry;

  // Find deltas
  //cout << "PC " << pc;
  for(int i = LOOKBACK_DEGREE-1; i > -1; i--){
    next_entry = ghb->buf[current_entry->link_ptr];
    //cout << " Next entry pc " << next_entry->pc;
    if(next_entry->pc == pc && next_entry!= current_entry){
      int delta = current_entry->miss_address - next_entry->miss_address;
      //cout << delta;
      deltas[i] = delta;
      current_entry = next_entry;
    }
    else{
      break;  // The nex element no longer holds a miss for the same pc
    }
  }
  //cout << endl;

  /*
  for(int i = 0; i < LOOKBACK_DEGREE; i++){
    cout << deltas[i] << " ";
  } cout << endl;
*/


  // Find correlation match in delta table
  int match = 0;
  int match_index;
  for(int i = 0; i < LOOKBACK_DEGREE-2; i+=2){
    if(deltas[i] == deltas[LOOKBACK_DEGREE-2] && deltas[i+1] == deltas[LOOKBACK_DEGREE-1]){
      match = 1;
      match_index = i+1;
      break;
    }
  }
  //DPRINTF(HWPrefetch,"Match :\n", match);
  if(!match){ // Couldn't find pattern
    return;
  }

  // Pick out addresses
  addr_to_be_prefetched[0] = ghb->buf[ghb->index_table[pc]]->miss_address + deltas[match_index];
  for(int i = 1; i < PREFETCH_DEGREE; i++){
    if(match_index + i < LOOKBACK_DEGREE && deltas[match_index+i] != 0){
      addr_to_be_prefetched[i] = addr_to_be_prefetched[i-1] + deltas[match_index+i];
    }
  }
  //cout << "PC :" << pc << " Prefetch addresses: ";
  for(int i = 0; i < PREFETCH_DEGREE; i++){
    if(addr_to_be_prefetched[i] != 0){
      //cout << addr_to_be_prefetched[i] << " ";
      issue_prefetch_check(addr_to_be_prefetched[i]);
    }
  }
  //cout << endl;
}

Ghb* ghb;

void prefetch_init(void){
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */
    ghb = new Ghb();
    DPRINTF(HWPrefetch, "Initialized sequential-on-access prefetcher\n");
}

void prefetch_access(AccessStat stat){
  //cout << ghb->index_table[stat.pc] << endl;
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
