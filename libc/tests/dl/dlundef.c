/* libdlundef.so: refers to a function nothing defines. */
int no_such_function_anywhere(int);
int dlundef(int x) { return no_such_function_anywhere(x); }
