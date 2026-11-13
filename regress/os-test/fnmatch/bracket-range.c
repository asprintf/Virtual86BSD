/* Test whether brackets ranges work. */

#include "suite.h"

int main(void)
{
	match("[a-z]", "a");
	match("[a-z]", "b");
	match("[a-z]", "z");
	reject("[a-z]", "A");
	reject("[a-z]", "1");

	reject("[!a-z]", "a");
	reject("[!a-z]", "b");
	reject("[!a-z]", "z");
	match("[!a-z]", "A");
	match("[!a-z]", "1");

	match("[-az]", "-");
	match("[-az]", "a");
	reject("[-az]", "b");
	match("[-az]", "z");
	reject("[-az]", "A");

	match("[az-]", "-");
	match("[az-]", "a");
	reject("[az-]", "b");
	match("[az-]", "z");
	reject("[az-]", "A");

	match("[#-]]", "#]");
	reject("[#-]]", "#");
	reject("[#-]]", "]");
	match("[#-]]", "-]");

	match("[]#-]]", "#]");
	reject("[[#-]]", "#");
	reject("[]#-]]", "]");
	match("[]#-]]", "]]");

	match("[$]", "$");
	reject("[$]", "#");

	match("[---]", "-");
	reject("[---]", "#");

	match("[]-a]", "]");
	match("[]-a]", "^");
	match("[]-a]", "a");
	reject("[]-a]", "[");
	reject("[]-a]", "-");

	reject("[!]-a]", "]");
	reject("[!]-a]", "^");
	reject("[!]-a]", "a");
	match("[!]-a]", "[");
	match("[!]-a]", "-");

	reject("[]-]]", "");
	reject("[]-]]", "[");
	reject("[]-]]", "-");
	reject("[]-]]", "]");
	reject("[]-]]", "x");
	reject("[]-]]", "[]");
	match("[]-]]", "-]");
	match("[]-]]", "]]");
	reject("[]-]]", "x]");

	reject("[!]-]]", "");
	reject("[!]-]]", "[");
	reject("[!]-]]", "-");
	reject("[!]-]]", "]");
	reject("[!]-]]", "x");
	match("[!]-]]", "[]");
	reject("[!]-]]", "-]");
	reject("[!]-]]", "]]");
	match("[!]-]]", "x]");

	return 0;
}
