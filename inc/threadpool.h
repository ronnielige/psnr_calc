/**
 * ===========================================================================
 * threadpool.h
 * - thread pooling
 * ---------------------------------------------------------------------------
 * ===========================================================================
 */

#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#ifdef _MSC_VER
#include "w32thread.h"
#else
#include <pthread.h>
#endif

#define MAX_THREADS 256

typedef struct _job_desc_
{
    void*  (*func)(void *);  // job function
    void*  arg;
    struct _job_desc_* next;
}Job;

typedef struct _job_list_
{
    Job* jobHeader;
    int  i_maxjbs;  // maximum job numbers
    int  i_jobnum;  // current job numbers
    pthread_mutex_t jl_mutex;
    pthread_cond_t  jl_cond;
    volatile int    i_exit;
}JobList;

typedef struct _threadpool
{
    pthread_t        p_thread_handles[MAX_THREADS];
    JobList          todo_job_list;
    JobList          unused_job_list;
    pthread_mutex_t  tp_mutex;
    pthread_cond_t   tp_cond;
    volatile int     i_exit;
    int              i_threads;
}threadpool_t;

int   threadpool_init(threadpool_t **p_pool, int threads);
void  threadpool_run(threadpool_t *pool, void *(*func)(void *), void *arg, int wait_sign);
void *threadpool_wait(threadpool_t *pool, void *arg);
void  threadpool_delete(threadpool_t *pool);


#endif  // _THREADPOOL_H
