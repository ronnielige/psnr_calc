/**
 * ===========================================================================
 * threadpool.c
 * - thread pooling
 * ===========================================================================
 */
#include "threadpool.h"
#include "defines.h"
#include <malloc.h>
#include <string.h>
#ifdef linux
#include <unistd.h>
#endif

void push_job_into_list(JobList* jobLst, Job* njb);

void init_job_list(JobList* jobLst, int init_job_num, int max_job_num)
{
    pthread_mutex_init(&jobLst->jl_mutex, NULL);
    pthread_cond_init(&jobLst->jl_cond,   NULL);

    jobLst->i_maxjbs  = max_job_num;
    jobLst->i_jobnum  = 0;
    jobLst->jobHeader = NULL;

    for (int i = 0; i < init_job_num; i++)
    {
        Job* job = (Job *)malloc(sizeof(Job));
        memset(job, 0, sizeof(Job));
        push_job_into_list(jobLst, job);
    }
    jobLst->i_exit = 0;
}

void free_job_list(JobList* jobLst)
{
    pthread_mutex_lock(&jobLst->jl_mutex);
    jobLst->i_exit = 1;
    pthread_cond_broadcast(&jobLst->jl_cond);  // inform all remove_job_from_list & push_job_into_list to exit
    pthread_mutex_unlock(&jobLst->jl_mutex);

    pthread_mutex_lock(&jobLst->jl_mutex);
    Job* jb_iter = jobLst->jobHeader;
    Job*  lst_jb = jobLst->jobHeader;
    while (jb_iter)
    {
        lst_jb  = jb_iter;
        jb_iter = jb_iter->next;
        free(lst_jb);
    }
    jobLst->i_jobnum = 0;
    pthread_mutex_unlock(&jobLst->jl_mutex);
    //pthread_mutex_destroy(&jobLst->jl_mutex);
    //pthread_cond_destroy(&jobLst->jl_cond);
}

Job* remove_job_from_list(JobList* jobLst)
{
    Job* retJob;
    pthread_mutex_lock(&jobLst->jl_mutex);
    while (jobLst->i_jobnum == 0 && jobLst->i_exit == 0)
        pthread_cond_wait(&jobLst->jl_cond, &jobLst->jl_mutex);
    if (jobLst->i_exit == 1)
    {
        pthread_mutex_unlock(&jobLst->jl_mutex);
        return NULL;
    }
    retJob = jobLst->jobHeader;
    jobLst->jobHeader = jobLst->jobHeader->next;
    jobLst->i_jobnum--;
    myprintf("jobLst %p jobnum = %d, jobHeader = %p, retJob   = %p, in remove_job_from_list\n", jobLst, jobLst->i_jobnum, jobLst->jobHeader, retJob);
    pthread_cond_signal(&jobLst->jl_cond);
    pthread_mutex_unlock(&jobLst->jl_mutex);
    return retJob;
}

void push_job_into_list(JobList* jobLst, Job* njb)
{
    pthread_mutex_lock(&jobLst->jl_mutex);
    if (jobLst->jobHeader == NULL)
    {
        jobLst->jobHeader = njb;
        njb->next = NULL;
    }
    else
    {
        while (jobLst->i_jobnum >= jobLst->i_maxjbs && jobLst->i_exit == 0)
            pthread_cond_wait(&jobLst->jl_cond, &jobLst->jl_mutex);

        Job* jb_iter = jobLst->jobHeader;
        Job*  lst_jb = jobLst->jobHeader;
        if (jobLst->i_exit == 1)
        {
            pthread_mutex_unlock(&jobLst->jl_mutex);
            return;
        }

        while (jb_iter)
        {
            lst_jb  = jb_iter;
            jb_iter = jb_iter->next;
        }
        lst_jb->next = njb;
        njb->next = NULL;
    }
    jobLst->i_jobnum++;
    myprintf("jobLst %p jobnum = %d, jobHeader = %p, newerjob = %p, jobHeader->next = %p, in push_job_into_list\n", jobLst, jobLst->i_jobnum, jobLst->jobHeader, njb, jobLst->jobHeader->next);
    pthread_cond_signal(&jobLst->jl_cond);
    pthread_mutex_unlock(&jobLst->jl_mutex);
}

void* thread_run(void *arg)
{
    threadpool_t*    pool = (threadpool_t*)arg;
    JobList*   todojobLst = &pool->todo_job_list;
    JobList* unusedjobLst = &pool->unused_job_list;
    Job*        newJob;
    while (pool->i_exit == 0)
    {
        newJob = remove_job_from_list(todojobLst);
        newJob->func(newJob->arg);
        push_job_into_list(unusedjobLst, newJob);
    }
    return NULL;
}

int threadpool_init(threadpool_t **p_pool, int threads)
{
    int ret;
    *p_pool = (threadpool_t*)malloc(sizeof(threadpool_t));
    threadpool_t*   threadp = (*p_pool);
    JobList* unused_job_lst = &(threadp->unused_job_list);
    JobList*   todo_job_lst = &(threadp->todo_job_list);

    init_job_list(unused_job_lst, threads, threads);
    init_job_list(todo_job_lst, 0, threads);

    myprintf("unused_job_list %p, jobnum %d; todo_job_list %p, jobnum %d\n", unused_job_lst, unused_job_lst->i_jobnum, todo_job_lst, todo_job_lst->i_jobnum);

    // start threads
    threadp->i_threads = threads;
    threadp->i_exit = 0;
    for (int i = 0; i < threads; i++)
    {
        ret = pthread_create(&threadp->p_thread_handles[i], NULL, thread_run, (void*)threadp);
#ifndef linux
        Sleep(100);
#else
        usleep(1000);
#endif
    }

    return 1;
}

void threadpool_run(threadpool_t *pool, void *(*func)(void *), void *arg, int wait_sign)
{
    Job* newJob = remove_job_from_list(&pool->unused_job_list);
    if (newJob)
    {
        newJob->func = func;
        newJob->arg  = arg;
        push_job_into_list(&pool->todo_job_list, newJob);
    }
}

void *threadpool_wait(threadpool_t *pool, void *arg)
{
    for(int i = 0; i < pool->i_threads; i++)
        pthread_join(pool->p_thread_handles[i], NULL);
    return NULL;
}

void  threadpool_delete(threadpool_t *pool)
{
    JobList* unused_job_lst = &(pool->unused_job_list);
    JobList*   todo_job_lst = &(pool->todo_job_list);
    pool->i_exit = 1;
    threadpool_wait(pool, NULL);
    free_job_list(unused_job_lst);
    free_job_list(todo_job_lst);
}