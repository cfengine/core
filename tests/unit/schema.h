#ifndef SCHEMA_H_
# define SCHEMA_H_

// Printf formatting for xml CUNIT Schema
#define CUNIT_INIT \
    "<\?xml version=\"1.0\" \?>\n" \
    "<\?xml-stylesheet type=\"text/xsl\" href=\"CUnit-Run.xsl\" \?>\n" \
    "<!DOCTYPE CUNIT_TEST_RUN_REPORT SYSTEM \"CUnit-Run.dtd\">\n" \
    "<CUNIT_TEST_RUN_REPORT>\n" \
    "  <CUNIT_HEADER/>\n" \
    "  <CUNIT_RESULT_LISTING>\n" \
    "    <CUNIT_RUN_SUITE>\n" \
    "      <CUNIT_RUN_SUITE_SUCCESS>\n" \
    "        <SUITE_NAME> %s suite </SUITE_NAME>\n"
#define CUNIT_RUN_TEST_SUCCESS \
    "        <CUNIT_RUN_TEST_RECORD>\n" \
    "          <CUNIT_RUN_TEST_SUCCESS>\n" \
    "            <TEST_NAME> %s </TEST_NAME>\n" \
    "          </CUNIT_RUN_TEST_SUCCESS>\n" \
    "        </CUNIT_RUN_TEST_RECORD>\n"
#define CUNIT_RUN_TEST_FAILURE_START \
    "        <CUNIT_RUN_TEST_RECORD>\n" \
    "          <CUNIT_RUN_TEST_FAILURE>\n" \
    "            <TEST_NAME> %s </TEST_NAME>\n"
#define CUNIT_RUN_TEST_FAILURE_ASSERT \
    "        <CUNIT_RUN_TEST_RECORD>\n" \
    "          <CUNIT_RUN_TEST_FAILURE>\n" \
    "            <TEST_NAME> %s </TEST_NAME>\n" \
    "            <FILE_NAME> %s </FILE_NAME>\n" \
    "            <LINE_NUMBER> %d </LINE_NUMBER>\n" \
    "            <CONDITION> %s(%lld) </CONDITION>\n" \
    "          </CUNIT_RUN_TEST_FAILURE>\n" \
    "        </CUNIT_RUN_TEST_RECORD>\n"
#define CUNIT_RUN_TEST_FAILURE_ASSERT_EQUALITY_LLD \
    "        <CUNIT_RUN_TEST_RECORD>\n" \
    "          <CUNIT_RUN_TEST_FAILURE>\n" \
    "            <TEST_NAME> %s </TEST_NAME>\n" \
    "            <FILE_NAME> %s </FILE_NAME>\n" \
    "            <LINE_NUMBER> %d </LINE_NUMBER>\n" \
    "            <CONDITION> %s(%lld, %lld) </CONDITION>\n" \
    "          </CUNIT_RUN_TEST_FAILURE>\n" \
    "        </CUNIT_RUN_TEST_RECORD>\n"
#define CUNIT_RUN_TEST_FAILURE_ASSERT_EQUALITY_STRING \
    "        <CUNIT_RUN_TEST_RECORD>\n" \
    "          <CUNIT_RUN_TEST_FAILURE>\n" \
    "            <TEST_NAME> %s </TEST_NAME>\n" \
    "            <FILE_NAME> %s </FILE_NAME>\n" \
    "            <LINE_NUMBER> %d </LINE_NUMBER>\n" \
    "            <CONDITION> %s(%s %s) </CONDITION>\n" \
    "          </CUNIT_RUN_TEST_FAILURE>\n" \
    "        </CUNIT_RUN_TEST_RECORD>\n"
#define CUNIT_RUN_TEST_FAILURE_ASSERT_RANGE_LLD \
    "        <CUNIT_RUN_TEST_RECORD>\n" \
    "          <CUNIT_RUN_TEST_FAILURE>\n" \
    "            <TEST_NAME> %s </TEST_NAME>\n" \
    "            <FILE_NAME> %s </FILE_NAME>\n" \
    "            <LINE_NUMBER> %d </LINE_NUMBER>\n" \
    "            <CONDITION> %s(value=%lld, min=%lld, max=%lld) </CONDITION>\n" \
    "          </CUNIT_RUN_TEST_FAILURE>\n" \
    "        </CUNIT_RUN_TEST_RECORD>\n"
#define CUNIT_RUN_TEST_FAILURE_ASSERT_SET_LLD \
    "        <CUNIT_RUN_TEST_RECORD>\n" \
    "          <CUNIT_RUN_TEST_FAILURE>\n" \
    "            <TEST_NAME> %s </TEST_NAME>\n" \
    "            <FILE_NAME> %s </FILE_NAME>\n" \
    "            <LINE_NUMBER> %d </LINE_NUMBER>\n" \
    "            <CONDITION> %s(value=%lld, number_of_values=%lld) </CONDITION>\n" \
    "          </CUNIT_RUN_TEST_FAILURE>\n" \
    "        </CUNIT_RUN_TEST_RECORD>\n"
#define CUNIT_RUN_TEST_ERROR \
    "        <CUNIT_RUN_TEST_RECORD>\n" \
    "          <CUNIT_RUN_TEST_ERROR>\n" \
    "            <FILE_NAME> %s </FILE_NAME>\n" \
    "            <LINE_NUMBER> %d </LINE_NUMBER>\n"
#define CUNIT_RUN_SUMMARY \
    "      </CUNIT_RUN_SUITE_SUCCESS>\n" \
    "    </CUNIT_RUN_SUITE>\n" \
    "  </CUNIT_RESULT_LISTING>\n" \
    "  <CUNIT_RUN_SUMMARY>\n" \
    "    <CUNIT_RUN_SUMMARY_RECORD>\n" \
    "      <TYPE> %s </TYPE>\n" \
    "      <TOTAL> %d </TOTAL>\n" \
    "      <RUN> %d </RUN>\n" \
    "      <SUCCEEDED> %d </SUCCEEDED>\n" \
    "      <FAILED> %d </FAILED>\n" \
    "      <INACTIVE> %d </INACTIVE>\n" \
    "    </CUNIT_RUN_SUMMARY_RECORD>\n" \
    "    <CUNIT_RUN_SUMMARY_RECORD>\n" \
    "      <TYPE> %s </TYPE>\n" \
    "      <TOTAL> %d </TOTAL>\n" \
    "      <RUN> %d </RUN>\n" \
    "      <SUCCEEDED> %d </SUCCEEDED>\n" \
    "      <FAILED> %d </FAILED>\n" \
    "      <INACTIVE> %d </INACTIVE>\n" \
    "    </CUNIT_RUN_SUMMARY_RECORD>\n" \
    "    <CUNIT_RUN_SUMMARY_RECORD>\n" \
    "      <TYPE> %s </TYPE>\n" \
    "      <TOTAL> %d </TOTAL>\n" \
    "      <RUN> %d </RUN>\n" \
    "      <SUCCEEDED> %d </SUCCEEDED>\n" \
    "      <FAILED> %d </FAILED>\n" \
    "      <INACTIVE> %d </INACTIVE>\n" \
    "    </CUNIT_RUN_SUMMARY_RECORD>\n" \
    "  </CUNIT_RUN_SUMMARY>\n" \
    "  <CUNIT_FOOTER> File Generated By CUnit v2.1-2 - %s\n" \
    "  </CUNIT_FOOTER>\n" \
    "</CUNIT_TEST_RUN_REPORT>\n"

// Printf formatting for xml XS Schema
#define XS_INIT_TESTSUITE \
    "<\?xml version=\"1.0\" encoding=\"UTF-8\"\?>\n" \
    "<testsuite name=\"%s\"\n" \
    "           timestamp=\"%s\"\n" \
    "           hostname=\"%s\"\n" \
    "           tests=\"%d\"\n" \
    "           failures=\"%d\"\n" \
    "           errors=\"%d\"\n" \
    "           skipped=\"%d\"\n" \
    "           time=\"%d\">\n"
#define XS_TESTCASE \
    "    <testcase name=\"%s\"\n" \
    "              classname=\"%s\"\n" \
    "              time=\"%s\">\n"
#define XS_RUN_TEST_FAILURE_ASSERT \
    "        <failure type=\"%s(%lld)\"\n" \
    "                 message=\"%s: Test failed.\">\n" \
    "        </failure>\n"
#define XS_RUN_TEST_FAILURE_ASSERT_EQUALITY_LLD \
    "        <failure type=\"%s(%lld, %lld)\"\n" \
    "                 message=\"%s: Test failed.\">\n" \
    "        </failure>\n"
#define XS_RUN_TEST_FAILURE_ASSERT_EQUALITY_STRING \
    "        <failure type=\"%s(%s %s)\"\n" \
    "                 message=\"%s: Test failed.\">\n" \
    "        </failure>\n"
#define XS_RUN_TEST_FAILURE_ASSERT_RANGE_LLD \
    "        <failure type=\"%s(value=%lld, min=%lld, max=%lld)\"\n" \
    "                 message=\"%s: Test failed.\">\n" \
    "        </failure>\n"
#define XS_RUN_TEST_FAILURE_ASSERT_SET_LLD \
    "        <failure type=\"%s(value=%lld, number_of_values=%lld)\"\n" \
    "                 message=\"%s: Test failed.\">\n" \
    "        </failure>\n"
#define XS_TESTCASE_END \
    "    </testcase>\n"
#define XS_TESTSUITE_END \
    "</testsuite>\n"

#endif
