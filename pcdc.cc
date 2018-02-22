/*

 */

#include <map>
#include <queue>
#include "interface.hh"

#define NUM_ENTRIES = 32768
#define NUM_INDEXES

using namespace std;

typedef struct Ghb_entry{
  Ghb_entry(Addr pc_, Addr miss_address_, Ghb_entry* link_ptr_) : pc(pc_), miss_address(miss_address_){
  }
  Addr pc;
  Addr miss_address;
  int link_ptr;  // points to an index in ring buffer
} Ghb_entry;

typedef struct Ghb{
  map<Addr, int> index_table; //Index table that maps a pc to an index in the GHB circular buffer
  Circular_buffer buf; // Holds NUM_ENTRIES number of recent misses

  int head;
  int tail;
  Ghb_entry* buf[NUM_ENTRIES]
} Ghb;

class Circular_buffer{
public:
  Circular_buffer(): head(0), tail(0), size(NUM_ENTRIES){
  }
  bool is_empty(){
    return(head == tail);
  }
  bool is_full(){
    return ((head+1) % size) == tail);
  }
  int get_head(){
    return head;
  }
  int get_size(){
    return size;
  }
  int put(Ghb_entry* entry){ // Adding entry and if we are full delete tail entry and imcrement tail
    buf[head] = entry;
    int prev_head = head;
    head = (head+1) % size;
    if(head == tail){
      delete buf[tail];
      tail = (tail+1) % size;
    }
    return head;
  }
  Ghb_entry* get(int index){
    return buf[index];
  }
private:

};

void issue_prefetch_check(Addr addr){
  if((addr <= MAX_PHYS_MEM_ADDR) && (current_queue_size() < MAX_QUEUE_SIZE) && (!in_cache(addr)) && (!in_mshr_queue(addr))){
    issue_prefetch(addr);
  }
}

Ghb* ghb;

void update_ghb(Ghb* ghb, Ghb_entry* entry){
  entry->link_ptr = ghb->index_table[entry->pc];
  int head = ghb->buf->put(entry);
  ghb->index_table[entry.pc] = head;
}


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

}

void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
}
