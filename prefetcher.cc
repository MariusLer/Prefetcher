/*
 * A sample prefetcher which does sequential one-block lookahead.
 * This means that the prefetcher fetches the next block _after_ the one that
 * was just accessed. It also ignores requests to blocks already in the cache.
 */
#include <map>
#include "interface.hh"



using namespace std;

enum State{
  initial,
  transient,
  steady,
  no_prediction
};

struct Rtp_entry{
  //A ddr tag;  // Program counter or LA-PC use tag as key in map
  Addr prev_addr;
  int64_t stride;
  State state;
};

struct Rtp_table{
  map<Addr, struct Rtp_entry> rtp_map;
};

struct Rtp_table table;

void prefetch_init(void){
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */

    DPRINTF(HWPrefetch, "Initialized sequential-on-access prefetcher\n");
}

void prefetch_access(AccessStat stat){
  if(table.rtp_map.find(stat.pc) == table.rtp_map.end()){
    struct Rtp_entry insert{stat.mem_addr, 0, initial};
    table.rtp_map[stat.pc] = insert;
  }
  else{
    struct Rtp_entry current_entry = table.rtp_map[stat.pc];

    if(stat.miss){
      switch(current_entry.state){
        case initial:
          current_entry.stride = stat.mem_addr - current_entry.prev_addr;
          current_entry.prev_addr = stat.mem_addr;
          current_entry.state = transient;
          break;
        case steady:
          current_entry.prev_addr = stat.mem_addr;
          current_entry.state = initial;
          break;
        case transient:
          current_entry.stride = stat.mem_addr - current_entry.prev_addr;
          current_entry.prev_addr = stat.mem_addr;
          current_entry.state = no_prediction;
          break;
        case no_prediction:
          current_entry.stride = stat.mem_addr - current_entry.prev_addr;
          current_entry.prev_addr = stat.mem_addr;
          break;
      }
    }
    else{
      switch(current_entry.state){
        case initial:
        case steady:
        case transient:
          current_entry.prev_addr = stat.mem_addr;
          current_entry.state = steady;
          break;
        case no_prediction:
          current_entry.prev_addr = stat.mem_addr;
          current_entry.state = transient;
          break;
      }
    }
    if(current_entry.state != no_prediction){
      issue_prefetch(current_entry.prev_addr + current_entry.stride);
    }
    table.rtp_map[stat.pc] = current_entry;
  }
}

void prefetch_complete(Addr addr){
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
}
