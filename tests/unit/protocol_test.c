#include "test.h"

#include <string.h>
#include "cmockery.h"
#include "server.h"

/*
 * Need to declare some functions that are not available.
 */
ProtocolCommand GetCommand(char *str);

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
 * "STARTTLS",
 */

static void test_command_parser(void)
{
    ProtocolCommand parsed = PROTOCOL_COMMAND_BAD;
    ProtocolCommand expected = PROTOCOL_COMMAND_BAD;
    /*
     * Test all the commands, one by one.
     */
    // EXEC
    expected = PROTOCOL_COMMAND_EXEC;
    parsed = GetCommand("EXEC");
    assert_int_equal(expected, parsed);
    // AUTH
    expected = PROTOCOL_COMMAND_AUTH;
    parsed = GetCommand("AUTH");
    assert_int_equal(expected, parsed);
    // GET
    expected = PROTOCOL_COMMAND_GET;
    parsed = GetCommand("GET");
    assert_int_equal(expected, parsed);
    // OPENDIR
    expected = PROTOCOL_COMMAND_OPENDIR;
    parsed = GetCommand("OPENDIR");
    assert_int_equal(expected, parsed);
    // SYNCH
    expected = PROTOCOL_COMMAND_SYNC;
    parsed = GetCommand("SYNCH");
    assert_int_equal(expected, parsed);
    // CLASSES
    expected = PROTOCOL_COMMAND_CONTEXTS;
    parsed = GetCommand("CLASSES");
    assert_int_equal(expected, parsed);
    // MD5
    expected = PROTOCOL_COMMAND_MD5;
    parsed = GetCommand("MD5");
    assert_int_equal(expected, parsed);
    // SMD5
    expected = PROTOCOL_COMMAND_MD5_SECURE;
    parsed = GetCommand("SMD5");
    assert_int_equal(expected, parsed);
    // CAUTH
    expected = PROTOCOL_COMMAND_AUTH_CLEAR;
    parsed = GetCommand("CAUTH");
    assert_int_equal(expected, parsed);
    // SAUTH
    expected = PROTOCOL_COMMAND_AUTH_SECURE;
    parsed = GetCommand("SAUTH");
    assert_int_equal(expected, parsed);
    // SSYNCH
    expected = PROTOCOL_COMMAND_SYNC_SECURE;
    parsed = GetCommand("SSYNCH");
    assert_int_equal(expected, parsed);
    // SGET
    expected = PROTOCOL_COMMAND_GET_SECURE;
    parsed = GetCommand("SGET");
    assert_int_equal(expected, parsed);
    // VERSION
    expected = PROTOCOL_COMMAND_VERSION;
    parsed = GetCommand("VERSION");
    assert_int_equal(expected, parsed);
    // SOPENDIR
    expected = PROTOCOL_COMMAND_OPENDIR_SECURE;
    parsed = GetCommand("SOPENDIR");
    assert_int_equal(expected, parsed);
    // VAR
    expected = PROTOCOL_COMMAND_VAR;
    parsed = GetCommand("VAR");
    assert_int_equal(expected, parsed);
    // SVAR
    expected = PROTOCOL_COMMAND_VAR_SECURE;
    parsed = GetCommand("SVAR");
    assert_int_equal(expected, parsed);
    // CONTEXT
    expected = PROTOCOL_COMMAND_CONTEXT;
    parsed = GetCommand("CONTEXT");
    assert_int_equal(expected, parsed);
    // SCONTEXT
    expected = PROTOCOL_COMMAND_CONTEXT_SECURE;
    parsed = GetCommand("SCONTEXT");
    assert_int_equal(expected, parsed);
    // SQUERY
    expected = PROTOCOL_COMMAND_QUERY_SECURE;
    parsed = GetCommand("SQUERY");
    assert_int_equal(expected, parsed);
    // SCALLBACK
    expected = PROTOCOL_COMMAND_CALL_ME_BACK;
    parsed = GetCommand("SCALLBACK");
    assert_int_equal(expected, parsed);
    // STARTTLS
    expected = PROTOCOL_COMMAND_STARTTLS;
    parsed = GetCommand("STARTTLS");
    assert_int_equal(expected, parsed);
    /*
     * Try using lowercase
     */
    // EXEC
    expected = PROTOCOL_COMMAND_BAD;
    parsed = GetCommand("exec");
    assert_int_equal(expected, parsed);
    // AUTH
    parsed = GetCommand("auth");
    assert_int_equal(expected, parsed);
    // GET
    parsed = GetCommand("get");
    assert_int_equal(expected, parsed);
    // OPENDIR
    parsed = GetCommand("opendir");
    assert_int_equal(expected, parsed);
    // SYNCH
    parsed = GetCommand("synch");
    assert_int_equal(expected, parsed);
    // CLASSES
    parsed = GetCommand("classes");
    assert_int_equal(expected, parsed);
    // MD5
    parsed = GetCommand("md5");
    assert_int_equal(expected, parsed);
    // SMD5
    parsed = GetCommand("smd5");
    assert_int_equal(expected, parsed);
    // CAUTH
    parsed = GetCommand("cauth");
    assert_int_equal(expected, parsed);
    // SAUTH
    parsed = GetCommand("sauth");
    assert_int_equal(expected, parsed);
    // SSYNCH
    parsed = GetCommand("synch");
    assert_int_equal(expected, parsed);
    // SGET
    parsed = GetCommand("sget");
    assert_int_equal(expected, parsed);
    // VERSION
    parsed = GetCommand("version");
    assert_int_equal(expected, parsed);
    // SOPENDIR
    parsed = GetCommand("sopendir");
    assert_int_equal(expected, parsed);
    // VAR
    parsed = GetCommand("var");
    assert_int_equal(expected, parsed);
    // SVAR
    parsed = GetCommand("svar");
    assert_int_equal(expected, parsed);
    // CONTEXT
    parsed = GetCommand("context");
    assert_int_equal(expected, parsed);
    // SCONTEXT
    parsed = GetCommand("scontext");
    assert_int_equal(expected, parsed);
    // SQUERY
    parsed = GetCommand("squery");
    assert_int_equal(expected, parsed);
    // SCALLBACK
    parsed = GetCommand("scallback");
    assert_int_equal(expected, parsed);
    // STARTTLS
    parsed = GetCommand("starttls");
    assert_int_equal(expected, parsed);
    /*
     * Try the commands with something in front
     */
    // EXEC
    expected = PROTOCOL_COMMAND_BAD;
    parsed = GetCommand("eEXEC");
    assert_int_equal(expected, parsed);
    // AUTH
    parsed = GetCommand("aAUTH");
    assert_int_equal(expected, parsed);
    // GET
    parsed = GetCommand("gGET");
    assert_int_equal(expected, parsed);
    // OPENDIR
    parsed = GetCommand("oOPENDIR");
    assert_int_equal(expected, parsed);
    // SYNCH
    parsed = GetCommand("sSYNCH");
    assert_int_equal(expected, parsed);
    // CLASSES
    parsed = GetCommand("cCLASSES");
    assert_int_equal(expected, parsed);
    // MD5
    parsed = GetCommand("mMD5");
    assert_int_equal(expected, parsed);
    // SMD5
    parsed = GetCommand("sMD5");
    assert_int_equal(expected, parsed);
    // CAUTH
    parsed = GetCommand("cCAUTH");
    assert_int_equal(expected, parsed);
    // SAUTH
    parsed = GetCommand("sSAUTH");
    assert_int_equal(expected, parsed);
    // SSYNCH
    parsed = GetCommand("sSSYNCH");
    assert_int_equal(expected, parsed);
    // SGET
    parsed = GetCommand("sSGET");
    assert_int_equal(expected, parsed);
    // VERSION
    parsed = GetCommand("vVERSION");
    assert_int_equal(expected, parsed);
    // SOPENDIR
    parsed = GetCommand("sSOPENDIR");
    assert_int_equal(expected, parsed);
    // VAR
    parsed = GetCommand("vVAR");
    assert_int_equal(expected, parsed);
    // SVAR
    parsed = GetCommand("sSVAR");
    assert_int_equal(expected, parsed);
    // CONTEXT
    parsed = GetCommand("cCONTEXT");
    assert_int_equal(expected, parsed);
    // SCONTEXT
    parsed = GetCommand("sSCONTEXT");
    assert_int_equal(expected, parsed);
    // SQUERY
    parsed = GetCommand("sSQUERY");
    assert_int_equal(expected, parsed);
    // SCALLBACK
    parsed = GetCommand("sSCALLBACK");
    assert_int_equal(expected, parsed);
    // STARTTLS
    parsed = GetCommand("sSTARTTLS");
    assert_int_equal(expected, parsed);
    /*
     * Try the commands with something after them
     */
    // EXEC
    expected = PROTOCOL_COMMAND_BAD;
    parsed = GetCommand("EXECx");
    assert_int_equal(expected, parsed);
    // AUTH
    parsed = GetCommand("AUTHx");
    assert_int_equal(expected, parsed);
    // GET
    parsed = GetCommand("GETx");
    assert_int_equal(expected, parsed);
    // OPENDIR
    parsed = GetCommand("OPENDIRx");
    assert_int_equal(expected, parsed);
    // SYNCH
    parsed = GetCommand("SYNCHx");
    assert_int_equal(expected, parsed);
    // CLASSES
    parsed = GetCommand("CLASSESx");
    assert_int_equal(expected, parsed);
    // MD5
    parsed = GetCommand("MD5x");
    assert_int_equal(expected, parsed);
    // SMD5                                                                                 
    parsed = GetCommand("SMD5x");
    assert_int_equal(expected, parsed);                                                             
    // CAUTH                                                                                            
    parsed = GetCommand("CAUTHx");
    assert_int_equal(expected, parsed);                                                                         
    // SAUTH                                                                                                        
    parsed = GetCommand("SAUTHx");
    assert_int_equal(expected, parsed);                                                                                     
    // SSYNCH                                                                                                                   
    parsed = GetCommand("SSYNCHx");
    assert_int_equal(expected, parsed);                                                                                                 
    // SGET                                                                                                                                 
    parsed = GetCommand("SGETx");
    assert_int_equal(expected, parsed);                                                                                                             
    // VERSION                                                                                                                                          
    parsed = GetCommand("VERSIONx");
    assert_int_equal(expected, parsed);
    // SOPENDIR
    parsed = GetCommand("SOPENDIRx");
    assert_int_equal(expected, parsed);
    // VAR
    parsed = GetCommand("VARx");
    assert_int_equal(expected, parsed);
    // SVAR
    parsed = GetCommand("SVARx");
    assert_int_equal(expected, parsed);
    // CONTEXT
    parsed = GetCommand("CONTEXTx");
    assert_int_equal(expected, parsed);
    // SCONTEXT
    parsed = GetCommand("SCONTEXTx");
    assert_int_equal(expected, parsed);
    // SQUERY
    parsed = GetCommand("SQUERYx");
    assert_int_equal(expected, parsed);
    // SCALLBACK
    parsed = GetCommand("SCALLBACKx");
    assert_int_equal(expected, parsed);
    // STARTTLS
    parsed = GetCommand("STARTTLSx");
    assert_int_equal(expected, parsed);
    /*
     * Try some common mispellings.
     */
    // EXEC
    expected = PROTOCOL_COMMAND_BAD;
    parsed = GetCommand("EXE");
    assert_int_equal(expected, parsed);
    parsed = GetCommand("EXECUTE");
    assert_int_equal(expected, parsed);
    // AUTH
    parsed = GetCommand("AUTHORIZATION");
    assert_int_equal(expected, parsed);
    // SYNCH
    parsed = GetCommand("SYNC");
    assert_int_equal(expected, parsed);
    parsed = GetCommand("SYNCHRONIZE");
    assert_int_equal(expected, parsed);
    // CLASSES
    parsed = GetCommand("CLASS");
    assert_int_equal(expected, parsed);
    // CAUTH
    parsed = GetCommand("CAUTHORIZATION");
    assert_int_equal(expected, parsed);
    // SAUTH
    parsed = GetCommand("SAUTHORIZATION");
    assert_int_equal(expected, parsed);
    // SSYNCH
    parsed = GetCommand("SSYNCHRONIZE");
    assert_int_equal(expected, parsed);
    parsed = GetCommand("SSYNC");
    assert_int_equal(expected, parsed);
    // VERSION                                                                                                                                          
    parsed = GetCommand("V");
    assert_int_equal(expected, parsed);
    // VAR
    parsed = GetCommand("VARIABLE");
    assert_int_equal(expected, parsed);
    // SVAR
    parsed = GetCommand("SVARIABLE");
    assert_int_equal(expected, parsed);
    /*
     * Finally, try the commands with a space and something else, they should be recognized"
     */
    // EXEC
    expected = PROTOCOL_COMMAND_EXEC;
    parsed = GetCommand("EXEC 123");
    assert_int_equal(expected, parsed);
    // AUTH
    expected = PROTOCOL_COMMAND_AUTH;
    parsed = GetCommand("AUTH 123");
    assert_int_equal(expected, parsed);
    // GET
    expected = PROTOCOL_COMMAND_GET;
    parsed = GetCommand("GET 123");
    assert_int_equal(expected, parsed);
    // OPENDIR
    expected = PROTOCOL_COMMAND_OPENDIR;
    parsed = GetCommand("OPENDIR 123");
    assert_int_equal(expected, parsed);
    // SYNCH
    expected = PROTOCOL_COMMAND_SYNC;
    parsed = GetCommand("SYNCH 123");
    assert_int_equal(expected, parsed);
    // CLASSES
    expected = PROTOCOL_COMMAND_CONTEXTS;
    parsed = GetCommand("CLASSES 123");
    assert_int_equal(expected, parsed);
    // MD5
    expected = PROTOCOL_COMMAND_MD5;
    parsed = GetCommand("MD5 123");
    assert_int_equal(expected, parsed);
    // SMD5
    expected = PROTOCOL_COMMAND_MD5_SECURE;
    parsed = GetCommand("SMD5 123");
    assert_int_equal(expected, parsed);
    // CAUTH
    expected = PROTOCOL_COMMAND_AUTH_CLEAR;
    parsed = GetCommand("CAUTH 123");
    assert_int_equal(expected, parsed);
    // SAUTH
    expected = PROTOCOL_COMMAND_AUTH_SECURE;
    parsed = GetCommand("SAUTH 123");
    assert_int_equal(expected, parsed);
    // SSYNCH
    expected = PROTOCOL_COMMAND_SYNC_SECURE;
    parsed = GetCommand("SSYNCH 123");
    assert_int_equal(expected, parsed);
    // SGET
    expected = PROTOCOL_COMMAND_GET_SECURE;
    parsed = GetCommand("SGET 123");
    assert_int_equal(expected, parsed);
    // VERSION
    expected = PROTOCOL_COMMAND_VERSION;
    parsed = GetCommand("VERSION 123");
    assert_int_equal(expected, parsed);
    // SOPENDIR
    expected = PROTOCOL_COMMAND_OPENDIR_SECURE;
    parsed = GetCommand("SOPENDIR 123");
    assert_int_equal(expected, parsed);
    // VAR
    expected = PROTOCOL_COMMAND_VAR;
    parsed = GetCommand("VAR 123");
    assert_int_equal(expected, parsed);
    // SVAR
    expected = PROTOCOL_COMMAND_VAR_SECURE;
    parsed = GetCommand("SVAR 123");
    assert_int_equal(expected, parsed);
    // CONTEXT
    expected = PROTOCOL_COMMAND_CONTEXT;
    parsed = GetCommand("CONTEXT 123");
    assert_int_equal(expected, parsed);
    // SCONTEXT
    expected = PROTOCOL_COMMAND_CONTEXT_SECURE;
    parsed = GetCommand("SCONTEXT 123");
    assert_int_equal(expected, parsed);
    // SQUERY
    expected = PROTOCOL_COMMAND_QUERY_SECURE;
    parsed = GetCommand("SQUERY 123");
    assert_int_equal(expected, parsed);
    // SCALLBACK
    expected = PROTOCOL_COMMAND_CALL_ME_BACK;
    parsed = GetCommand("SCALLBACK 123");
    assert_int_equal(expected, parsed);
    // STARTTLS
    expected = PROTOCOL_COMMAND_STARTTLS;
    parsed = GetCommand("STARTTLS 123");
    assert_int_equal(expected, parsed);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
          unit_test(test_command_parser)
    };

    return run_tests(tests);
}
