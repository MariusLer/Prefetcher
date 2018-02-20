/*

 */

#include <map>
#include <queue>
#include "interface.hh"

#define NUM_ENTRIES = 1024

using namespace std;

class circular_buffer{
public:

};



struct Ghb_entry{


  Addr miss_address;
  struct Ghb_entry* link_ptr;
};

struct Ghb{
  int current_num_entries;
  struct Ghb_entry* head;

  map<Addr,struct Ghb_entry> index_table;
  queue<struct Ghb_entry*> buffer; // Holds NUM_ENTRIES number of recent misses
};

void Ghb_insert(struct Ghb* ghb, struct Ghb_entry* entry, Addr pc){
  switch(ghb->current_num_entries){
    case NUM_ENTRIES:
      entry->link_ptr = index_table[pc];
      ghb->index_table[pc] = entry;
      ghb->buffer.pop();
      ghb->buffer.push(entry);
      ghb->head = entry;


  }

}


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

}

void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
}
