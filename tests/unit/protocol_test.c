#include <test.h>

#include <string.h>
#include <cmockery.h>
#include <server.h>
#include <server_common.h>
#include <server_classic.h>

#include <server_classic.c>                            /* GetCommandClassic */


/*
 * The protocol consists of the following commands:
 * "EXEC",
 * "AUTH",
 * "GET",
 * "OPENDIR",
 * "SYNCH",
 * "CLASSES",
 * "MD5",
 * "SMD5",
 * "CAUTH",
 * "SAUTH",
 * "SSYNCH",
 * "SGET",
 * "VERSION",
 * "SOPENDIR",
 * "VAR",
 * "SVAR",
 * "CONTEXT",
 * "SCONTEXT",
 * "SQUERY",
 * "SCALLBACK",
 */

static void test_command_parser(void)
{
    ProtocolCommandClassic parsed = PROTOCOL_COMMAND_BAD;
    ProtocolCommandClassic expected = PROTOCOL_COMMAND_BAD;
    /*
     * Test all the commands, one by one.
     */
    // EXEC
    expected = PROTOCOL_COMMAND_EXEC;
    parsed = GetCommandClassic("EXEC");
    assert_int_equal(expected, parsed);
    // AUTH
    expected = PROTOCOL_COMMAND_AUTH;
    parsed = GetCommandClassic("AUTH");
    assert_int_equal(expected, parsed);
    // GET
    expected = PROTOCOL_COMMAND_GET;
    parsed = GetCommandClassic("GET");
    assert_int_equal(expected, parsed);
    // OPENDIR
    expected = PROTOCOL_COMMAND_OPENDIR;
    parsed = GetCommandClassic("OPENDIR");
    assert_int_equal(expected, parsed);
    // SYNCH
    expected = PROTOCOL_COMMAND_SYNC;
    parsed = GetCommandClassic("SYNCH");
    assert_int_equal(expected, parsed);
    // CLASSES
    expected = PROTOCOL_COMMAND_CONTEXTS;
    parsed = GetCommandClassic("CLASSES");
    assert_int_equal(expected, parsed);
    // MD5
    expected = PROTOCOL_COMMAND_MD5;
    parsed = GetCommandClassic("MD5");
    assert_int_equal(expected, parsed);
    // SMD5
    expected = PROTOCOL_COMMAND_MD5_SECURE;
    parsed = GetCommandClassic("SMD5");
    assert_int_equal(expected, parsed);
    // CAUTH
    expected = PROTOCOL_COMMAND_AUTH_PLAIN;
    parsed = GetCommandClassic("CAUTH");
    assert_int_equal(expected, parsed);
    // SAUTH
    expected = PROTOCOL_COMMAND_AUTH_SECURE;
    parsed = GetCommandClassic("SAUTH");
    assert_int_equal(expected, parsed);
    // SSYNCH
    expected = PROTOCOL_COMMAND_SYNC_SECURE;
    parsed = GetCommandClassic("SSYNCH");
    assert_int_equal(expected, parsed);
    // SGET
    expected = PROTOCOL_COMMAND_GET_SECURE;
    parsed = GetCommandClassic("SGET");
    assert_int_equal(expected, parsed);
    // VERSION
    expected = PROTOCOL_COMMAND_VERSION;
    parsed = GetCommandClassic("VERSION");
    assert_int_equal(expected, parsed);
    // SOPENDIR
    expected = PROTOCOL_COMMAND_OPENDIR_SECURE;
    parsed = GetCommandClassic("SOPENDIR");
    assert_int_equal(expected, parsed);
    // VAR
    expected = PROTOCOL_COMMAND_VAR;
    parsed = GetCommandClassic("VAR");
    assert_int_equal(expected, parsed);
    // SVAR
    expected = PROTOCOL_COMMAND_VAR_SECURE;
    parsed = GetCommandClassic("SVAR");
    assert_int_equal(expected, parsed);
    // CONTEXT
    expected = PROTOCOL_COMMAND_CONTEXT;
    parsed = GetCommandClassic("CONTEXT");
    assert_int_equal(expected, parsed);
    // SCONTEXT
    expected = PROTOCOL_COMMAND_CONTEXT_SECURE;
    parsed = GetCommandClassic("SCONTEXT");
    assert_int_equal(expected, parsed);
    // SQUERY
    expected = PROTOCOL_COMMAND_QUERY_SECURE;
    parsed = GetCommandClassic("SQUERY");
    assert_int_equal(expected, parsed);
    // SCALLBACK
    expected = PROTOCOL_COMMAND_CALL_ME_BACK;
    parsed = GetCommandClassic("SCALLBACK");
    assert_int_equal(expected, parsed);
    /*
     * Try using lowercase
     */
    // EXEC
    expected = PROTOCOL_COMMAND_BAD;
    parsed = GetCommandClassic("exec");
    assert_int_equal(expected, parsed);
    // AUTH
    parsed = GetCommandClassic("auth");
    assert_int_equal(expected, parsed);
    // GET
    parsed = GetCommandClassic("get");
    assert_int_equal(expected, parsed);
    // OPENDIR
    parsed = GetCommandClassic("opendir");
    assert_int_equal(expected, parsed);
    // SYNCH
    parsed = GetCommandClassic("synch");
    assert_int_equal(expected, parsed);
    // CLASSES
    parsed = GetCommandClassic("classes");
    assert_int_equal(expected, parsed);
    // MD5
    parsed = GetCommandClassic("md5");
    assert_int_equal(expected, parsed);
    // SMD5
    parsed = GetCommandClassic("smd5");
    assert_int_equal(expected, parsed);
    // CAUTH
    parsed = GetCommandClassic("cauth");
    assert_int_equal(expected, parsed);
    // SAUTH
    parsed = GetCommandClassic("sauth");
    assert_int_equal(expected, parsed);
    // SSYNCH
    parsed = GetCommandClassic("synch");
    assert_int_equal(expected, parsed);
    // SGET
    parsed = GetCommandClassic("sget");
    assert_int_equal(expected, parsed);
    // VERSION
    parsed = GetCommandClassic("version");
    assert_int_equal(expected, parsed);
    // SOPENDIR
    parsed = GetCommandClassic("sopendir");
    assert_int_equal(expected, parsed);
    // VAR
    parsed = GetCommandClassic("var");
    assert_int_equal(expected, parsed);
    // SVAR
    parsed = GetCommandClassic("svar");
    assert_int_equal(expected, parsed);
    // CONTEXT
    parsed = GetCommandClassic("context");
    assert_int_equal(expected, parsed);
    // SCONTEXT
    parsed = GetCommandClassic("scontext");
    assert_int_equal(expected, parsed);
    // SQUERY
    parsed = GetCommandClassic("squery");
    assert_int_equal(expected, parsed);
    // SCALLBACK
    parsed = GetCommandClassic("scallback");
    assert_int_equal(expected, parsed);
    /*
     * Try the commands with something in front
     */
    // EXEC
    expected = PROTOCOL_COMMAND_BAD;
    parsed = GetCommandClassic("eEXEC");
    assert_int_equal(expected, parsed);
    // AUTH
    parsed = GetCommandClassic("aAUTH");
    assert_int_equal(expected, parsed);
    // GET
    parsed = GetCommandClassic("gGET");
    assert_int_equal(expected, parsed);
    // OPENDIR
    parsed = GetCommandClassic("oOPENDIR");
    assert_int_equal(expected, parsed);
    // SYNCH
    parsed = GetCommandClassic("sSYNCH");
    assert_int_equal(expected, parsed);
    // CLASSES
    parsed = GetCommandClassic("cCLASSES");
    assert_int_equal(expected, parsed);
    // MD5
    parsed = GetCommandClassic("mMD5");
    assert_int_equal(expected, parsed);
    // SMD5
    parsed = GetCommandClassic("sMD5");
    assert_int_equal(expected, parsed);
    // CAUTH
    parsed = GetCommandClassic("cCAUTH");
    assert_int_equal(expected, parsed);
    // SAUTH
    parsed = GetCommandClassic("sSAUTH");
    assert_int_equal(expected, parsed);
    // SSYNCH
    parsed = GetCommandClassic("sSSYNCH");
    assert_int_equal(expected, parsed);
    // SGET
    parsed = GetCommandClassic("sSGET");
    assert_int_equal(expected, parsed);
    // VERSION
    parsed = GetCommandClassic("vVERSION");
    assert_int_equal(expected, parsed);
    // SOPENDIR
    parsed = GetCommandClassic("sSOPENDIR");
    assert_int_equal(expected, parsed);
    // VAR
    parsed = GetCommandClassic("vVAR");
    assert_int_equal(expected, parsed);
    // SVAR
    parsed = GetCommandClassic("sSVAR");
    assert_int_equal(expected, parsed);
    // CONTEXT
    parsed = GetCommandClassic("cCONTEXT");
    assert_int_equal(expected, parsed);
    // SCONTEXT
    parsed = GetCommandClassic("sSCONTEXT");
    assert_int_equal(expected, parsed);
    // SQUERY
    parsed = GetCommandClassic("sSQUERY");
    assert_int_equal(expected, parsed);
    // SCALLBACK
    parsed = GetCommandClassic("sSCALLBACK");
    assert_int_equal(expected, parsed);
    /*
     * Try the commands with something after them
     */
    // EXEC
    expected = PROTOCOL_COMMAND_BAD;
    parsed = GetCommandClassic("EXECx");
    assert_int_equal(expected, parsed);
    // AUTH
    parsed = GetCommandClassic("AUTHx");
    assert_int_equal(expected, parsed);
    // GET
    parsed = GetCommandClassic("GETx");
    assert_int_equal(expected, parsed);
    // OPENDIR
    parsed = GetCommandClassic("OPENDIRx");
    assert_int_equal(expected, parsed);
    // SYNCH
    parsed = GetCommandClassic("SYNCHx");
    assert_int_equal(expected, parsed);
    // CLASSES
    parsed = GetCommandClassic("CLASSESx");
    assert_int_equal(expected, parsed);
    // MD5
    parsed = GetCommandClassic("MD5x");
    assert_int_equal(expected, parsed);
    // SMD5
    parsed = GetCommandClassic("SMD5x");
    assert_int_equal(expected, parsed);
    // CAUTH
    parsed = GetCommandClassic("CAUTHx");
    assert_int_equal(expected, parsed);
    // SAUTH
    parsed = GetCommandClassic("SAUTHx");
    assert_int_equal(expected, parsed);
    // SSYNCH
    parsed = GetCommandClassic("SSYNCHx");
    assert_int_equal(expected, parsed);
    // SGET
    parsed = GetCommandClassic("SGETx");
    assert_int_equal(expected, parsed);
    // VERSION
    parsed = GetCommandClassic("VERSIONx");
    assert_int_equal(expected, parsed);
    // SOPENDIR
    parsed = GetCommandClassic("SOPENDIRx");
    assert_int_equal(expected, parsed);
    // VAR
    parsed = GetCommandClassic("VARx");
    assert_int_equal(expected, parsed);
    // SVAR
    parsed = GetCommandClassic("SVARx");
    assert_int_equal(expected, parsed);
    // CONTEXT
    parsed = GetCommandClassic("CONTEXTx");
    assert_int_equal(expected, parsed);
    // SCONTEXT
    parsed = GetCommandClassic("SCONTEXTx");
    assert_int_equal(expected, parsed);
    // SQUERY
    parsed = GetCommandClassic("SQUERYx");
    assert_int_equal(expected, parsed);
    // SCALLBACK
    parsed = GetCommandClassic("SCALLBACKx");
    assert_int_equal(expected, parsed);
    /*
     * Try some common mispellings.
     */
    // EXEC
    expected = PROTOCOL_COMMAND_BAD;
    parsed = GetCommandClassic("EXE");
    assert_int_equal(expected, parsed);
    parsed = GetCommandClassic("EXECUTE");
    assert_int_equal(expected, parsed);
    // AUTH
    parsed = GetCommandClassic("AUTHORIZATION");
    assert_int_equal(expected, parsed);
    // SYNCH
    parsed = GetCommandClassic("SYNC");
    assert_int_equal(expected, parsed);
    parsed = GetCommandClassic("SYNCHRONIZE");
    assert_int_equal(expected, parsed);
    // CLASSES
    parsed = GetCommandClassic("CLASS");
    assert_int_equal(expected, parsed);
    // CAUTH
    parsed = GetCommandClassic("CAUTHORIZATION");
    assert_int_equal(expected, parsed);
    // SAUTH
    parsed = GetCommandClassic("SAUTHORIZATION");
    assert_int_equal(expected, parsed);
    // SSYNCH
    parsed = GetCommandClassic("SSYNCHRONIZE");
    assert_int_equal(expected, parsed);
    parsed = GetCommandClassic("SSYNC");
    assert_int_equal(expected, parsed);
    // VERSION
    parsed = GetCommandClassic("V");
    assert_int_equal(expected, parsed);
    // VAR
    parsed = GetCommandClassic("VARIABLE");
    assert_int_equal(expected, parsed);
    // SVAR
    parsed = GetCommandClassic("SVARIABLE");
    assert_int_equal(expected, parsed);
    /*
     * Finally, try the commands with a space and something else, they should be recognized"
     */
    // EXEC
    expected = PROTOCOL_COMMAND_EXEC;
    parsed = GetCommandClassic("EXEC 123");
    assert_int_equal(expected, parsed);
    // AUTH
    expected = PROTOCOL_COMMAND_AUTH;
    parsed = GetCommandClassic("AUTH 123");
    assert_int_equal(expected, parsed);
    // GET
    expected = PROTOCOL_COMMAND_GET;
    parsed = GetCommandClassic("GET 123");
    assert_int_equal(expected, parsed);
    // OPENDIR
    expected = PROTOCOL_COMMAND_OPENDIR;
    parsed = GetCommandClassic("OPENDIR 123");
    assert_int_equal(expected, parsed);
    // SYNCH
    expected = PROTOCOL_COMMAND_SYNC;
    parsed = GetCommandClassic("SYNCH 123");
    assert_int_equal(expected, parsed);
    // CLASSES
    expected = PROTOCOL_COMMAND_CONTEXTS;
    parsed = GetCommandClassic("CLASSES 123");
    assert_int_equal(expected, parsed);
    // MD5
    expected = PROTOCOL_COMMAND_MD5;
    parsed = GetCommandClassic("MD5 123");
    assert_int_equal(expected, parsed);
    // SMD5
    expected = PROTOCOL_COMMAND_MD5_SECURE;
    parsed = GetCommandClassic("SMD5 123");
    assert_int_equal(expected, parsed);
    // CAUTH
    expected = PROTOCOL_COMMAND_AUTH_PLAIN;
    parsed = GetCommandClassic("CAUTH 123");
    assert_int_equal(expected, parsed);
    // SAUTH
    expected = PROTOCOL_COMMAND_AUTH_SECURE;
    parsed = GetCommandClassic("SAUTH 123");
    assert_int_equal(expected, parsed);
    // SSYNCH
    expected = PROTOCOL_COMMAND_SYNC_SECURE;
    parsed = GetCommandClassic("SSYNCH 123");
    assert_int_equal(expected, parsed);
    // SGET
    expected = PROTOCOL_COMMAND_GET_SECURE;
    parsed = GetCommandClassic("SGET 123");
    assert_int_equal(expected, parsed);
    // VERSION
    expected = PROTOCOL_COMMAND_VERSION;
    parsed = GetCommandClassic("VERSION 123");
    assert_int_equal(expected, parsed);
    // SOPENDIR
    expected = PROTOCOL_COMMAND_OPENDIR_SECURE;
    parsed = GetCommandClassic("SOPENDIR 123");
    assert_int_equal(expected, parsed);
    // VAR
    expected = PROTOCOL_COMMAND_VAR;
    parsed = GetCommandClassic("VAR 123");
    assert_int_equal(expected, parsed);
    // SVAR
    expected = PROTOCOL_COMMAND_VAR_SECURE;
    parsed = GetCommandClassic("SVAR 123");
    assert_int_equal(expected, parsed);
    // CONTEXT
    expected = PROTOCOL_COMMAND_CONTEXT;
    parsed = GetCommandClassic("CONTEXT 123");
    assert_int_equal(expected, parsed);
    // SCONTEXT
    expected = PROTOCOL_COMMAND_CONTEXT_SECURE;
    parsed = GetCommandClassic("SCONTEXT 123");
    assert_int_equal(expected, parsed);
    // SQUERY
    expected = PROTOCOL_COMMAND_QUERY_SECURE;
    parsed = GetCommandClassic("SQUERY 123");
    assert_int_equal(expected, parsed);
    // SCALLBACK
    expected = PROTOCOL_COMMAND_CALL_ME_BACK;
    parsed = GetCommandClassic("SCALLBACK 123");
    assert_int_equal(expected, parsed);
}

static void test_user_name(void)
{
    char invalid_user_name[] = "user\\";
    char invalid_user_name2[] = "user//";
    char invalid_user_name3[] = "//\\";
    char valid_user_name[] = "valid_user";

    assert_false(IsUserNameValid(invalid_user_name));
    assert_false(IsUserNameValid(invalid_user_name2));
    assert_false(IsUserNameValid(invalid_user_name3));
    assert_true(IsUserNameValid(valid_user_name));
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
          unit_test(test_command_parser),
          unit_test(test_user_name)
    };

    return run_tests(tests);
}
