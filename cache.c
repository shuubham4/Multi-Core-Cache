#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "cache.h"
#include "main.h"

/* cache configuration parameters */
static int cache_usize = DEFAULT_CACHE_SIZE;
static int cache_block_size = DEFAULT_CACHE_BLOCK_SIZE;
static int words_per_block = DEFAULT_CACHE_BLOCK_SIZE / WORD_SIZE;
static int cache_assoc = DEFAULT_CACHE_ASSOC;
static int cache_writeback = DEFAULT_CACHE_WRITEBACK;
static int cache_writealloc = DEFAULT_CACHE_WRITEALLOC;
static int num_core = DEFAULT_NUM_CORE;

/* cache model data structures */
/* max of 8 cores */
static cache* mesi_cache[8];
static cache_stat* mesi_cache_stat[8];

/************************************************************/
void set_cache_param(param, value)
  int param;
  int value;
{
  switch (param) {
  case NUM_CORE:
    num_core = value;
    break;
  case CACHE_PARAM_BLOCK_SIZE:
    cache_block_size = value;
    words_per_block = value / WORD_SIZE;
    break;
  case CACHE_PARAM_USIZE:
    cache_usize = value;
    break;
  case CACHE_PARAM_ASSOC:
    cache_assoc = value;
    break;
  default:
    printf("error set_cache_param: bad parameter value\n");
    exit(-1);
  }
}
/************************************************************/

/************************************************************/
void init_cache()
{
  int i,j;
  for (j = 0; j < num_core; j++){
        mesi_cache[j] = (cache*)malloc(sizeof(cache));
        mesi_cache[j]->id = j;
        mesi_cache[j]->size = cache_usize;
        mesi_cache[j]->associativity = cache_assoc;
        mesi_cache[j]->n_sets = (cache_usize)/(cache_block_size*cache_assoc);
        mesi_cache[j]->set_contents = (int*)malloc(sizeof(int)*mesi_cache[j]->n_sets);
        mesi_cache[j]->LRU_head = (Pcache_line*)malloc(sizeof(Pcache_line)*mesi_cache[j]->n_sets);
        for (i = 0; i < mesi_cache[j]->n_sets; i++){
            mesi_cache[j]->LRU_head[i] = NULL;
            mesi_cache[j]->set_contents[i] = 0;
        }
        mesi_cache[j]->LRU_tail = (Pcache_line*)malloc(sizeof(Pcache_line)*mesi_cache[j]->n_sets);
        for (i = 0; i < mesi_cache[j]->n_sets; i++){
            mesi_cache[j]->LRU_tail[i] = NULL;
        }
        mesi_cache[j]->index_mask_offset = LOG2(cache_block_size);
        mesi_cache[j]->index_mask = ((1<<LOG2(mesi_cache[j]->n_sets)) - 1)<<(mesi_cache[j]->index_mask_offset);
  

  	mesi_cache_stat[j] = (cache_stat*)malloc(sizeof(cache_stat));
  	mesi_cache_stat[j]->accesses = 0;
  	mesi_cache_stat[j]->misses = 0;
  	mesi_cache_stat[j]->replacements = 0;
  	mesi_cache_stat[j]->demand_fetches = 0;
  	mesi_cache_stat[j]->copies_back = 0;
  	mesi_cache_stat[j]->broadcasts = 0;
  }
}
void perform_access(addr, access_type, pid)
     unsigned addr, access_type, pid;
   
{
       unsigned tag1;
       unsigned index;
       int match = 0, i;
       tag1 = addr>>(mesi_cache[pid]->index_mask_offset+LOG2(mesi_cache[pid]->n_sets));
       index = (addr & mesi_cache[pid]->index_mask)>>mesi_cache[pid]->index_mask_offset;
       Pcache_line temp, temp_insert, temp1;
       temp_insert = (cache_line*)malloc(sizeof(cache_line));
       temp_insert->state = 0; 
       temp_insert->tag = tag1;
       temp_insert->LRU_next = NULL;
       temp_insert->LRU_prev = NULL;

       if (access_type == 0){
           temp = mesi_cache[pid]->LRU_head[index];
           while (temp != NULL){ 
              if (temp->tag == tag1 && temp->state != 4){ 
                 (mesi_cache_stat[pid])->accesses++;   
                 temp_insert->state = temp->state;
                 insert(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), temp_insert);
                 delete(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), temp);
                 match = 1;
                 break;
              }
              temp = temp->LRU_next;
           }

           if (match == 0){  
                mesi_cache_stat[pid]->misses++;  
                mesi_cache_stat[pid]->broadcasts++;

                for (i = 0; i < num_core ; i++){ 
                    if (i == pid)  { continue;}
                    temp = mesi_cache[i]->LRU_head[index];
                    while (temp != NULL){ 
                          if (temp->tag == tag1 && temp->state != 4){  
                             match = 1;
                             (mesi_cache_stat[pid])->accesses++; 
                             temp_insert->state = 3;
                             mesi_cache_stat[pid]->demand_fetches += (cache_block_size)/4;
                             if (temp->state == 1) mesi_cache_stat[i]->copies_back += (cache_block_size)/4;
                             temp->state = 3;
                	     if (mesi_cache[pid]->set_contents[index] == mesi_cache[pid]->associativity){
                                 mesi_cache_stat[pid]->replacements++;
                                 insert(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), temp_insert);
                                 if (mesi_cache[pid]->LRU_tail[index]->state == 1)
                                     mesi_cache_stat[pid]->copies_back += (cache_block_size)/4;
                                 delete(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), mesi_cache[pid]->LRU_tail[index]);
                             }
                             else if (mesi_cache[pid]->set_contents[index] < mesi_cache[pid]->associativity) {
                                     mesi_cache[pid]->set_contents[index]++;
                                     insert(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), temp_insert);
                             }
                             break;              
                          }      
                          temp = temp->LRU_next;
                    }
                    //if (match == 0) mesi_cache_stat[i]->misses++;  // this may not check all caches.. 
                    if (match == 1) break; // this might be wrong   
                }
           }

           if (match == 0){
              (mesi_cache_stat[pid])->accesses++;
              mesi_cache_stat[pid]->demand_fetches += (cache_block_size)/4;
              temp_insert->state = 2;
              if (mesi_cache[pid]->set_contents[index] == mesi_cache[pid]->associativity){
                  mesi_cache_stat[pid]->replacements++;
                  insert(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), temp_insert);
                  if (mesi_cache[pid]->LRU_tail[index]->state == 1)
                      mesi_cache_stat[pid]->copies_back += (cache_block_size)/4;
                  delete(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), mesi_cache[pid]->LRU_tail[index]);
              }
              else if (mesi_cache[pid]->set_contents[index] < mesi_cache[pid]->associativity) {
                      mesi_cache[pid]->set_contents[index]++;
                      insert(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), temp_insert);
              }
           }
       } // access_type == 0 

       if (access_type == 1){
               temp = mesi_cache[pid]->LRU_head[index];
               while(temp != NULL){  
                  if (temp->tag == tag1 && temp->state != 4){
                     (mesi_cache_stat[pid])->accesses++;
                     temp_insert->state = 1;
                     insert(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), temp_insert);
                     delete(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), temp);
                     match = 1;
                     if (temp->state == 3){ 
                        mesi_cache_stat[pid]->broadcasts++;
                        // may need multiple broadcasts here  
                        for (i = 0; i < num_core ; i++){ 
                            if (i == pid) {continue;}
                            temp1 = mesi_cache[i]->LRU_head[index];
                            while (temp1 != NULL){ 
                                  if (temp1->tag == tag1){
                                      temp1->state = 4;
                                      delete(&(mesi_cache[i]->LRU_head[index]), &(mesi_cache[i]->LRU_tail[index]), temp1);
                                      mesi_cache[i]->set_contents[index]--;
                                  }
                                  temp1 = temp1->LRU_next;
                            }
                        } 
                     } 
                     break;
                  }
                  temp = temp->LRU_next;
               }

               if (match == 0){
                  mesi_cache_stat[pid]->misses++;
                  mesi_cache_stat[pid]->broadcasts++;
                  for (i = 0; i < num_core ; i++){ 
                      if (i == pid)  { continue;}
                      temp = mesi_cache[i]->LRU_head[index];
                      while (temp != NULL){  
                          if (temp->tag == tag1 && temp->state != 4){
                             match = 1;
                             temp_insert->state = 1; 
                             //mesi_cache_stat[pid]->demand_fetches += (cache_block_size)/4; // bug here .. do this only once ..
                             temp->state = 4;
                             delete(&(mesi_cache[i]->LRU_head[index]), &(mesi_cache[i]->LRU_tail[index]), temp);
                             mesi_cache[i]->set_contents[index]--; 
                          }
                          temp = temp->LRU_next;    
                      } 
                      //if (match == 0) mesi_cache_stat[i]->misses++; // this checks all caches.              
                  }
                  if (match == 1) {
                    mesi_cache_stat[pid]->accesses++;
                    mesi_cache_stat[pid]->demand_fetches += (cache_block_size)/4;
                    if (mesi_cache[pid]->set_contents[index] == mesi_cache[pid]->associativity){
                        mesi_cache_stat[pid]->replacements++;
                        insert(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), temp_insert);
                        if (mesi_cache[pid]->LRU_tail[index]->state == 1)
                            mesi_cache_stat[pid]->copies_back += (cache_block_size)/4;
                        delete(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), mesi_cache[pid]->LRU_tail[index]);
                    }
                    else if (mesi_cache[pid]->set_contents[index] < mesi_cache[pid]->associativity) {
                            mesi_cache[pid]->set_contents[index]++;
                            insert(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), temp_insert);
                    }  
                  }
               }

               if (match == 0){
                   mesi_cache_stat[pid]->accesses++;
                   mesi_cache_stat[pid]->demand_fetches += (cache_block_size)/4;
                   temp_insert->state = 1; 
                   if (mesi_cache[pid]->set_contents[index] == mesi_cache[pid]->associativity){
                       mesi_cache_stat[pid]->replacements++;
                       insert(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), temp_insert);
                       if (mesi_cache[pid]->LRU_tail[index]->state == 1)
                          mesi_cache_stat[pid]->copies_back += (cache_block_size)/4;
                       delete(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), mesi_cache[pid]->LRU_tail[index]);
                   }
                   else if (mesi_cache[pid]->set_contents[index] < mesi_cache[pid]->associativity) {
                           mesi_cache[pid]->set_contents[index]++;
                           insert(&(mesi_cache[pid]->LRU_head[index]), &(mesi_cache[pid]->LRU_tail[index]), temp_insert);
                   }
               }        
       } // access_type == 1*/
}

void flush()
{
     int i = 0, j = 0;
     Pcache_line temp;

     for (j = 0; j < num_core; j++){ 
        for (i = 0; i < mesi_cache[j]->n_sets; i++){ 
            temp = mesi_cache[j]->LRU_head[i];
            while(temp){ 
                 if (temp->state == 1){
                    mesi_cache_stat[j]->copies_back += (cache_block_size)/4;
                 }
                 temp = temp->LRU_next;
            }
        }
     }
     for (j = 0; j < num_core; j++){ 
          free(mesi_cache[j]->set_contents);
          free(mesi_cache[j]->LRU_head);
          free(mesi_cache[j]->LRU_tail);
          free(mesi_cache[j]);
     }
}



void delete(head, tail, item)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  if (item->LRU_prev) {
    item->LRU_prev->LRU_next = item->LRU_next;
  } else {
    /* item at head */
    *head = item->LRU_next;
  }

  if (item->LRU_next) {
    item->LRU_next->LRU_prev = item->LRU_prev;
  } else {
    /* item at tail */
    *tail = item->LRU_prev;
  }
}
/************************************************************/

/************************************************************/
/* inserts at the head of the list */
void insert(head, tail, item)
  Pcache_line *head, *tail;
  Pcache_line item;
{
  item->LRU_next = *head;
  item->LRU_prev = (Pcache_line)NULL;

  if (item->LRU_next)
    item->LRU_next->LRU_prev = item;
  else
    *tail = item;

  *head = item;
}
/************************************************************/

/************************************************************/
void dump_settings()
{
  printf("Cache Settings:\n");
  printf("\tSize: \t%d\n", cache_usize);
  printf("\tAssociativity: \t%d\n", cache_assoc);
  printf("\tBlock size: \t%d\n", cache_block_size);
}
/************************************************************/

/************************************************************/
void print_stats()
{
  int i;
  int demand_fetches = 0;
  int copies_back = 0;
  int broadcasts = 0;

  printf("*** CACHE STATISTICS ***\n");

  for (i = 0; i < num_core; i++) {
    printf("  CORE %d\n", i);
    printf("  accesses:  %d\n", mesi_cache_stat[i]->accesses);
    printf("  misses:    %d\n", mesi_cache_stat[i]->misses);
    printf("  miss rate: %f (%f)\n", 
	   (float)mesi_cache_stat[i]->misses / (float)mesi_cache_stat[i]->accesses,
	   1.0 - (float)mesi_cache_stat[i]->misses / (float)mesi_cache_stat[i]->accesses);
    printf("  replace:   %d\n", mesi_cache_stat[i]->replacements);
  }

  printf("\n");
  printf("  TRAFFIC\n");
  for (i = 0; i < num_core; i++) {
    demand_fetches += mesi_cache_stat[i]->demand_fetches;
    copies_back += mesi_cache_stat[i]->copies_back;
    broadcasts += mesi_cache_stat[i]->broadcasts;
  }
  printf("  demand fetch (words): %d\n", demand_fetches);
  /* number of broadcasts */
  printf("  broadcasts:           %d\n", broadcasts);
  printf("  copies back (words):  %d\n", copies_back);
}
/************************************************************/
