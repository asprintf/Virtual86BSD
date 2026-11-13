/* Test whether ERE alternations work combined with brackets. */

#include "suite.h"

int main(void)
{
	cflags = REG_EXTENDED;

	reject("[ab][b][c]|ccb|[b][ab][abc]", "");
	reject("[ab][b][c]|ccb|[ab][abc]", "a");
	reject("[ab][b][c]|ccb|[ab][abc]", "b");
	reject("[ab][b][c]|ccb|[ab][abc]", "c");
	reject("[ab][b][c]|ccb|[b][abc]", "aa");
	reject("[ab][b][c]|ccb|[b][abc]", "ab");
	reject("[ab][b][c]|ccb|[b][abc]", "ac");
	match("[ab][b][c]|ccb|[b][abc]", "ba");
	match("[ab][b][c]|ccb|[b][abc]", "bb");
	match("[ab][b][c]|ccb|[b][abc]", "bc");
	reject("[ab][b][c]|ccb|[b][abc]", "ca");
	reject("[ab][b][c]|ccb|[b][abc]", "cb");
	reject("[ab][b][c]|ccb|[b][abc]", "cc");
	reject("[ab][b][c]|ccb|[b][abc]", "aaa");
	reject("[ab][b][c]|ccb|[b][abc]", "aab");
	reject("[ab][b][c]|ccb|[b][abc]", "aac");
	partial("[ab][b][c]|ccb|[b][abc]", "aba", m(1, 3));
	partial("[ab][b][c]|ccb|[b][abc]", "abb", m(1, 3));
	partial("[ab][b][c]|ccb|[b][abc]", "abc", m(0, 3));
	reject("[ab][b][c]|ccb|[b][abc]", "aca");
	reject("[ab][b][c]|ccb|[b][abc]", "acb");
	reject("[ab][b][c]|ccb|[b][abc]", "acc");
	partial("[ab][b][c]|ccb|[b][abc]", "baa", m(0, 2));
	partial("[ab][b][c]|ccb|[b][abc]", "bab", m(0, 2));
	partial("[ab][b][c]|ccb|[b][abc]", "bac", m(0, 2));
	partial("[ab][b][c]|ccb|[b][abc]", "bba", m(0, 2));
	partial("[ab][b][c]|ccb|[b][abc]", "bbb", m(0, 2));
	partial("[ab][b][c]|ccb|[b][abc]", "bbc", m(0, 3));
	partial("[ab][b][c]|ccb|[b][abc]", "bca", m(0, 2));
	partial("[ab][b][c]|ccb|[b][abc]", "bcb", m(0, 2));
	partial("[ab][b][c]|ccb|[b][abc]", "bcc", m(0, 2));
	reject("[ab][b][c]|ccb|[b][abc]", "caa");
	reject("[ab][b][c]|ccb|[b][abc]", "cab");
	reject("[ab][b][c]|ccb|[b][abc]", "cac");
	partial("[ab][b][c]|ccb|[b][abc]", "cba", m(1, 3));
	partial("[ab][b][c]|ccb|[b][abc]", "cbb", m(1, 3));
	partial("[ab][b][c]|ccb|[b][abc]", "cbc", m(1, 3));
	reject("[ab][b][c]|ccb|[b][abc]", "cca");
	match("[ab][b][c]|ccb|[b][abc]", "ccb");
	reject("[ab][b][c]|ccb|[b][abc]", "ccc");

	return 0;
}
