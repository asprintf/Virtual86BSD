/*[nonstandard]*/
/* Test how [[=]=]x] is parsed. */

#include "suite.h"

int main(void)
{
	// POSIX 9.3.5 1. "The <right-square-bracket> (']') shall lose its special
	// meaning and represent itself in a bracket expression if it occurs first
	// in the list (after an initial <circumflex> ('!'), if any). Otherwise, it
	// shall terminate the bracket expression, unless it appears in a collating
	// symbol (such as "[.].]") or is the ending <right-square-bracket> for a
	// collating symbol, equivalence class, or character class."
	//
	// 9.5.2 RE and Bracket Expression Grammar "
	// collating_symbol : Open_dot COLL_ELEM_SINGLE Dot_close
	//                | Open_dot COLL_ELEM_MULTI Dot_close
	//                | Open_dot META_CHAR Dot_close
	//                ;
	// equivalence_class : Open_equal COLL_ELEM_SINGLE Equal_close
	//                | Open_equal COLL_ELEM_MULTI Equal_close
	//                ;
	// "
	//
	// There is no doubt that [[.].] is valid, but a strict reading of POSIX
	// does not allow [[=]=]] since ] is a META_CHAR. The grammar takes
	// precedence over the basic text, which also does not allow this syntax.
	// However, some systems do allow a ] in an equivalence class. Personally,
	// I prefer allowing this syntax because it allows unifying the parsing of
	// the [. [= and [: sequences inside bracket expressions. This test checks
	// whether that syntax is allowed as an extension.
	reject("[[=]=]x]", "");
	reject("[[=]=]x]", ".");
	match("[[=]=]x]", "x");
	reject("[[=]=]x]", "[");
	match("[[=]=]x]", "]");
	reject("[[=]=]x]", "y");

	return 0;
}
