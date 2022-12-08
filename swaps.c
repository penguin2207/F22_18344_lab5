#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "tm.h"

#define VARIANT_NOSYNC 0
#define VARIANT_MUT 1
#define VARIANT_TM 2

/*The parameters of the SWAPS kernel*/
size_t num_iters  = 1000000;
size_t num_thread = 8;
size_t kv_entries = 1000;
size_t variant = 1;
size_t num_swaps = 0;

size_t *aborts_tm;
size_t *commits_tm;
size_t *fallbacks_tm;


void *(*process)(void*) = NULL;

/*The key-value store that we run SWAPS on*/
unsigned long *kv;
pthread_spinlock_t *kv_mut;

pthread_barrier_t start_bar;

/*uniq_list fills in num conecutive elements of inds[]
  with indices in the range 0 - range.

  This function produces a unique list of num
  indices that you can use for SWAPS (i.e., generate
  your sub-space indices using uniq_list)
*/
void uniq_list(size_t range, size_t num, size_t *inds){

  for( int j = 0; j < num; j++ ){

    while(1){

      int dup = 0;

      inds[j] = rand() % range;

      for( int k = 0; k < num; k++ ){

        if(inds[j] == inds[k] && j != k){ dup = 1; }

      }
      if(!dup){ break; }

    }

  } 

}

void preprocess(){

  /*Allocate and zero the kv store that we're SWAPSing*/
  kv = calloc( kv_entries, sizeof(unsigned long) );

}

/*Processing the array without synchronization is broken!*/
void *process_nosync( void *varg ){

  /*Synchronize threads at the barrier to start work at the
    same time.*/
  pthread_barrier_wait(&start_bar);
  size_t *inds = calloc( num_swaps, sizeof(size_t) );
  for(int i = 0; i < num_iters; i++){

    uniq_list(kv_entries, num_swaps, inds);

    for(int j = 0; j < num_swaps; j++){

      int j2 = j == 0 ? (num_swaps-1) : j-1;
      unsigned long t = kv[ inds[j2] ];
      kv[ inds[j2] ] = kv[ inds[j] ];
      kv[ inds[j] ] = t+1;
    }

  }

}

// cite : https://www.tutorialspoint.com/c_standard_library/c_function_qsort.htm
int cmpfunc (const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}


void *process_mut( void *varg ){

  pthread_barrier_wait(&start_bar);
  size_t *inds = calloc( num_swaps, sizeof(size_t) );
  size_t *inds_sorted = calloc( num_swaps, sizeof(size_t) );
  
  for(int i = 0; i < num_iters; i++){

    uniq_list(kv_entries, num_swaps, inds);
    memcpy(inds_sorted, inds, num_swaps*sizeof(size_t));

    // Sort the locks 
    qsort(inds_sorted, num_swaps, sizeof(size_t), cmpfunc);
    // for (int i = 0; i < num_swaps; i++) {
    //   printf("%d",inds_sorted[i]);
    // }
    // printf("\n");
    // for (int i = 0; i < num_swaps; i++) {
    //   printf("%d",inds[i]);
    // }
    // printf("\n");

    // lock per each ind to be swapped
    for (int ind = 0; ind < num_swaps; ind++) {
      pthread_spin_lock(kv_mut + inds_sorted[ind]);
    }

    // Lock the indices that were to be swapped
    for(int j = 0; j < num_swaps; j++){
      int j2 = j == 0 ? (num_swaps-1) : j-1;
      
      unsigned long t = kv[ inds[j2] ];
      kv[ inds[j2] ] = kv[ inds[j] ];
      kv[ inds[j] ] = t+1;
    }
    // unlock
    for (int ind = 0; ind < num_swaps; ind++) {
      pthread_spin_unlock(kv_mut + inds_sorted[ind]);
    }
  }

}

    

void *process_tm( void *varg ){

  pthread_barrier_wait(&start_bar);
  int MAX_TRIES = 1;
  size_t *inds = calloc( num_swaps, sizeof(size_t) );
  size_t *inds_sorted = calloc( num_swaps, sizeof(size_t) );


  for(int k = 0; k < num_iters; k++){

    uniq_list(kv_entries, num_swaps, inds);  
    
    for(int i=0; i<MAX_TRIES; i++){
      if(_xbegin() == _XBEGIN_STARTED){
        for (int j = 0; j < num_swaps; j++) {
          if(kv_mut[ inds[j] ] != 1) {
            // __sync_fetch_and_add(aborts_tm, 1);
            _xabort(_XABORT_EXPLICIT);
          } 
        }
        for (int j = 0; j < num_swaps; j++) {
          // Do the swap
          int j2 = j == 0 ? (num_swaps-1) : j-1;
          unsigned long t = kv[ inds[j2] ];
          kv[ inds[j2] ] = kv[ inds[j] ];
          kv[ inds[j] ] = t+1; 
          // sync_fetch_and_add(commits_tm, 1);
        }
        _xend();
        goto end;
      }
    }

    /* Fall back case */
    memcpy(inds_sorted, inds, num_swaps*sizeof(size_t));
    qsort(inds_sorted, num_swaps, sizeof(size_t), cmpfunc);

    for (int ind = 0; ind < num_swaps; ind++) {
      pthread_spin_lock(kv_mut + inds_sorted[ind]);
    }
    for(int j = 0; j < num_swaps; j++){
      int j2 = j == 0 ? (num_swaps-1) : j-1;
      unsigned long t = kv[ inds[j2] ];
      kv[ inds[j2] ] = kv[ inds[j] ];
      kv[ inds[j] ] = t+1; 
    }
    for (int ind = 0; ind < num_swaps; ind++) {
      pthread_spin_unlock(kv_mut + inds_sorted[ind]);
    }

    end: ;

  }

}

unsigned long postprocess(){

  /*Sum up the results of the swappage to be sure that we have 
    the right number of elements updated in total.*/ 
  unsigned long sum = 0; 
  for(int i = 0; i < kv_entries; i++){
    sum = sum + kv[i];     
  } 
  if( sum != num_thread * num_iters * num_swaps){
    printf("Found %lu != %lu updates\n",sum, num_thread * num_iters * num_swaps);
    exit(-1);
  }else{
    printf("Found %lu updates\n",sum);
    exit(0);
  }

}

int main(int argc, char *argv[]){

  if(argc != 6){
    fprintf(stderr,"Usage: swaps <entries> <threads> <iterations> <variant 0=none,1=spin,2=tm> <swaps>\n");
    exit(-1);
  }

  process = process_nosync;

  /*swaps <entries> <threads> <iters> <variant 0=none,1=spin,2=tm> <swaps>*/
  if( argc >= 2 ){
    kv_entries = atoi( argv[1] );
  }
  
  if( argc >= 3 ){
    num_thread = atoi( argv[2] );
  }
  
  if( argc >= 4 ){
    num_iters = atoi( argv[3] );
  }

  if( argc >= 5 ){
    variant = atoi( argv[4] );
    switch(variant){

      case VARIANT_NOSYNC :
        process = &process_nosync;
        break;

      case VARIANT_MUT :
        process = &process_mut;
      
        /*Hint, Hint: You'll need some spin locks for the mutex version...*/ 
        kv_mut = calloc(kv_entries,sizeof(pthread_spinlock_t)); 
        for(int i = 0; i < kv_entries; i++){

          pthread_spin_init(kv_mut + i,PTHREAD_PROCESS_PRIVATE);

        }

        break;

      case VARIANT_TM :
        process = &process_tm;
        
        /*Hint, Hint: You'll need some spin locks for the TM version...*/ 
        kv_mut = calloc(kv_entries,sizeof(pthread_spinlock_t)); 
        for(int i = 0; i < kv_entries; i++){

          pthread_spin_init(kv_mut + i,PTHREAD_PROCESS_PRIVATE);

        }
        break;
        

      default: 
        process = &process_nosync;
    }
  }

  if( argc >= 6 ){
    num_swaps = atoi( argv[5] );
  }

  printf("%lu entries, %lu threads, %lu iters, variant %lu, %lu swaps\n",
          kv_entries, num_thread, num_iters, variant, num_swaps);
  
  pthread_t threads[num_thread];
  pthread_barrier_init(&start_bar,NULL,num_thread);

  preprocess();

  if( num_thread > 1 ){

    /*If multi-threaded, spawn threads here*/
    for(int i = 0; i < num_thread; i++){
      pthread_create(&(threads[i]),NULL,process,NULL);
    }

  }else{
    /*If single-threaded, call process*/
    process(NULL);
  }
  
  if( num_thread > 1 ){

    /*If parallel, join all the threads up here*/
    for(int i = 0; i < num_thread; i++){
      pthread_join(threads[i],NULL);
    }

  }


  /*Check results and print output*/
  //printf("Aborts ave: %d, Fallback ave: %d\n", *aborts_tm/num_thread, *fallbacks_tm/num_thread);
  postprocess();
  free(aborts_tm);
  free(fallbacks_tm);

}

