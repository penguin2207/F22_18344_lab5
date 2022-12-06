#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include "tm.h"

#define VARIANT_NOSYNC 0
#define VARIANT_MUT 1
#define VARIANT_FETCHANDADD 2
#define VARIANT_CMPEXCHG 3
#define VARIANT_TM 4


/*These are the main parameters of GUPS*/
size_t num_iters  = 1000000;
size_t num_thread = 8;
size_t kv_entries = 1000;


size_t *aborts_tm;
size_t *commits_tm;
size_t *fallbacks_tm;


void *(*process)(void*) = NULL;

/*kv is a pointer to the key value store that your
  code will be manipulating*/
unsigned long *kv;
pthread_spinlock_t *kv_mut;

pthread_barrier_t start_bar;

void preprocess(){

  /*Allocate and zero the kv store*/
  kv = calloc( kv_entries, sizeof(unsigned long) );

}

/*Processing with no synchronization is broken!*/
void *process_nosync( void *varg ){

  /*The barrier wait synchronizes the threads to 
    start working at the same time*/
  pthread_barrier_wait(&start_bar);
  for(int i = 0; i < num_iters; i++){
    size_t ind = rand() % kv_entries;
    kv[ind]++;
  }

}

void *process_mut( void *varg ){

  /*The barrier wait synchronizes the threads to 
    start working at the same time*/
  pthread_barrier_wait(&start_bar);

  /*TODO: Change this code to use spinlocks*/
  for(int i = 0; i < num_iters; i++){

    size_t ind = rand() % kv_entries;
    pthread_spin_lock(kv_mut + ind);
    kv[ind]++;
    pthread_spin_unlock(kv_mut + ind);
    

  }

}

void *process_fetchandadd( void *varg ){

  /*The barrier wait synchronizes the threads to 
    start working at the same time*/
  pthread_barrier_wait(&start_bar);

  /*TODO: Change this code to use fetch and add*/
  for(int i = 0; i < num_iters; i++){

    size_t ind = rand() % kv_entries;
    __sync_fetch_and_add(kv + ind, 1);

  }

}

void *process_cmpexchg( void *varg ){

  /*The barrier wait synchronizes the threads to 
    start working at the same time*/
  pthread_barrier_wait(&start_bar);
  
  /*TODO: Change this code to use cmpexchg*/
  for(int i = 0; i < num_iters; i++){

    size_t ind = rand() % kv_entries;
    while(!__sync_bool_compare_and_swap(kv + ind, kv[ind], kv[ind]+1));
  }

}

void *process_tm( void *varg ){

  /*The barrier wait synchronizes the threads to 
    start working at the same time*/
  pthread_barrier_wait(&start_bar);
  int MAX_TRIES = 200;

  /*TODO: Change this code to use TM*/
  for(int i = 0; i < num_iters; i++){

    size_t ind = rand() % kv_entries;

  for(int j=0; j<MAX_TRIES; j++){
      int status = _xbegin();
      if(status == _XBEGIN_STARTED){
        if(kv_mut[ind] != 1) {
          __sync_fetch_and_add(aborts_tm, 1);
          _xabort(_XABORT_EXPLICIT);
        }
        kv[ind]++;
        __sync_fetch_and_add(commits_tm, 1);
        _xend();
        goto end;
      }
    }
fallback: 
    __sync_fetch_and_add(fallbacks_tm, 1);
    pthread_spin_lock(kv_mut + ind);
    kv[ind]++;
    pthread_spin_unlock(kv_mut + ind);

end: continue;
  }

}

unsigned long postprocess(){
 
  unsigned long sum = 0; 

  /*Sum up the number of updates and print the result*/
  for(int i = 0; i < kv_entries; i++){
    sum = sum + kv[i];     
  } 
  printf("Found %lu updates\n",sum);

}

int main(int argc, char *argv[]){

  if(argc != 5){
    fprintf(stderr,"Usage: gups <entries> <threads> <iterations> <variant>\n");
    exit(-1);
  }

  process = process_nosync;

  /*ARGS: gups <entries> <threads> <iters> <variant>*/
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
    size_t variant = atoi( argv[4] );
    switch(variant){

      case VARIANT_NOSYNC :
        process = &process_nosync;
        break;

      case VARIANT_MUT :
        process = &process_mut;
  
        /*Hint, Hint: for the mut version, you'll need some 
          spinlocks*/     
        kv_mut = calloc(kv_entries,sizeof(pthread_spinlock_t)); 
        for(int i = 0; i < kv_entries; i++){
          pthread_spin_init(kv_mut + i,PTHREAD_PROCESS_PRIVATE);
        }

        break;

      case VARIANT_FETCHANDADD :
        process = &process_fetchandadd;
        break;

      case VARIANT_CMPEXCHG :
        process = &process_cmpexchg;
        break;

      case VARIANT_TM :
        process = &process_tm;
        aborts_tm = (size_t*)calloc(1, sizeof(size_t));
        fallbacks_tm = (size_t*)calloc(1, sizeof(size_t));
        /*Hint, Hint: for the TM version, you'll *also* need some 
          spinlocks*/     
        kv_mut = calloc(kv_entries,sizeof(pthread_spinlock_t)); 
        for(int i = 0; i < kv_entries; i++){
          pthread_spin_init(kv_mut + i,PTHREAD_PROCESS_PRIVATE);
        }
        break;

      default: 
        process = &process_nosync;
    }
    printf("%lu entries, %lu threads, %lu iters, variant %lu \n",
            kv_entries, num_thread, num_iters, variant);
  }

  /*Initialize the startup barrier*/ 
  pthread_t threads[num_thread];
  pthread_barrier_init(&start_bar,NULL,num_thread);

  /*Run initialization*/
  preprocess();

  if( num_thread > 1 ){

    /*If multi-threaded, create our worker threads*/
    for(int i = 0; i < num_thread; i++){
      pthread_create(&(threads[i]),NULL,process,NULL);
    }

  }else{
    /*If single-threaded, call the routine directly*/
    process(NULL);
  }
  
  if( num_thread > 1 ){

    /*If multi-threaded, join up the workers to finish*/
    for(int i = 0; i < num_thread; i++){
      pthread_join(threads[i],NULL);
    }

  }

  /*Run post-processing*/
  postprocess();

}


