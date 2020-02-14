#ifndef W32PTHREADS_H
#define W32PTHREADS_H

#include <windows.h>
#include <process.h>
#include <stdlib.h>

typedef struct pthread_t
{
    void* handle;
    void* (*func)(void* arg);
    void* arg;
    void* ret;
} pthread_t;

/* the conditional variable api for windows 6.0+ uses critical sections and
 * not mutexes */
typedef CRITICAL_SECTION pthread_mutex_t;

/* This is the CONDITIONAL_VARIABLE typedef for using Window's native
 * conditional variables on kernels 6.0+.
 * MinGW does not currently have this typedef. */
typedef struct pthread_cond_t
{
    void* ptr;
} pthread_cond_t;

/* function pointers to conditional variable API on windows 6.0+ kernels */
#if _WIN32_WINNT < 0x0600
static void (WINAPI* cond_broadcast)(pthread_cond_t* cond);
static void (WINAPI* cond_init)(pthread_cond_t* cond);
static void (WINAPI* cond_signal)(pthread_cond_t* cond);
static BOOL (WINAPI* cond_wait)(pthread_cond_t* cond, pthread_mutex_t* mutex,
                                DWORD milliseconds);
#else
#define cond_init      InitializeConditionVariable
#define cond_broadcast WakeAllConditionVariable
#define cond_signal    WakeConditionVariable
#define cond_wait      SleepConditionVariableCS
#endif

static inline int pthread_mutex_init(pthread_mutex_t* m, void* attr)
{
    InitializeCriticalSection(m);
    return 0;
}
static inline int pthread_mutex_destroy(pthread_mutex_t* m)
{
    DeleteCriticalSection(m);
    return 0;
}
static inline int pthread_mutex_lock(pthread_mutex_t* m)
{
    EnterCriticalSection(m);
    return 0;
}
static inline int pthread_mutex_unlock(pthread_mutex_t* m)
{
    LeaveCriticalSection(m);
    return 0;
}

/* for pre-Windows 6.0 platforms we need to define and use our own condition
 * variable and api */
typedef struct  win32_cond_t
{
    pthread_mutex_t mtx_broadcast;
    pthread_mutex_t mtx_waiter_count;
    volatile int waiter_count;
    HANDLE semaphore;
    HANDLE waiters_done;
    volatile int is_broadcast;
} win32_cond_t;

static void pthread_cond_init(pthread_cond_t* cond, const void* unused_attr)
{
    win32_cond_t* win32_cond = NULL;
    //if(cond_init)
    {
        cond_init((PCONDITION_VARIABLE)cond);
        return;
    }

    /* non native condition variables */
    win32_cond = (win32_cond_t *)malloc(sizeof(win32_cond_t));
    if(win32_cond)
        memset(win32_cond, 0, sizeof(win32_cond_t));
    if(!win32_cond)
        return;
    cond->ptr = win32_cond;
    win32_cond->semaphore = CreateSemaphore(NULL, 0, 0x7fffffff, NULL);
    if(!win32_cond->semaphore)
        return;
    win32_cond->waiters_done = CreateEvent(NULL, TRUE, FALSE, NULL);
    if(!win32_cond->waiters_done)
        return;

    pthread_mutex_init(&win32_cond->mtx_waiter_count, NULL);
    pthread_mutex_init(&win32_cond->mtx_broadcast, NULL);
}

static void pthread_cond_destroy(pthread_cond_t* cond)
{
    win32_cond_t* win32_cond = (win32_cond_t *)cond->ptr;
    /* native condition variables do not destroy */
    //if(cond_init)
        return;

    /* non native condition variables */
    CloseHandle(win32_cond->semaphore);
    CloseHandle(win32_cond->waiters_done);
    pthread_mutex_destroy(&win32_cond->mtx_waiter_count);
    pthread_mutex_destroy(&win32_cond->mtx_broadcast);
    free(&win32_cond);
    cond->ptr = NULL;
}

static void pthread_cond_broadcast(pthread_cond_t* cond)
{
    win32_cond_t* win32_cond = (win32_cond_t *)cond->ptr;
    int have_waiter;

    //if(cond_broadcast)
    {
        cond_broadcast((PCONDITION_VARIABLE)cond);
        return;
    }

    /* non native condition variables */
    pthread_mutex_lock(&win32_cond->mtx_broadcast);
    pthread_mutex_lock(&win32_cond->mtx_waiter_count);
    have_waiter = 0;

    if(win32_cond->waiter_count)
    {
        win32_cond->is_broadcast = 1;
        have_waiter = 1;
    }

    if(have_waiter)
    {
        ReleaseSemaphore(win32_cond->semaphore, win32_cond->waiter_count, NULL);
        pthread_mutex_unlock(&win32_cond->mtx_waiter_count);
        WaitForSingleObject(win32_cond->waiters_done, INFINITE);
        ResetEvent(win32_cond->waiters_done);
        win32_cond->is_broadcast = 0;
    }
    else
        pthread_mutex_unlock(&win32_cond->mtx_waiter_count);
    pthread_mutex_unlock(&win32_cond->mtx_broadcast);
}

static int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex)
{
    win32_cond_t* win32_cond = (win32_cond_t *)cond->ptr;
    int last_waiter;
    //if(cond_wait)
    {
        cond_wait((PCONDITION_VARIABLE)cond, mutex, INFINITE);
        return 0;
    }

    /* non native condition variables */
    pthread_mutex_lock(&win32_cond->mtx_broadcast);
    pthread_mutex_lock(&win32_cond->mtx_waiter_count);
    win32_cond->waiter_count++;
    pthread_mutex_unlock(&win32_cond->mtx_waiter_count);
    pthread_mutex_unlock(&win32_cond->mtx_broadcast);

    // unlock the external mutex
    pthread_mutex_unlock(mutex);
    WaitForSingleObject(win32_cond->semaphore, INFINITE);

    pthread_mutex_lock(&win32_cond->mtx_waiter_count);
    win32_cond->waiter_count--;
    last_waiter = !win32_cond->waiter_count || !win32_cond->is_broadcast;
    pthread_mutex_unlock(&win32_cond->mtx_waiter_count);

    if(last_waiter)
        SetEvent(win32_cond->waiters_done);

    // lock the external mutex
    return pthread_mutex_lock(mutex);
}

static void pthread_cond_signal(pthread_cond_t* cond)
{
    win32_cond_t* win32_cond = (win32_cond_t *)cond->ptr;
    int have_waiter;
    //if(cond_signal)
    {
        cond_signal((PCONDITION_VARIABLE)cond);
        return;
    }

    pthread_mutex_lock(&win32_cond->mtx_broadcast);

    /* non-native condition variables */
    pthread_mutex_lock(&win32_cond->mtx_waiter_count);
    have_waiter = win32_cond->waiter_count;
    pthread_mutex_unlock(&win32_cond->mtx_waiter_count);

    if(have_waiter)
    {
        ReleaseSemaphore(win32_cond->semaphore, 1, NULL);
        WaitForSingleObject(win32_cond->waiters_done, INFINITE);
        ResetEvent(win32_cond->waiters_done);
    }

    pthread_mutex_unlock(&win32_cond->mtx_broadcast);
}

static void w32thread_init(void)
{
#if _WIN32_WINNT < 0x0600
    HANDLE kernel_dll = GetModuleHandle(TEXT("kernel32.dll"));
    /* if one is available, then they should all be available */
    cond_init      =
        (void*)GetProcAddress(kernel_dll, "InitializeConditionVariable");
    cond_broadcast =
        (void*)GetProcAddress(kernel_dll, "WakeAllConditionVariable");
    cond_signal    =
        (void*)GetProcAddress(kernel_dll, "WakeConditionVariable");
    cond_wait      =
        (void*)GetProcAddress(kernel_dll, "SleepConditionVariableCS");
#endif
}

static unsigned __stdcall win32thread_worker(void* arg)
{
	pthread_t* h = (pthread_t *)arg;
	h->ret = h->func(h->arg);
	return 0;
}

static int pthread_create(pthread_t* thread, const void* unused_attr, void * (*start_routine)(void*), void* arg)
{
	thread->func   = start_routine;
	thread->arg    = arg;
	thread->handle = (void*)_beginthreadex(NULL, 0, win32thread_worker, thread, 0, NULL);
	return !thread->handle;
}

static void pthread_join(pthread_t thread, void** value_ptr)
{
	DWORD ret = WaitForSingleObject(thread.handle, INFINITE);
	if(ret != WAIT_OBJECT_0)
		return;
	if(value_ptr)
		*value_ptr = thread.ret;
	CloseHandle(thread.handle);
}

#endif