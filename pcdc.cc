/*

 */

#include <map>
#include "interface.hh"

#define NUM_DELTAS = 16
#define NUM_ENTRIES = 512

using namespace std;

struct Dcpt_entry{
  Dcpt_entry(Addr addr, struct Dcpt_entry* prev_i) : last_addr(addr), prev(prev_i), last_prefetch(0), deltas({0}){
  }
  Addr last_addr;
  Addr last_prefetch;
  int deltas[NUM_DELTAS];
  int delta_ptr;
  struct Dcpt_entry* next;
  struct Dcpt_entry* prev;
};

struct Dcpt_table{
  int current_num_entries;
  struct Dcpt_entry* head;
  struct Dcpt_entry* tail;

  map<Addr, struct Dcpt_entry*> dcpt_map;
};




void issue_prefetch_check(Addr addr){
  if((addr <= MAX_PHYS_MEM_ADDR) && (current_queue_size() < MAX_QUEUE_SIZE) && (!in_cache(addr)) && (!in_mshr_queue(addr))){
    issue_prefetch(addr);
  }
}

void prefetch_init(void)
{
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */

    DPRINTF(HWPrefetch, "Initialized sequential-on-access prefetcher\n");
}

void prefetch_access(AccessStat stat)
{
    /* pf_addr is now an address within the _next_ cache block */
    Addr pf_addr = stat.mem_addr + BLOCK_SIZE;

    /*
     * Issue a prefetch request if a demand miss occured,
     * and the block is not already in cache.
     */
    if (stat.miss && !in_cache(pf_addr)) {
      issue_prefetch(pf_addr);
    }
}

void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
}
