#include <test.h>

#include <eval_context.h>
#include <string_lib.h>

static inline
PackageModuleBody *make_mock_package_module(const char *name, int updates_ifel, int installed_ifel, Rlist *options)
{
    PackageModuleBody *pm = xmalloc(sizeof(PackageModuleBody));
    pm->name = SafeStringDuplicate(name);
    pm->installed_ifelapsed = installed_ifel;
    pm->updates_ifelapsed = updates_ifel;
    pm->options = RlistCopy(options);
    return pm;
}

static void test_add_module_to_context()
{
    EvalContext *ctx = EvalContextNew();

    PackageModuleBody *pm = make_mock_package_module("apt_get", 120, 240, NULL);
    AddPackageModuleToContext(ctx, pm);
   
    PackageModuleBody *pm2 = make_mock_package_module("yum", 220, 440, NULL);
    AddPackageModuleToContext(ctx, pm2);

    PackagePromiseContext *pp_ctx = GetPackagePromiseContext(ctx);

    assert_true(pp_ctx != NULL);
    assert_int_equal(2, SeqLength(pp_ctx->package_modules_bodies));

    PackageModuleBody *yum = GetPackageModuleFromContext(ctx, "yum");
    assert_true(yum != NULL);
    assert_int_equal(220, yum->updates_ifelapsed);
    assert_int_equal(440, yum->installed_ifelapsed);

    /* make sure that adding body with the same name will not make set larger */
    PackageModuleBody *pm3 = make_mock_package_module("yum", 330, 550, NULL);
    AddPackageModuleToContext(ctx, pm3);

    assert_int_equal(2, SeqLength(pp_ctx->package_modules_bodies));

    /* check if parameters are updated */
    yum = GetPackageModuleFromContext(ctx, "yum");
    assert_int_equal(330, yum->updates_ifelapsed);
    assert_int_equal(550, yum->installed_ifelapsed);

    EvalContextDestroy(ctx);
}

static void test_default_package_module_settings()
{
    EvalContext *ctx = EvalContextNew();

    PackageModuleBody *pm = make_mock_package_module("apt_get", 120, 240, NULL);
    AddPackageModuleToContext(ctx, pm);
   
    PackageModuleBody *pm2 = make_mock_package_module("yum", 220, 440, NULL);
    AddPackageModuleToContext(ctx, pm2);
    
    PackageModuleBody *pm3 = make_mock_package_module("yum_2", 220, 440, NULL);
    AddPackageModuleToContext(ctx, pm3);

    AddDefaultPackageModuleToContext(ctx, "apt_get");
    PackageModuleBody *def_pm = GetDefaultPackageModuleFromContext(ctx);
    assert_string_equal("apt_get", def_pm->name);
    
    AddDefaultPackageModuleToContext(ctx, "yum");
    def_pm = GetDefaultPackageModuleFromContext(ctx);
    assert_string_equal("yum", def_pm->name);

    EvalContextDestroy(ctx);
}


int main()
{
    PRINT_TEST_BANNER();

    const UnitTest tests[] =
    {
        unit_test(test_default_package_module_settings),
        unit_test(test_add_module_to_context),
    };

    int ret = run_tests(tests);

    return ret;
}
