/*
 Implementation of a prefetcher using Delta Correlation Prediction Table
 */

#include <map>
#include <queue>
#include "interface.hh"

#define NUM_DELTAS 8
#define NUM_ENTRIES 256

using namespace std;

typedef struct Dcpt_entry{
  Dcpt_entry(Addr pc_, Addr addr) : pc(pc_), last_addr(addr), last_prefetch(0), deltas(), delta_ptr(0){
  }
  Addr pc;
  Addr last_addr;
  Addr last_prefetch;
  int32_t deltas[NUM_DELTAS];
  uint8_t delta_ptr;
} Dcpt_entry;

typedef struct Dcpt_table{
  int current_num_entries;
  map<Addr, struct Dcpt_entry*> dcpt_map;
  queue<Addr> dcpt_queue;
} Dcpt_table;

int issue_prefetch_check(Addr addr){
  if((addr <= MAX_PHYS_MEM_ADDR) && (current_queue_size() < MAX_QUEUE_SIZE) && (!in_cache(addr)) && (!in_mshr_queue(addr))){
    issue_prefetch(addr);
    return 1;
  }
  return 0;
}

void add_dcpt_entry(Dcpt_entry* new_entry, Dcpt_table* table){
  switch(table->current_num_entries){
    case NUM_ENTRIES:
      Addr pc_to_be_deleted;
      pc_to_be_deleted = table->dcpt_queue.front();
      delete table->dcpt_map[pc_to_be_deleted];
      table->dcpt_map.erase(pc_to_be_deleted);
      table->dcpt_queue.pop();
      break;
    default:
      table->current_num_entries++;
      break;
  }
  table->dcpt_queue.push(new_entry->pc);
  table->dcpt_map[new_entry->pc] = new_entry;
}

void update_dcpt_entry(Addr pc, Addr prefetch_address, Dcpt_table* table){ // Updates last_addr and deltas and delta ptr
  Dcpt_entry* entry = table->dcpt_map[pc];
  int delta = prefetch_address - entry->last_addr;
  if(!delta){
    return;
  }
  /*
  if(delta >= 32768 || delta < -32768){  // Check if overflow
    delta = 0;
  }
  */
  entry->deltas[entry->delta_ptr++] = delta;
  if(entry->delta_ptr == NUM_DELTAS){
    entry->delta_ptr = 0;
  }
  entry->last_addr = prefetch_address;
}

void dcpt_prefetch(Addr pc, Dcpt_table* table){
  Dcpt_entry* entry = table->dcpt_map[pc];
  Addr prefetch_buffer[NUM_DELTAS] = {0};

  // Puts the deltas from oldest to newest into an ordered list
  int iterator = entry->delta_ptr;
  int current_index = 0;
  int ordered_deltas[NUM_DELTAS];
  for(int i = 0; i < NUM_DELTAS; i++){
    ordered_deltas[current_index++] = entry->deltas[iterator++];
    if(iterator == NUM_DELTAS){
      iterator = 0;
    }
  }

  // Find delta match if any
  int match_index = -1;
  /*  Backwards search
  for(int i = NUM_DELTAS - 3; i > 0; i--){
    if ((ordered_deltas[i] == ordered_deltas[NUM_DELTAS-1]) && (ordered_deltas[i-1] == ordered_deltas[NUM_DELTAS-2])){
      match_index = i+1;
      break;
    }
  }
  */
  // Forwards search
  for(int i = 0; i < NUM_DELTAS-3; i++){
    if ((ordered_deltas[i] == ordered_deltas[NUM_DELTAS-2]) && (ordered_deltas[i+1] == ordered_deltas[NUM_DELTAS-1])){
      match_index = i+2;
      break;
    }
  }
  if(match_index == -1){
    return;
  }

  // Add up the addresses in prefetch buffer
  int prefetch_valid_after = 0;  // To invalidate the prefetches in the buffer
  int prefetch_index = 1;
  prefetch_buffer[0] = entry->last_addr + ordered_deltas[match_index];
  for(int i = match_index + 1; i < NUM_DELTAS; i++){
    prefetch_buffer[prefetch_index] = prefetch_buffer[prefetch_index - 1] + ordered_deltas[i];
    if(prefetch_buffer[prefetch_index] == entry->last_prefetch){
      prefetch_valid_after = prefetch_index + 1;
    }
    prefetch_index++;
  }

  // Actually do the prefetches if valid
  for(int i = prefetch_valid_after; i < NUM_DELTAS; i++){
    if(prefetch_buffer[i] && prefetch_buffer[i] != entry->last_addr){
      if(issue_prefetch_check(prefetch_buffer[i])){
        entry->last_prefetch = prefetch_buffer[i];
      }
    }
  }
}

Dcpt_table* table;

void prefetch_init(void){
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */
    table = new Dcpt_table();
    DPRINTF(HWPrefetch, "Initialized DCPT prefetcher\n");
}

void prefetch_access(AccessStat stat){
  if(table->dcpt_map.find(stat.pc) == table->dcpt_map.end()){
    Dcpt_entry* new_entry = new Dcpt_entry(stat.pc, stat.mem_addr);
    add_dcpt_entry(new_entry, table);
    return;
  }
  update_dcpt_entry(stat.pc, stat.mem_addr, table);
  dcpt_prefetch(stat.pc, table);

}

void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
}
