/* expect error: two compatible types */
int x = _Generic(1, int: 1, signed int: 2);
