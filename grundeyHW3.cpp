/**
* Software Transactional Memory
*   The five functions of software transactional memory are implemented
*   and used in a Bank Account example. Lazy implementation is used so
*   write-back occurs in tx_commit() and a redo-log is kept of read and
*   write values to try again. tx_abort() is not explicitly implemented,
*   rather it is the return value of tx_commit() to show that it was aborted.
*
* Author: Colin Grundey
* Date: February 19, 2018
*
* Compile:
*   g++ grundeyHW3.cpp -o HW3 -lpthread
*/

#include <pthread.h>
#include <list>
#include <cstdlib>
#include <vector>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <iostream>
#include <time.h>
#include <unordered_map>
#include <list> // Linked list for read/write sets

#include <errno.h>

#define CFENCE  __asm__ volatile ("":::"memory")
#define MFENCE  __asm__ volatile ("mfence":::"memory")

#define NUM_ACCTS    1000000
#define NUM_TRFR     10
#define TRFR_AMT     50
#define INIT_BALANCE 1000

using namespace std;

typedef struct {
  int addr;
  int value;
  int ver;
} Acct;

vector<Acct> accts;
// unordered_map<pthread_mutex_t, int> myLocks;
pthread_mutex_t myLocks[NUM_ACCTS];
int numThreads;

/* Where a transaction begins. Read and write sets are intialized and cleared. */
void tx_begin(list<Acct>& read_set, list<Acct>& write_set) {
  read_set.clear();
  write_set.clear();
}
/* Adds account with version to read_set and returns the value for later use. */
int tx_read(int addr, list<Acct>& read_set) {
  int lck = pthread_mutex_lock(&(myLocks[addr]));
  if (!lck) {
    read_set.push_back(accts[addr]);
    return accts[addr].value;
  }
  else
    return -1; // ABORT - TODO: maybe consider retrying lock
}
/* Adds a version of the account at addr to write_set with the given value. */
bool tx_write(int addr, int val, list<Acct>& write_set) {
  int lck = pthread_mutex_lock(&(myLocks[addr]));
  if (!lck) {
    Acct temp; temp.addr = addr;
    temp.value = val;
    temp.ver = accts[addr].ver;
    write_set.push_back(temp);
    return true;
  }
  return false; // ABORT - TODO: maybe consider retrying lock
}
/* Attempts to commit the transaction. Checks read and write set versions
 * and compares it to memory versions (i.e. accts[].ver). If valid, all
 * accounts in read and write sets are written back to memory. */
bool tx_commit(list<Acct>& read_set, list<Acct>& write_set) {
  list<Acct>::iterator iterator;
  bool aborting = false;
  /* Validate write_set */
  for (iterator = write_set.begin(); iterator != write_set.end(); ++iterator) {
    if (aborting) { // Abort_2
      pthread_mutex_unlock(&(myLocks[iterator->addr]));
      continue;
    }
    pthread_mutex_lock(&(myLocks[iterator->addr]));
    if (iterator->ver != accts[iterator->addr].ver) {
      aborting = true; // Abort_1
      iterator = write_set.begin(); // Begin parse again to unlock all
    }
  }
  if (aborting) // Abort_3
    return false;
  aborting = false;
  /* Validate read_set */
  for (iterator = read_set.begin(); iterator != read_set.end(); ++iterator) {
    if (aborting) { // Abort_2
      pthread_mutex_unlock(&(myLocks[iterator->addr]));
      continue;
    }
    pthread_mutex_lock(&(myLocks[iterator->addr]));
    if (iterator->ver != accts[iterator->addr].ver) {
      aborting = true; // Abort_1
      iterator = read_set.begin(); // Begin parse again to unlock all
    }
  }
  if (aborting) // Abort_3
    return false;
  /* Validation is a success */
  for (iterator = write_set.begin(); iterator != write_set.end(); ++iterator) {
    accts[iterator->addr].value = iterator->value;
    accts[iterator->addr].ver = iterator->ver;
    pthread_mutex_unlock(&(myLocks[iterator->addr]));
  }
  return true;
}

inline unsigned long long get_real_time() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC_RAW, &time);
    return time.tv_sec * 1000000000L + time.tv_nsec;
}

/* Support a few lightweight barriers */
void barrier(int which) {
    static volatile int barriers[16] = {0};
    CFENCE;
    __sync_fetch_and_add(&barriers[which], 1);
    while (barriers[which] != numThreads) { }
    CFENCE;
}

// Thread function
void* th_run(void * args)
{
  long id = (long)args;
  barrier(0);
  srand((unsigned)time(0));

  list<Acct> read_set;
  list<Acct> write_set;

  int workload = NUM_TRFR/numThreads;
  for (int i = 0; i < workload; i++) {
// ________________BEGIN_________________
    do {
      tx_begin(read_set, write_set);
      int r1 = 0;
      int r2 = 0;
      for (int j = 0; j < 10; j++) {
        while (r1 == r2) {
          r1 = rand() % NUM_ACCTS;
          r2 = rand() % NUM_ACCTS;
        }
        // Perform the transfer
        int a1 = tx_read(r1, read_set);
        if (a1 < 0) {continue;}
        int a2 = tx_read(r2, read_set);
        tx_write(r1, a1 - TRFR_AMT, write_set);
        tx_write(r2, a2 + TRFR_AMT, write_set);
      }
    } while (!tx_commit(read_set, write_set));
// _________________END__________________
  }
  return 0;
}

int main(int argc, char* argv[]) {
  // Input arguments error checking
  if (argc != 2) {
    printf("Usage: <# of threads -> 1, 2, or 4\n");
    exit(0);
  } else {
    numThreads = atoi(argv[1]);
    if (numThreads != 1 && numThreads != 2 && numThreads != 4) {
      printf("Usage: <# of threads -> 1, 2, or 4\n");
      exit(0);
    }
  }

  // Initializing 1,000,000 accounts with $1000 each
  for (int i = 0; i < NUM_ACCTS; i++) {
    accts[i].value = INIT_BALANCE;
  }

  // Initialize mutex locks
  for (int i = 0; i < NUM_ACCTS; i++) {
    if (pthread_mutex_init(&myLocks[i], NULL) != 0) {
      printf("\n mutex %d init failed\n", i);
      return 1;
    }
  }
  // Thread initializations
  pthread_attr_t thread_attr;
  pthread_attr_init(&thread_attr);

  pthread_t client_th[300];
  long ids = 1;
  for (int i = 1; i < numThreads; i++) {
    pthread_create(&client_th[ids-1], &thread_attr, th_run, (void*)ids);
    ids++;
  }

/* EXECUTION BEGIN */
  unsigned long long start = get_real_time();
  th_run(0);

  long totalMoneyBefore = 0;
  for (int i = 0; i < NUM_ACCTS; i++) {
    totalMoneyBefore += accts[i].value;
  }

  for (int i=0; i<ids-1; i++) {
    pthread_join(client_th[i], NULL);
  }
/* EXECUTION END */
  for (int i = 0; i < NUM_ACCTS; i++) {
    pthread_mutex_destroy(&myLocks[i]);
  }

  //printf("Total transfers: %d\n", actual_transfers);
  printf("Total time = %lld ns\n", get_real_time() - start);
  printf("Total Money Before: $%ld\n", totalMoneyBefore);
  // printf("Total Money After: $%ld\n", totalMoneyAfter);

  return 0;
}
