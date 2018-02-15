/*
 * A sample prefetcher which does sequential one-block lookahead.
 * This means that the prefetcher fetches the next block _after_ the one that
 * was just accessed. It also ignores requests to blocks already in the cache.
 */
#include <map>
#include "interface.hh"


using namespace std;

struct Rtp_entry{
  //Addr tag;  // Program counter or LA-PC use tag as key in map
  Rtp_entry(Addr addr) : prev_addr(addr), stride(0) /*state(initial)*/{
  }
  Addr prev_addr;
  int64_t stride;
};

unordered_map<Addr, struct Rtp_entry*> table;

void issue_prefetch_check(Addr addr){
  if((addr <= MAX_PHYS_MEM_ADDR) && (current_queue_size() < MAX_QUEUE_SIZE) && (!in_cache(addr)) && (!in_mshr_queue(addr))){
    issue_prefetch(addr);
  }
}

void prefetch_init(void){
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */

    DPRINTF(HWPrefetch, "Initialized sequential-on-access prefetcher\n");
}

void prefetch_access(AccessStat stat){
  if(table.find(stat.pc) == table.end()){
    struct Rtp_entry* new_entry = new Rtp_entry(stat.mem_addr);
    table[stat.pc] = new_entry;
    return;
  }
  struct Rtp_entry* current_entry = table[stat.pc];
  current_entry->stride = stat.mem_addr - current_entry->prev_addr;
  current_entry->prev_addr = stat.mem_addr;
  issue_prefetch_check(current_entry->prev_addr + current_entry->stride);
}

void prefetch_complete(Addr addr){
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
}
