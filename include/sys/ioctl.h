#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H
/* Not in POSIX: ioctl() and the requests of Linux most often used. */
#include <bits/features.h>
#define __NEED_struct_winsize
#include <bits/alltypes.h>

#define TCGETS     0x5401
#define TCSETS     0x5402
#define TIOCSCTTY  0x540E
#define TIOCGPGRP  0x540F
#define TIOCSPGRP  0x5410
#define TIOCOUTQ   0x5411
#define TIOCGWINSZ 0x5413
#define TIOCSWINSZ 0x5414
#define FIONREAD   0x541B
#define TIOCINQ    FIONREAD
#define TIOCNOTTY  0x5422
#define TIOCGSID   0x5429
#define FIONBIO    0x5421
#define FIONCLEX   0x5450
#define FIOCLEX    0x5451
#define FIOASYNC   0x5452
#define SIOCATMARK 0x8905
#define SIOCGIFNAME  0x8910
#define SIOCGIFCONF  0x8912
#define SIOCGIFFLAGS 0x8913
#define SIOCGIFINDEX 0x8933

__BEGIN_DECLS
int ioctl(int, unsigned long, ...);
__END_DECLS

#endif
