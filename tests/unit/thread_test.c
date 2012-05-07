#include "cf3.defs.h"
#include "cf3.extern.h"

#include <setjmp.h>
#include <cmockery.h>


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
    assert_int_equal(res_trylock_locked, EBUSY);

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


int main()
{
    const UnitTest tests[] =
        {
            unit_test(test_init_destroy),
            unit_test(test_trylock_dynamic),
            unit_test(test_trylock_static),
            unit_test(test_trylock_static_errorcheck),
        };

    return run_tests(tests);
}
