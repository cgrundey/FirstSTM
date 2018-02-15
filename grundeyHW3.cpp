/**
* Software Transactional Memory
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

#include <errno.h>

#define CFENCE  __asm__ volatile ("":::"memory")
#define MFENCE  __asm__ volatile ("mfence":::"memory")

#define NUM_ACCTS    1000000
#define NUM_TRFR     10
#define TRFR_AMT     50
#define INIT_BALANCE 1000

using namespace std;

void tx_begin() {
  // Reset read_set & write_set
  for (int i = 0; i < 10; i++) {
    read_set[i] =
    write_set[i] =
  }
}
int tx_read(int addr) {
  /**
  * lock = read(lock[addr]);
  * ver = read(version[addr]);
  * if (!lock) {
  *   add(ver, read_set);
  *   return *addr;
  * }
  * else
  *   tx_abort();
  */
  int lock = myLocks[addr].lock();
  int ver = accts[addr].ver;
  if (!lock) {
    Acct temp = {addr, accts[addr].value, ver};
    read_set[rp] = temp;
    return accts[addr].value;
  }
  else
    tx_abort();
}

void tx_write(int addr, int val) {
  /**
  * lock = read(lock[addr]);
  * ver = read(version[addr]);
  * if (!lock)
  *   write_set.add(addr, value, ver);
  * else
  *   tx_abort();
  */
  int lock = myLocks[addr].lock();
  int ver = accts[addr].ver;
  if (!lock) {
    accts[addr] = val;
  }
  else
    tx_abort();
}

bool tx_abort() {
  return false;
}

void tx_commit() {
  // validate
  // acquire myLocks
  /*
  for each(entry in write_set) {
    lock(entry);
    if (entry.version != mem_ver) {
      unlock(all);
      tx_abort();
    }
  }
  for each(entry in read_set) {
    if (entry.version != mem_ver) {
      unlock(all);
      tx_abort();
    }
  }
  // Validation is a success
  for each(entry in write_set) {
    entry_add = entry.value;
    entry.ver += 1;
    entry.lock = 0;
  }
  */
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

typedef struct {
  int addr;
  int value;
  int ver;
} Acct;

vector<Acct> accts;
// unordered_map<pthread_mutex_t, int> myLocks;
vector<pthread_mutex_t> myLocks;

void* th_run(void * args)
{
  long id = (long)args;
  barrier(0);
  srand((unsigned)time(0));
  int workload = NUM_TRFR/numThreads;
  do { // ________________BEGIN_________________
    tx_begin();
    for (int i = 0; i < workload; i++) {
      int r1 = 0;
      int r2 = 0;
      for (int j = 0; j < 10; j++) {
        while (r1 == r2) {
          r1 = rand() % NUM_ACCTS;
          r2 = rand() % NUM_ACCTS;
        }
        // Perform the transfer
        int a1 = tx_read(r1);
        int a2 = tx_read(r2);
        tx_write(r1, a1 - TRFR_AMT);
        tx_write(r2, a2 + TRFR_AMT);
      }
    }
  } while (!tx_commit()); // ______________END__________________
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

  // initializing 1,000,000 accounts with $1000 each
  for (int i = 0; i < NUM_ACCTS; i++) {
    accts[i] = INIT_BALANCE;
  }

  // initialize mutex locks
  for (int i = 0; i < NUM_ACCTS; i++) {
    if (pthread_mutex_init(&mylocks[i], NULL) != 0) {
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
    totalMoneyBefore += accts[i].getBalance();
  }

  for (int i=0; i<ids-1; i++) {
    pthread_join(client_th[i], NULL);
  }
/* EXECUTION END */
  pthread_mutex_destroy(&mylock);

  printf("Total transfers: %d\n", actual_transfers);
  printf("Total time = %lld ns\n", get_real_time() - start);
  printf("Total Money Before: $%ld\n", totalMoneyBefore);
  printf("Total Money After: $%ld\n", totalMoneyAfter);

  return 0;
}
