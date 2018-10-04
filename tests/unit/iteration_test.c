#include <test.h>


#include <iteration.c>



static void test_FindDollarParen(void)
{
    /* not found */
    assert_int_equal(FindDollarParen("", 1), 0);
    assert_int_equal(FindDollarParen(" ", 2), 1);
    assert_int_equal(FindDollarParen("$", 2), 1);
    assert_int_equal(FindDollarParen("(", 2), 1);
    assert_int_equal(FindDollarParen("{", 2), 1);
    assert_int_equal(FindDollarParen("$ ", 3), 2);
    assert_int_equal(FindDollarParen("$$", 3), 2);
    assert_int_equal(FindDollarParen("$[", 3), 2);
    assert_int_equal(FindDollarParen("($", 3), 2);
    assert_int_equal(FindDollarParen(" $", 3), 2);
    assert_int_equal(FindDollarParen(" $[", 4), 3);
    assert_int_equal(FindDollarParen("$ (", 4), 3);
    assert_int_equal(FindDollarParen("$ {", 4), 3);

    /* found */
    assert_int_equal(FindDollarParen("${", 3), 0);
    assert_int_equal(FindDollarParen("$(", 3), 0);
    assert_int_equal(FindDollarParen(" $(", 4), 1);
    assert_int_equal(FindDollarParen(" ${", 4), 1);
    assert_int_equal(FindDollarParen("$$(", 4), 1);
    assert_int_equal(FindDollarParen("$${", 4), 1);

    // Detect out of bounds read:
    // If max is 0, it shouldn't try to deref these invalid pointers:
    char a = 'a';
    assert_int_equal(FindDollarParen(0x1, 0), 0);
    assert_int_equal(FindDollarParen((&a) + 1, 0), 0);

    // Should not read past max bytes:
    char b[1] = {'b'};
    assert_int_equal(FindDollarParen(b, 1), 1);
    char c[8] = {'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c'};
    assert_int_equal(FindDollarParen(c, 8), 8);

    // We had some problems with FindDollarParen reading outside a buffer
    // so I added these cryptic but useful tests. Some of them will only
    // fail if you run tests with ASAN, valgrind or similar.
}


static void test_IsMangled(void)
{
    /* Simply true. */
    assert_false(IsMangled(""));
    assert_false(IsMangled("blah"));
    assert_false(IsMangled("namespace:blah"));
    assert_false(IsMangled("scope.blah"));
    assert_false(IsMangled("namespace:scope.blah"));

    /* Simply false. */
    assert_true(IsMangled("scope#blah"));
    assert_true(IsMangled("namespace*blah"));
    assert_true(IsMangled("namespace*scope.blah"));
    assert_true(IsMangled("namespace:scope#blah"));

    /* Complicated: nested expansions shouldn't affect result */
    assert_false(IsMangled("$("));
    assert_false(IsMangled("${"));
    assert_false(IsMangled("blah$(blue)"));
    assert_false(IsMangled("blah$(scope.blue)"));
    assert_false(IsMangled("blah$(scope#blue)"));
    assert_false(IsMangled("blah$(namespace:blue)"));
    assert_false(IsMangled("blah$(namespace*blue)"));
    assert_false(IsMangled("blah$(namespace:scope.blue)"));
    assert_false(IsMangled("blah$(namespace:scope#blue)"));
    assert_false(IsMangled("blah$(namespace*scope.blue)"));
    assert_false(IsMangled("blah$(namespace*scope#blue)"));

    assert_false(IsMangled("scope.blah$(blue)"));
    assert_false(IsMangled("scope.blah$(scope.blue)"));
    assert_false(IsMangled("scope.blah$(scope#blue)"));
    assert_false(IsMangled("scope.blah$(namespace:blue)"));
    assert_false(IsMangled("scope.blah$(namespace*blue)"));
    assert_false(IsMangled("scope.blah$(namespace:scope.blue)"));
    assert_false(IsMangled("scope.blah$(namespace:scope#blue)"));
    assert_false(IsMangled("scope.blah$(namespace*scope.blue)"));
    assert_false(IsMangled("scope.blah$(namespace*scope#blue)"));

    assert_true(IsMangled("scope#blah$(blue)"));
    assert_true(IsMangled("scope#blah$(scope.blue)"));
    assert_true(IsMangled("scope#blah$(scope#blue)"));
    assert_true(IsMangled("scope#blah$(namespace:blue)"));
    assert_true(IsMangled("scope#blah$(namespace*blue)"));
    assert_true(IsMangled("scope#blah$(namespace:scope.blue)"));
    assert_true(IsMangled("scope#blah$(namespace:scope#blue)"));
    assert_true(IsMangled("scope#blah$(namespace*scope.blue)"));
    assert_true(IsMangled("scope#blah$(namespace*scope#blue)"));

    assert_true(IsMangled("namespace*blah$(blue)"));
    assert_true(IsMangled("namespace*blah$(scope.blue)"));
    assert_true(IsMangled("namespace*blah$(scope#blue)"));
    assert_true(IsMangled("namespace*blah$(namespace:blue)"));
    assert_true(IsMangled("namespace*blah$(namespace*blue)"));
    assert_true(IsMangled("namespace*blah$(namespace:scope.blue)"));
    assert_true(IsMangled("namespace*blah$(namespace:scope#blue)"));
    assert_true(IsMangled("namespace*blah$(namespace*scope.blue)"));
    assert_true(IsMangled("namespace*blah$(namespace*scope#blue)"));

    assert_false(IsMangled("$(scope#blah)"));
    assert_false(IsMangled("$(namespace*blah)"));
    assert_false(IsMangled("$(namespace*scope#blah)"));

    /* Multiple nested expansions, again, none should matter. */
    assert_false(IsMangled("blah$(blue$(bleh))"));
    assert_false(IsMangled("blah$(scope.blue$(scope#bleh))"));

    /* Array indexes shouldn't affect the result either. */
    assert_false(IsMangled("["));
    assert_false(IsMangled("blah[$(blue)]"));
    assert_false(IsMangled("blah$(blue[bleh])"));
    assert_false(IsMangled("blah[S#i]"));
    assert_false(IsMangled("blah[S#i][N*i]"));
    assert_false(IsMangled("blah[S#i][N*i]"));

    assert_true(IsMangled("S#blah[S.blue]"));
    assert_true(IsMangled("S#blah[N:blue]"));
    assert_true(IsMangled("S#blah[S#blue]"));
    assert_true(IsMangled("N*blah[S.blue]"));
    assert_true(IsMangled("N*blah[N:blue]"));
    assert_true(IsMangled("N*S.blah[N:blue]"));
    assert_true(IsMangled("N*S.blah[S.blue]"));

    assert_false(IsMangled("S.blah[S#i][N*i]"));
    assert_true (IsMangled("S#blah[S#i][N*i]"));

    assert_false(IsMangled("[scope#blah]"));
    assert_false(IsMangled("[namespace*blah]"));
    assert_false(IsMangled("[namespace*scope#blah]"));

    /* Complicated: combine nested variables with array expansions. */
    assert_false(IsMangled("[$("));
    assert_false(IsMangled("S.blah[$("));
    assert_false(IsMangled("S.blah[i]$(blah)"));
    assert_false(IsMangled("S.blah[$(i)]"));
    assert_false(IsMangled("S.v[$(i)]"));
    assert_true (IsMangled("S#v[$(i)]"));
    assert_true (IsMangled("N*v[$(i)]"));
    assert_false(IsMangled("N:v[$(i)]"));
    assert_false(IsMangled("N:v[$(N*i)]"));
    assert_false(IsMangled("N:v[$(S#i)]"));
    assert_true (IsMangled("N*v[$(S#i)]"));
    assert_true (IsMangled("S#v[$(S#i)]"));
    assert_false(IsMangled("v[$(N*S#i)]"));
    assert_false(IsMangled("v[$(N:S#i)]"));
    assert_false(IsMangled("v[$(N*S.i)]"));
    assert_false(IsMangled("S.v[$(N*S#i)]"));
    assert_true (IsMangled("S#v[$(N*S#i)]"));
    assert_false(IsMangled("N:v[$(N*S#i)]"));
    assert_true (IsMangled("N*v[$(N*S#i)]"));
}


static void Mangle_TestHelper(const char *orig, const char *expected)
{
    printf("Testing MangleVarRefString: %s\n", orig);

    char *s = xstrdup(orig);
    MangleVarRefString(s, strlen(s));
    assert_string_equal(s, expected);
    free(s);
}
static void Mangle_TestHelperWithLength1(const char *orig, size_t orig_len,
                                         const char *expected)
{
    printf("Testing MangleVarRefString: %.*s\n", (int) orig_len, orig);

    char *s = xstrdup(orig);
    MangleVarRefString(s, orig_len);
    assert_string_equal(s, expected);
    free(s);
}
/* NOTE: this one changes "orig" in-place. */
static void Mangle_TestHelperWithLength2(char *orig,           size_t orig_len,
                                         const char *expected, size_t expected_len)
{
    printf("Testing MangleVarRefString: %.*s\n", (int) orig_len, orig);

    MangleVarRefString(orig, orig_len);
    assert_memory_equal(orig, expected, expected_len);
}
static void test_MangleVarRefString(void)
{
    Mangle_TestHelper            ("", "");
    Mangle_TestHelperWithLength1 ("a", 0, "a");

    Mangle_TestHelper            ("a.b", "a#b");
    /* Force length to 1, no change should occur. */
    Mangle_TestHelperWithLength1 ("a.b", 1, "a.b");

    Mangle_TestHelper            ("a:b",    "a*b");
    /* Force length to 1, no change should occur. */
    Mangle_TestHelperWithLength1 ("a:b", 1, "a:b");

    Mangle_TestHelper            ("a:b.c",  "a*b#c");
    /* Force length to 1, no change should occur. */
    Mangle_TestHelperWithLength1 ("a:b.c", 1, "a:b.c");

    /* Never mangle after array indexing */
    Mangle_TestHelper            ("a[b.c]",   "a[b.c]");

    /* "this" scope never gets mangled. */
    Mangle_TestHelper            ("this.a", "this.a");

    /* Inner expansions never get mangled. */
    Mangle_TestHelper            ("a_$(s.i)", "a_$(s.i)");
    Mangle_TestHelper            ("a_$(n:i)", "a_$(n:i)");

    /* Only before the inner expansion it gets mangled. */
    Mangle_TestHelper            ("s.a_$(s.i)", "s#a_$(s.i)");
    Mangle_TestHelper            ("n:a_$(n:i)", "n*a_$(n:i)");

    /* Testing non '\0'-terminated char arrays. */

    char A[4] = {'a','b','.','c'};
    Mangle_TestHelperWithLength2(A, 0, "ab.c", 4);
    Mangle_TestHelperWithLength2(A, 4, "ab#c", 4);
    char B[3] = {'a',':','b'};
    Mangle_TestHelperWithLength2(B, 0, "a:b", 3);
    Mangle_TestHelperWithLength2(B, 3, "a*b", 3);
    char C[1] = {'a'};
    Mangle_TestHelperWithLength2(C, 0, "a", 1);
    Mangle_TestHelperWithLength2(C, 1, "a", 1);
}


/* NOTE: the two variables "i" and "j" are defined as empty slists in the
 * EvalContext. */
static void IteratorPrepare_TestHelper(
    const char *promiser,
    size_t expected_wheels_num,
    const char **expected_wheels)
{
    /* INIT EvalContext and Promise. */
    EvalContext *evalctx = EvalContextNew();
    Policy *policy = PolicyNew();
    Bundle *bundle = PolicyAppendBundle(policy, "ns1", "bundle1", "agent",
                                        NULL, NULL);
    PromiseType *promise_type = BundleAppendPromiseType(bundle, "dummy");
    Promise *promise = PromiseTypeAppendPromise(promise_type, promiser,
                                                (Rval) { NULL, RVAL_TYPE_NOPROMISEE },
                                                "any", NULL);
    EvalContextStackPushBundleFrame(evalctx, bundle, NULL, false);
    EvalContextStackPushPromiseTypeFrame(evalctx, promise_type);
    PromiseIterator *iterctx = PromiseIteratorNew(promise);
    char *promiser_copy = xstrdup(promiser);

    /* Insert the variables "i" and "j" as empty Rlists in the EvalContext, so
     * that the iteration engine creates wheels for them. */
    {
        VarRef *ref_i = VarRefParseFromBundle("i", bundle);
        EvalContextVariablePut(evalctx, ref_i,
                               (Rlist *) NULL, CF_DATA_TYPE_STRING_LIST,
                               NULL);
        VarRefDestroy(ref_i);

        VarRef *ref_j = VarRefParseFromBundle("j", bundle);
        EvalContextVariablePut(evalctx, ref_j,
                               (Rlist *) NULL, CF_DATA_TYPE_STRING_LIST,
                               NULL);
        VarRefDestroy(ref_j);
    }

    /* TEST */
    printf("Testing PromiseIteratorPrepare: %s\n", promiser);
    PromiseIteratorPrepare(iterctx, evalctx, promiser_copy);

    /* CHECK */
    assert_true(iterctx->wheels != NULL);

    size_t wheels_num = SeqLength(iterctx->wheels);
    assert_int_equal(wheels_num, expected_wheels_num);

    for (size_t i = 0; i < expected_wheels_num; i++)
    {
        const Wheel *wheel = SeqAt(iterctx->wheels, i);
        assert_string_equal(wheel->varname_unexp, expected_wheels[i]);
    }

    /* CLEANUP */
    free(promiser_copy);
    PromiseIteratorDestroy(iterctx);
    EvalContextStackPopFrame(evalctx);
    EvalContextStackPopFrame(evalctx);
    PolicyDestroy(policy);
    EvalContextDestroy(evalctx);
}
static void test_PromiseIteratorPrepare(void)
{
    IteratorPrepare_TestHelper("", 0, NULL);
    /* No wheel added for "blah" because this variable does not resolve. */
    IteratorPrepare_TestHelper("$(blah)", 0, NULL);
    /* "$(i)" is a valid list, but these syntaxes are not correct. */
    IteratorPrepare_TestHelper("i", 0, NULL);
    IteratorPrepare_TestHelper("$i", 0, NULL);
    IteratorPrepare_TestHelper("$(i", 0, NULL);
    /* The following however is correct and should add one wheel */
    IteratorPrepare_TestHelper("$(i)", 1,
                               (const char *[]) {"i"});
    IteratorPrepare_TestHelper("$(i))", 1,
                               (const char *[]) {"i"});
    IteratorPrepare_TestHelper("$(i)(", 1,
                               (const char *[]) {"i"});
    IteratorPrepare_TestHelper("$(i)$(", 1,
                               (const char *[]) {"i"});
    /* The same variable twice should just add one wheel. */
    IteratorPrepare_TestHelper("$(i)$(i)", 1,
                               (const char *[]) {"i"});
    /* "$(ij)" does not resolve. */
    IteratorPrepare_TestHelper("$(i)$(ij)", 1,
                               (const char *[]) {"i"});
    /* Both "$(i)" and "$(j)" resolve. */
    IteratorPrepare_TestHelper("$(i)$(j)", 2,
                               (const char *[]) {"i","j"});
    IteratorPrepare_TestHelper("$(i))$(j)", 2,
                               (const char *[]) {"i","j"});
    IteratorPrepare_TestHelper("0$(i)$(j)", 2,
                               (const char *[]) {"i","j"});
    IteratorPrepare_TestHelper("$(i)1$(j)", 2,
                               (const char *[]) {"i","j"});
    IteratorPrepare_TestHelper("$(i)$(j)2", 2,
                               (const char *[]) {"i","j"});
    IteratorPrepare_TestHelper("0$(i)1$(j)", 2,
                               (const char *[]) {"i","j"});
    IteratorPrepare_TestHelper("0$(i)1$(j)2", 2,
                               (const char *[]) {"i","j"});
    /* Any variable dependent on other variables should be added as a wheel. */
    IteratorPrepare_TestHelper("$(A[$(i)][$(j)])", 3,
                               (const char *[]) {"i","j", "A[$(i)][$(j)]"});
    /* Even if the inner variables don't resolve. */
    IteratorPrepare_TestHelper("$(A[$(blah)][$(blue)])", 1,
                               (const char *[]) {"A[$(blah)][$(blue)]"});
    IteratorPrepare_TestHelper("$(A[1][2]) $(A[$(i)][$(j)])", 3,
                               (const char *[]) {"i","j", "A[$(i)][$(j)]"});
    IteratorPrepare_TestHelper("$(A[$(B[$(i)])][$(j)])", 4,
                               (const char *[]) {"i", "B[$(i)]", "j", "A[$(B[$(i)])][$(j)]"});
    IteratorPrepare_TestHelper("$(A[$(B[$(i)][$(j)])])", 4,
                               (const char *[]) {"i","j", "B[$(i)][$(j)]", "A[$(B[$(i)][$(j)])]"});
}



int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_FindDollarParen),
        unit_test(test_IsMangled),
        unit_test(test_MangleVarRefString),
        unit_test(test_PromiseIteratorPrepare),
    };

    int ret = run_tests(tests);

    return ret;
}
