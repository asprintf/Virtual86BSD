/* Test whether asterisks work. */

#include "suite.h"

int main(void)
{
	match("*", "");
	match("*", "a");
	match("*", "b");
	match("*", "aa");
	match("*", "ab");
	match("*", "aaa");
	match("*", ".");
	match("*", "a.b");
	match("*", "/");
	match("*", "a/b");

	reject("a*", "");
	match("a*", "a");
	reject("a*", "b");
	match("a*", "aa");
	match("a*", "ab");
	reject("a*", "ba");
	reject("a*", "bb");
	match("a*", "aaa");
	match("a*", "aab");
	match("a*", "aba");
	match("a*", "abb");
	reject("a*", "baa");
	reject("a*", "bab");
	reject("a*", "bba");
	reject("a*", "bbb");

	reject("*b", "");
	reject("*b", "a");
	match("*b", "b");
	reject("*b", "aa");
	match("*b", "ab");
	reject("*b", "ba");
	match("*b", "bb");
	reject("*b", "aaa");
	match("*b", "aab");
	reject("*b", "aba");
	match("*b", "abb");
	reject("*b", "baa");
	match("*b", "bab");
	reject("*b", "bba");
	match("*b", "bbb");

	reject("a*b", "");
	reject("a*b", "a");
	reject("a*b", "b");
	reject("a*b", "aa");
	match("a*b", "ab");
	reject("a*b", "ba");
	reject("a*b", "bb");
	reject("a*b", "aaa");
	match("a*b", "aab");
	reject("a*b", "aba");
	match("a*b", "abb");
	reject("a*b", "baa");
	reject("a*b", "bab");
	reject("a*b", "bba");
	reject("a*b", "bbb");

	reject("a*b*a", "");
	reject("a*b*a", "a");
	reject("a*b*a", "b");
	reject("a*b*a", "aa");
	reject("a*b*a", "ab");
	reject("a*b*a", "ba");
	reject("a*b*a", "bb");
	reject("a*b*a", "aaa");
	reject("a*b*a", "aab");
	match("a*b*a", "aba");
	reject("a*b*a", "abb");
	reject("a*b*a", "baa");
	reject("a*b*a", "bab");
	reject("a*b*a", "bba");
	reject("a*b*a", "bbb");
	reject("a*b*a", "aaaa");
	reject("a*b*a", "aaab");
	match("a*b*a", "aaba");
	reject("a*b*a", "aabb");
	match("a*b*a", "abaa");
	reject("a*b*a", "abab");
	match("a*b*a", "abba");
	reject("a*b*a", "abbb");
	reject("a*b*a", "baaa");
	reject("a*b*a", "baab");
	reject("a*b*a", "baba");
	reject("a*b*a", "babb");
	reject("a*b*a", "bbaa");
	reject("a*b*a", "bbab");
	reject("a*b*a", "bbba");
	reject("a*b*a", "bbbb");

	match("aa*bb*aa", "aaaabbaaaa");
	reject("aa*bb*aa", "aaaabbaaab");
	reject("aa*bb*aa", "baaabbaaaa");
	reject("aa*bb*aa", "aaaababaaa");
	match("aa*bb*aa", "aaaababbaa");
	match("aa*bb*aa", "aaaababbaa");
	match("aa*bb*aa", "aaabbabbaa");

	reject("a*aba*a", "a");
	reject("a*aba*a", "aa");
	reject("a*aba*a", "ab");
	reject("a*aba*a", "aba");
	reject("a*aba*a", "aaba");
	reject("a*aba*a", "abaa");
	match("a*aba*a", "aabaa");
	reject("a*aba*a", "aabab");
	reject("a*aba*a", "babaa");
	match("a*aba*a", "aaabaaa");
	match("a*aba*a", "abababa");
	match("a*aba*a", "aabababa");
	match("a*aba*a", "aabababaa");
	match("a*aba*a", "aabababbaa");
	match("a*aba*a", "aabbababaa");
	match("a*aba*a", "aabbababbaa");

	return 0;
}
