/* Internal: compiled regular expressions.
 *
 * regcomp parses the pattern into a tree, expands bounded repetitions
 * (x{2,4} becomes x x (x (x)?)?, each copy a distinct subtree) and
 * compiles the tree into a Thompson program in which every subtree owns
 * a contiguous fragment [entry, end) whose exits all lead to end.
 *
 * regexec, for patterns without back-references:
 *   1. one pass over the program, all start positions at once, finds the
 *      leftmost match and its longest end (or the set of ends, when the
 *      pattern has minimal repetitions);
 *   2. if subexpressions are wanted, the tree is walked top-down: in a
 *      concatenation each element in turn takes the longest end (the
 *      shortest, for a minimal repetition) that still lets the rest reach
 *      the end already fixed; repetitions likewise, iteration by
 *      iteration. Candidate ends come from forward simulation of the
 *      element's fragment, feasibility from backward simulation of the
 *      rest's fragment. This is the POSIX rule: the whole match leftmost-
 *      longest, then each subpattern from left to right longest.
 * With back-references, a backtracking matcher on the tree tries ends in
 * the same order of preference. In the program a back-reference is
 * compiled as "any string", so the simulations still prune its search,
 * and subtrees without back-references are matched as above.
 */
#ifndef _INTERNAL_REGEX_H
#define _INTERNAL_REGEX_H
#include <regex.h>
#include <stddef.h>

enum { N_CHAR, N_SET, N_BOL, N_EOL, N_BREF, N_EMPTY, N_GROUP, N_CAT, N_ALT, N_STAR, N_OPT, N_REP };

struct re_node {
	unsigned char type;
	unsigned char minimal;      /* prefer the shortest extent */
	unsigned char rep;          /* body of a repetition: reset its groups per iteration */
	unsigned char hasbref;      /* a back-reference inside */
	unsigned char hasref;       /* a group inside that a back-reference refers to */
	int val;                    /* char, set index, group or back-reference number */
	int min, max;               /* N_REP; max -1 = unbounded */
	int glo, ghi;               /* groups contained: [glo, ghi) */
	int minlen, maxlen;         /* maxlen -1 = unbounded */
	int entry, end;             /* program fragment */
	int nkid;
	struct re_node **kid;
};

enum { I_CHAR, I_SET, I_BOL, I_EOL, I_SPLIT, I_JMP, I_MATCH };

/* the kinds of N_BOL/I_BOL (val, x): ^, and the GNU extensions \< \> \b \B \`;
 * of N_EOL/I_EOL: $ and \' */
enum { A_LINE, A_WORDSTART, A_WORDEND, A_BOUND, A_NOTBOUND, A_BUFSTART };
#define A_BUFEND 1

struct re_inst {
	unsigned char op;
	int x, y;                   /* char / set index / targets */
};

struct re_prog {
	int flags;                  /* cflags */
	size_t nsub;
	int bref;                   /* pattern has back-references */
	unsigned refd;              /* bit n: \\n is used */
	int minimal;                /* pattern has minimal repetitions */
	int anchored;               /* begins with ^ and no REG_NEWLINE */
	int u8;                     /* compiled for UTF-8 */
	const struct __lcat *mbcs;  /* a multibyte codeset (EUC-JP ...): pattern and
	                             * subjects are converted to UTF-8 */
	struct re_node *root;
	int ninst;
	struct re_inst *inst;
	int nset;
	unsigned char (*sets)[32];
	int *epred, *epredx;        /* epsilon predecessors (CSR) */
	void *arena;                /* chain of node blocks */
};

struct __lcat;
/* s[0..n) of codeset ct converted to UTF-8 (malloc'd, NUL-terminated;
 * bytes that are no character copied as they are); with off, a malloc'd
 * map: (*off)[i] is the offset in s of byte i of the result, (*off)[len]
 * is n.  Null if out of memory. */
char *__re_to_u8(const char *s, size_t n, const struct __lcat *ct, long **off, size_t *len);
#endif
