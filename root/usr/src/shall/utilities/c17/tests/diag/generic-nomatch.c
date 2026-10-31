/* expect error: not compatible with any association */
int x = _Generic(1.0, int: 1, long: 2);
