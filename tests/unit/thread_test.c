#include "cf3.defs.h"

#include <setjmp.h>
#include <cmockery.h>

#define NUM_THREADS 100

static void create_children(pthread_t tids[NUM_THREADS]);
static void join_children(pthread_t tids[NUM_THREADS]);
static void increment_shared_var(void);

int SHARED_VAR;
pthread_mutex_t shared_var_mutex = PTHREAD_MUTEX_INITIALIZER;

void test_init_destroy(void **p)
{
    pthread_mutex_t mutex_dynamic;
 
    int res_init = pthread_mutex_init(&mutex_dynamic, NULL);
    assert_int_equal(res_init, 0);

    int res_destroy = pthread_mutex_destroy(&mutex_dynamic);
    assert_int_equal(res_destroy, 0);
}


void test_trylock_impl(pthread_mutex_t *mutex)
{
    int res_trylock_unlocked = pthread_mutex_trylock(mutex);
    assert_int_equal(res_trylock_unlocked, 0);
    
    int res_trylock_locked = pthread_mutex_trylock(mutex);

    if (res_trylock_locked != EBUSY && res_trylock_locked != EDEADLK)
    {
    /* Some pthread implementations return EDEADLK despite SUS saying otherwise */
        fail();
    }

    int res_unlock = pthread_mutex_unlock(mutex);
    assert_int_equal(res_unlock, 0);
}


void test_trylock_dynamic(void **p)
{
    pthread_mutex_t mutex_dynamic;

    int res_init = pthread_mutex_init(&mutex_dynamic, NULL);
    assert_int_equal(res_init, 0);
    
    test_trylock_impl(&mutex_dynamic);

    int res_destroy = pthread_mutex_destroy(&mutex_dynamic);
    assert_int_equal(res_destroy, 0);
}


void test_trylock_static(void **p)
{
    pthread_mutex_t mutex_static = PTHREAD_MUTEX_INITIALIZER;
    
    test_trylock_impl(&mutex_static);
}


void test_trylock_static_errorcheck(void **p)
{
    pthread_mutex_t mutex_static_errorcheck = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

    test_trylock_impl(&mutex_static_errorcheck);
}


void test_create(void **p)
{
    SHARED_VAR = 0;

    pthread_t tid;

    int res_create = pthread_create(&tid, NULL, (void *) increment_shared_var, NULL);
    assert_int_equal(res_create, 0);

    int res_join = pthread_join(tid, NULL);
    assert_int_equal(res_join, 0);
    assert_int_equal(SHARED_VAR, 1);
}




static void increment_shared_var(void)
{
#define THREAD_ITERATIONS 1000
    
    int res_lock = pthread_mutex_lock(&shared_var_mutex);
    assert_int_equal(res_lock, 0);

    for(int i = 0; i < THREAD_ITERATIONS; i++)
    {
        SHARED_VAR++;
        SHARED_VAR--;
    }
    
    SHARED_VAR++;

    int res_unlock = pthread_mutex_unlock(&shared_var_mutex);
    assert_int_equal(res_unlock, 0);
}


void test_lock(void **p)
{
    SHARED_VAR = 0;
    
    pthread_t tids[NUM_THREADS];
    
    create_children(tids);
    join_children(tids);
    
    assert_int_equal(SHARED_VAR, NUM_THREADS);
}


static void create_children(pthread_t tids[NUM_THREADS])
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 65536);

    for(int i = 0; i < NUM_THREADS; i++)
    {
        int res_create = pthread_create(&(tids[i]), &attr, (void *) increment_shared_var, NULL);
        assert_int_equal(res_create, 0);
    }
}


static void join_children(pthread_t tids[NUM_THREADS])
{
    for(int i = 0; i < NUM_THREADS; i++)
    {
        int res_join = pthread_join(tids[i], NULL);
        assert_int_equal(res_join, 0);
    }
}


int main()
{
    const UnitTest tests[] =
        {
            unit_test(test_init_destroy),
            unit_test(test_trylock_dynamic),
            unit_test(test_trylock_static),
            unit_test(test_trylock_static_errorcheck),
            unit_test(test_create),
            unit_test(test_lock),
        };

    return run_tests(tests);
}
