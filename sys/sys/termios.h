/* termios.h - terminal settings and window size (the ABI part). */
#ifndef _SYS_TERMIOS_H_
#define _SYS_TERMIOS_H_

#include <sys/types.h>

#define NCCS_K 19
struct ktermios {
  uint32_t iflag, oflag, cflag, lflag;
  uint8_t line;
  uint8_t cc[NCCS_K];
};

struct winsize {
  uint16_t row, col, xpixel, ypixel;
};

#endif
