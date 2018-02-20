/*
  Simple implementation of a prefetcher using and rpt prefetching scheme
 */
#include <map>
#include <stdio.h>
#include "interface.hh"

#define NUM_ENTRIES 256

using namespace std;

struct Rpt_entry{
  Rpt_entry(Addr addr, Addr pc) : pc(pc), prev_addr(addr), stride(0), next(NULL), prev(NULL){
  }
  Addr pc; // Only really needed when deleting an element from the table
  Addr prev_addr;
  int64_t stride;
  struct Rpt_entry* next; // The entry entered after
  struct Rpt_entry* prev; // The entry entered before
};

struct Rpt_table{
  Rpt_table() : num_current_entries(0), head(NULL), tail(NULL){
  }
  int num_current_entries;
  struct Rpt_entry* head;
  struct Rpt_entry* tail;
  map<Addr, struct Rpt_entry*> table;
};

struct Rpt_table* rpt_table;

void issue_prefetch_check(Addr addr){
  if((addr <= MAX_PHYS_MEM_ADDR) && (current_queue_size() < MAX_QUEUE_SIZE) && (!in_cache(addr)) && (!in_mshr_queue(addr))){
    issue_prefetch(addr);
  }
}

void insert_new_entry(struct Rpt_table* table, struct Rpt_entry* entry, Addr pc){
  switch(table->num_current_entries){
    case NUM_ENTRIES:
      table->table.erase(table->tail->pc);
      table->tail = table->tail->next;
      delete table->tail->prev;
      table->tail->prev = NULL;
      table->head->next = entry;
      entry->prev = table->head;
      table-> head = entry;
      table->table[pc] = entry;
      break;
    case 0:
      table->num_current_entries++;
      table-> head = table->tail = entry;
      table->table[pc] = entry;
      break;
    default:
      table->num_current_entries++;
      table->head->next = entry;
      entry->prev = table->head;
      table-> head = entry;
      table->table[pc] = entry;
      break;
  }
}

void prefetch_init(void){
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */
    rpt_table = new Rpt_table();
    DPRINTF(HWPrefetch, "Initialized rpt prefetcher\n");
}

void prefetch_access(AccessStat stat){
  if(rpt_table->table.find(stat.pc) == rpt_table->table.end()){
    struct Rpt_entry* new_entry = new Rpt_entry(stat.mem_addr, stat.pc);
    insert_new_entry(rpt_table, new_entry, stat.pc);
    return;
  }
  struct Rpt_entry* current_entry = rpt_table->table[stat.pc];
  current_entry->stride = stat.mem_addr - current_entry->prev_addr;
  current_entry->prev_addr = stat.mem_addr;
  issue_prefetch_check(current_entry->prev_addr + current_entry->stride);
}

void prefetch_complete(Addr addr){
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
}
