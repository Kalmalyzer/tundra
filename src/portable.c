#include "portable.h"

#if defined(__APPLE__) || defined(linux)
#include <sys/stat.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
#include <errno.h>
#include <process.h>
#include <stdio.h>
#include <assert.h>

int pthread_mutex_init(pthread_mutex_t *mutex, void *args)
{
	assert(args == NULL);
	mutex->handle = (PCRITICAL_SECTION)malloc(sizeof(CRITICAL_SECTION));
	if (!mutex->handle)
		return ENOMEM;
	InitializeCriticalSection((PCRITICAL_SECTION)mutex->handle);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	DeleteCriticalSection((PCRITICAL_SECTION)mutex->handle);
	free(mutex->handle);
	mutex->handle = 0;
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *lock)
{
	EnterCriticalSection((PCRITICAL_SECTION)lock->handle);
	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *lock)
{
	LeaveCriticalSection((PCRITICAL_SECTION)lock->handle);
	return 0;
}

int pthread_cond_init(pthread_cond_t *cond, void *args)
{
	assert(args == NULL);

	if (NULL == (cond->handle = malloc(sizeof(CONDITION_VARIABLE))))
		return ENOMEM;

	InitializeConditionVariable((PCONDITION_VARIABLE) cond->handle);
	return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	free (cond->handle);
	return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t* lock)
{
	BOOL result = SleepConditionVariableCS((PCONDITION_VARIABLE) cond->handle, (PCRITICAL_SECTION) lock->handle, INFINITE);
	return !result;
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
	WakeAllConditionVariable((PCONDITION_VARIABLE) cond->handle);
	return 0;
}

int pthread_cond_signal(pthread_cond_t *cond)
{
	WakeConditionVariable((PCONDITION_VARIABLE) cond->handle);
	return 0;
}

typedef struct
{
	uintptr_t thread;
	pthread_thread_routine func;
	void *input_arg;
	void *result;
	volatile LONG taken;
} thread_data;

enum { W32_MAX_THREADS = 32 };
static thread_data setup[W32_MAX_THREADS];

static thread_data* alloc_thread(void)
{
	int i;
	for (i = 0; i < W32_MAX_THREADS; ++i)
	{
		thread_data* p = &setup[i];
		if (0 == InterlockedExchange(&p->taken, 1))
			return p;
	}

	fprintf(stderr, "fatal: too many threads allocated (limit: %d)\n", W32_MAX_THREADS);
	exit(1);
}

static unsigned thread_start(void *args_)
{
	thread_data* args = (thread_data*) args_;
	args->result = (*args->func)(args->input_arg);
	return 0;
}

int pthread_create(pthread_t *result, void* options, pthread_thread_routine start, void *routine_arg)
{
	thread_data* data = alloc_thread();
	data->func = start;
	data->input_arg = routine_arg;
	data->thread = _beginthreadex(NULL, 0, thread_start, data, 0, NULL);
	result->index = (int) (data - &setup[0]);
	return 0;
}

int pthread_join(pthread_t thread, void **result_out)
{
	thread_data *data = &setup[thread.index];
	assert(thread.index >= 0 && thread.index < W32_MAX_THREADS);
	assert(data->taken);

	if (WAIT_OBJECT_0 == WaitForSingleObject((HANDLE) data->thread, INFINITE))
	{
		CloseHandle((HANDLE) data->thread);
		*result_out = data->result;
		memset(data, 0, sizeof(thread_data));
		return 0;
	}
	else
	{
		return EINVAL;
	}
}

#endif

int
td_stat_file(const char *filename, td_stat *stat_out)
{
#if defined(__APPLE__) || defined(linux)
	int err;
	struct stat s;
	if (0 != (err = stat(filename, &s)))
		return err;

	stat_out->is_dir = 0 != (s.st_mode & S_IFDIR);
	stat_out->size = s.st_size;
	stat_out->timestamp = s.st_mtime;

#elif defined(_WIN32)
#error meh
#else
#error meh
#endif

	return 0;
}
