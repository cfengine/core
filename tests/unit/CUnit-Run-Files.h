
#ifndef CUNIT_RUN_FILES_H_
# define CUNIT_RUN_FILES_H_

#define CUNIT_RUN_DTD \
"<!ELEMENT CUNIT_TEST_RUN_REPORT\n" \
"  (CUNIT_HEADER, CUNIT_RESULT_LISTING, CUNIT_RUN_SUMMARY, CUNIT_FOOTER)>\n" \
"\n" \
"<!ELEMENT CUNIT_HEADER EMPTY>\n" \
"\n" \
"<!ELEMENT CUNIT_RESULT_LISTING (CUNIT_RUN_SUITE*|CUNIT_RUN_GROUP*)>\n" \
"\n" \
"<!ELEMENT CUNIT_RUN_SUITE (CUNIT_RUN_SUITE_SUCCESS|CUNIT_RUN_SUITE_FAILURE)>\n" \
"  <!ELEMENT CUNIT_RUN_SUITE_SUCCESS (SUITE_NAME,CUNIT_RUN_TEST_RECORD*)>\n" \
"  <!ELEMENT CUNIT_RUN_SUITE_FAILURE (SUITE_NAME,FAILURE_REASON)>\n" \
"    <!ELEMENT SUITE_NAME (#PCDATA)>\n" \
"    <!ELEMENT FAILURE_REASON (#PCDATA)>\n" \
"\n" \
"<!ELEMENT CUNIT_RUN_GROUP (CUNIT_RUN_GROUP_SUCCESS|CUNIT_RUN_GROUP_FAILURE)>\n" \
"  <!ELEMENT CUNIT_RUN_GROUP_SUCCESS (GROUP_NAME,CUNIT_RUN_TEST_RECORD*)>\n" \
"  <!ELEMENT CUNIT_RUN_GROUP_FAILURE (GROUP_NAME,FAILURE_REASON)>\n" \
"    <!ELEMENT GROUP_NAME (#PCDATA)>\n" \
"\n" \
"<!ELEMENT CUNIT_RUN_TEST_RECORD (CUNIT_RUN_TEST_SUCCESS|CUNIT_RUN_TEST_FAILURE)>\n" \
"  <!ELEMENT CUNIT_RUN_TEST_SUCCESS (TEST_NAME)>\n" \
"  <!ELEMENT CUNIT_RUN_TEST_FAILURE (TEST_NAME, FILE_NAME, LINE_NUMBER, CONDITION)>\n" \
"    <!ELEMENT TEST_NAME (#PCDATA)>\n" \
"    <!ELEMENT FILE_NAME (#PCDATA)>\n" \
"    <!ELEMENT LINE_NUMBER (#PCDATA)>\n" \
"    <!ELEMENT CONDITION (#PCDATA)>\n" \
"\n" \
"<!ELEMENT CUNIT_RUN_SUMMARY (CUNIT_RUN_SUMMARY_RECORD*)>\n" \
"  <!ELEMENT CUNIT_RUN_SUMMARY_RECORD (TYPE, TOTAL, RUN, SUCCEEDED, FAILED, INACTIVE\?)>\n" \
"    <!ELEMENT TYPE (#PCDATA)>\n" \
"    <!ELEMENT TOTAL (#PCDATA)>\n" \
"    <!ELEMENT RUN (#PCDATA)>\n" \
"    <!ELEMENT SUCCEEDED (#PCDATA)>\n" \
"    <!ELEMENT FAILED (#PCDATA)>\n" \
"    <!ELEMENT INACTIVE (#PCDATA)>\n" \
"\n" \
"<!ELEMENT CUNIT_FOOTER (#PCDATA)>"

#define CUNIT_RUN_XSL \
"<\?xml version='1.0'\?>\n" \
"<xsl:stylesheet\n" \
"	version=\"1.0\"\n" \
"	xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\">\n" \
"\n" \
"	<xsl:template match=\"CUNIT_TEST_RUN_REPORT\">\n" \
"		<html>\n" \
"			<head>\n" \
"				<title> CUnit - Automated Test Run Summary Report </title>\n" \
"			</head>\n" \
"\n" \
"			<body bgcolor=\"#e0e0f0\">\n" \
"				<xsl:apply-templates/>\n" \
"			</body>\n" \
"		</html>\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"CUNIT_HEADER\">\n" \
"		<div align=\"center\">\n" \
"			<h3>\n" \
"				<b> CUnit - A Unit testing framework for C. </b> <br/>\n" \
"				<a href=\"http://cunit.sourceforge.net/\"> http://cunit.sourceforge.net/ </a>\n" \
"			</h3>\n" \
"		</div>\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"CUNIT_RESULT_LISTING\">\n" \
"		<p/>\n" \
"		<div align=\"center\">\n" \
"			<h2> Automated Test Run Results </h2>\n" \
"		</div>\n" \
"		<table cols=\"4\" width=\"90%%\" align=\"center\">\n" \
"			<tr>\n" \
"				<td width=\"25%%\"> </td>\n" \
"				<td width=\"25%%\"> </td>\n" \
"				<td width=\"25%%\"> </td>\n" \
"				<td width=\"25%%\"> </td>\n" \
"			</tr>\n" \
"			<xsl:apply-templates/>\n" \
"		</table>\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"CUNIT_RUN_SUITE\">\n" \
"		<xsl:apply-templates/>\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"SUITE_NAME\">\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"CUNIT_RUN_SUITE_SUCCESS\">\n" \
"		<tr bgcolor=\"#f0e0f0\">\n" \
"			<td colspan=\"4\">\n" \
"				Running Suite <xsl:value-of select=\"SUITE_NAME\"/>\n" \
"			</td>\n" \
"		</tr>\n" \
"		<xsl:apply-templates/>\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"CUNIT_RUN_GROUP\">\n" \
"		<xsl:apply-templates/>\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"CUNIT_RUN_GROUP_SUCCESS\">\n" \
"		<tr bgcolor=\"#f0e0f0\">\n" \
"			<td colspan=\"4\">\n" \
"				Running Group <xsl:apply-templates/>\n" \
"			</td>\n" \
"		</tr>\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"CUNIT_RUN_TEST_RECORD\">\n" \
"		<xsl:apply-templates/>\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"CUNIT_RUN_TEST_SUCCESS\">\n" \
"		<tr bgcolor=\"#e0f0d0\">\n" \
"			<td> </td>\n" \
"			<td colspan=\"2\">\n" \
"				Running test <xsl:apply-templates/>...\n" \
"			</td>\n" \
"			<td bgcolor=\"#50ff50\"> Passed </td>\n" \
"		</tr>\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"CUNIT_RUN_TEST_FAILURE\">\n" \
"		<tr bgcolor=\"#e0f0d0\">\n" \
"			<td> </td>\n" \
"			<td colspan=\"2\">\n" \
"				Running test <xsl:value-of select=\"TEST_NAME\"/>...\n" \
"			</td>\n" \
"			<td bgcolor=\"#ff5050\"> Failed </td>\n" \
"		</tr>\n" \
"\n" \
"		<tr>\n" \
"			<td colspan=\"4\" bgcolor=\"#ff9090\">\n" \
"				<table width=\"100%%\">\n" \
"					<tr>\n" \
"						<th width=\"15%%\"> File Name </th>\n" \
"						<td width=\"50%%\" bgcolor=\"#e0eee0\">\n" \
"							<xsl:value-of select=\"FILE_NAME\"/>\n" \
"						</td>\n" \
"						<th width=\"20%%\"> Line Number </th>\n" \
"						<td width=\"10%%\" bgcolor=\"#e0eee0\">\n" \
"							<xsl:value-of select=\"LINE_NUMBER\"/>\n" \
"						</td>\n" \
"					</tr>\n" \
"					<tr>\n" \
"						<th width=\"15%%\"> Condition </th>\n" \
"						<td colspan=\"3\" width=\"85%%\" bgcolor=\"#e0eee0\">\n" \
"							<xsl:value-of select=\"CONDITION\"/>\n" \
"						</td>\n" \
"					</tr>\n" \
"				</table>\n" \
"			</td>\n" \
"		</tr>\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"CUNIT_RUN_SUITE_FAILURE\">\n" \
"		<tr>\n" \
"			<td colspan=\"3\" bgcolor=\"#f0b0f0\">\n" \
"				Running Suite <xsl:value-of select=\"SUITE_NAME\"/>...\n" \
"			</td>\n" \
"			<td bgcolor=\"#ff7070\">\n" \
"				<xsl:value-of select=\"FAILURE_REASON\"/>\n" \
"			</td>\n" \
"		</tr>\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"CUNIT_RUN_GROUP_FAILURE\">\n" \
"		<tr>\n" \
"			<td colspan=\"3\" bgcolor=\"#f0b0f0\">\n" \
"				Running Group <xsl:value-of select=\"GROUP_NAME\"/>...\n" \
"			</td>\n" \
"			<td bgcolor=\"#ff7070\">\n" \
"				<xsl:value-of select=\"FAILURE_REASON\"/>\n" \
"			</td>\n" \
"		</tr>\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"CUNIT_RUN_SUMMARY\">\n" \
"		<p/>\n" \
"		<table width=\"90%%\" rows=\"5\" align=\"center\">\n" \
"			<tr align=\"center\" bgcolor=\"skyblue\">\n" \
"				<th colspan=\"6\"> Cumulative Summary for Run </th>\n" \
"			</tr>\n" \
"			<tr>\n" \
"				<th width=\"15%%\" bgcolor=\"#ffffc0\" align=\"center\"> Type </th>\n" \
"				<th width=\"17%%\" bgcolor=\"#ffffc0\" align=\"center\"> Total </th>\n" \
"				<th width=\"17%%\" bgcolor=\"#ffffc0\" align=\"center\"> Run </th>\n" \
"				<th width=\"17%%\" bgcolor=\"#ffffc0\" align=\"center\"> Succeeded </th>\n" \
"				<th width=\"17%%\" bgcolor=\"#ffffc0\" align=\"center\"> Failed </th>\n" \
"				<th width=\"17%%\" bgcolor=\"#ffffc0\" align=\"center\"> Inactive </th>\n" \
"			</tr>\n" \
"			<xsl:for-each select=\"CUNIT_RUN_SUMMARY_RECORD\">\n" \
"				<tr align=\"center\" bgcolor=\"lightgreen\">\n" \
"					<td> <xsl:value-of select=\"TYPE\" /> </td>\n" \
"					<td> <xsl:value-of select=\"TOTAL\" /> </td>\n" \
"					<td> <xsl:value-of select=\"RUN\" /> </td>\n" \
"					<td> <xsl:value-of select=\"SUCCEEDED\" /> </td>\n" \
"					<td> <xsl:value-of select=\"FAILED\" /> </td>\n" \
"					<td> <xsl:value-of select=\"INACTIVE\" /> </td>\n" \
"				</tr>\n" \
"			</xsl:for-each>\n" \
"		</table>\n" \
"	</xsl:template>\n" \
"\n" \
"	<xsl:template match=\"CUNIT_FOOTER\">\n" \
"		<p/>\n" \
"		<hr align=\"center\" width=\"90%%\" color=\"maroon\" />\n" \
"		<h5 align=\"center\"> <xsl:apply-templates/> </h5>\n" \
"	</xsl:template>\n" \
"\n" \
"</xsl:stylesheet>"

#endif
