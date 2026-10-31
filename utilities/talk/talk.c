/*
 * talk.c - talk: talk to another user (POSIX.1-2024, XCU talk)
 *
 * Synopsis: talk address [terminal]
 *
 * Talk between the users of this system, without a daemon.  The caller
 * makes a socket (AF_UNIX) named talk.CALLER.CALLEE in the directory of
 * meetings (/tmp), and rings the callee: on a terminal of the callee that
 * allows messages (as write chooses one, utx.c) it writes
 *     \r\n\aMessage from talk@NODE at HH:MM ...\r\n
 *     talk: connection requested by CALLER\r\n
 *     talk: respond with: talk CALLER\r\n
 * again every 10 seconds until the callee answers.  The callee's talk
 * finds the socket, checks that its owner is the caller, and connects.
 * The caller checks the callee in turn: it sends a random number, the
 * callee makes the file talk.proof.NUMBER in the same directory, and the
 * caller checks that its owner is the callee.  When both run talk for
 * each other at the same time, the one whose name sorts higher answers
 * the other's call.
 *
 * The screen is split: the upper half for what the user types, the lower
 * half for what the other party types, with a line between them for the
 * state of the talk.  Each half is written from its top; past its end
 * writing starts again at its top (each line cleared before it is
 * written).  The screen is driven by terminfo (cup, clear, and el when
 * there is one), as tput does; the size is LINES and COLUMNS, else the
 * window size, else terminfo, else 24 by 80.
 *
 * The terminal is in noncanonical mode without echo, and talk handles the
 * input: the erase and kill characters erase on both screens, so does
 * the word-erase character when iexten is set; <alert> alerts the other
 * terminal; <control>-L redraws the screen; the end-of-file character and
 * the interrupt character (by SIGINT) end talk with status 0.  The other
 * characters of the classes print and space are sent as they are; the
 * other bytes as ^X (^? for DEL), and those with the high bit set and not
 * part of a printable character as M- and the same for the byte less that
 * bit.  When the other party has left, this is shown, and the input is
 * read only for the end-of-file and interrupt characters.
 *
 * Choices where the standard leaves room:
 *  - Diagnostic messages go to the standard output (the standard gives
 *    talk no standard error), also when that is not a terminal; then, and
 *    when the standard input is not a terminal, talk exits with status 1,
 *    as for any error; usage errors exit with status 2.
 *  - address is a user name of this system; other forms are errors.
 *  - Without TERM (or with an empty one) the terminal type is vt100; a
 *    terminal without cursor addressing cannot be used (status 1).
 *  - The talk itself is not internationalized: what is typed is sent in
 *    the sender's codeset.
 *  - POSIX_UTMPX, POSIX_DEVDIR (utx.c) and POSIX_TALKDIR (the directory
 *    of meetings) are hooks for the tests; not when talk is set-user-ID
 *    or set-group-ID.
 */

#define _POSIX_C_SOURCE 202405L
#define _XOPEN_SOURCE 800

#include "terminfo.h"
#include "utx.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>
#include <wchar.h>
#include <wctype.h>

#include "port.h"

enum { RING_EVERY = 10, WIRE_ERASE = 0x08, WIRE_KILL = 0x15, WIRE_WERASE = 0x17 };

static const char *me, *peer;
static uid_t peer_uid;
static const char *dir;
static char mysock[sizeof ((struct sockaddr_un *)0)->sun_path];   /* to unlink */
static bool have_sock;

static struct termios saved;
static bool tty_set, screen_on;
static volatile sig_atomic_t interrupted;

/* ---- messages and leaving ---- */

static void restore(void)
{
    if (screen_on) {
        fflush(stdout);
        screen_on = false;
    }
    if (tty_set) {
        tcsetattr(0, TCSAFLUSH, &saved);
        tty_set = false;
    }
    if (have_sock) {
        unlink(mysock);
        have_sock = false;
    }
}

static void leave(int status);

static void fail(const char *fmt, ...);

static void on_int(int sig)
{
    (void)sig;
    interrupted = 1;
}

/* ---- the screen ---- */

static struct terminfo ti;
static const char *cap_cup, *cap_clear, *cap_el;
static int rows = 24, cols = 80;

struct cell {
    wchar_t wc;                 /* L' ' for the columns after a wide one: 0 */
    int width;
};
struct region {
    int top, n;                 /* first screen row, number of rows */
    struct cell **line;         /* each of cols cells */
    int *len;                   /* cells used in each line */
    int r, c;                   /* the cursor: line, column */
};
static struct region mine, theirs;
static char status_text[256];

static void move(int r, int c)
{
    long p[9] = { r, c };
    char *sp[9] = { 0 };
    char *s = ti_tparm(cap_cup, p, sp);
    if (s) {
        ti_put(s);
        free(s);
    }
}

static void put_wc(wchar_t wc)
{
    char mb[MB_LEN_MAX + 1];
    mbstate_t ps;
    memset(&ps, 0, sizeof ps);
    size_t k = wcrtomb(mb, wc, &ps);
    if (k == (size_t)-1) {
        putchar('?');
        return;
    }
    fwrite(mb, 1, k, stdout);
}

/* clear from column c of screen row r to the end */
static void clear_to_end(int r, int c)
{
    move(r, c);
    if (cap_el) {
        ti_put(cap_el);
    } else {
        for (int i = c; i < cols - (r == rows - 1); i++)
            putchar(' ');
    }
}

static void region_init(struct region *g, int top, int n)
{
    g->top = top;
    g->n = n;
    g->line = calloc((size_t)n, sizeof *g->line);
    g->len = calloc((size_t)n, sizeof *g->len);
    if (!g->line || !g->len)
        fail("out of memory");
    for (int i = 0; i < n; i++)
        if (!(g->line[i] = calloc((size_t)cols, sizeof **g->line)))
            fail("out of memory");
    g->r = g->c = 0;
}

static void region_newline(struct region *g)
{
    g->r = (g->r + 1) % g->n;
    g->len[g->r] = 0;
    g->c = 0;
    clear_to_end(g->top + g->r, 0);
}

static void region_putwc(struct region *g, wchar_t wc)
{
    if (wc == L'\t') {
        int to = (g->c / 8 + 1) * 8;
        if (to > cols)
            to = cols;
        if (g->c >= cols)
            region_newline(g);
        while (g->c < to)
            region_putwc(g, L' ');
        return;
    }
    int w = wcwidth(wc);
    if (w < 0)
        wc = L'?', w = 1;
    if (w == 0)
        return;                 /* combining marks are left out */
    if (g->c + w > cols)
        region_newline(g);
    move(g->top + g->r, g->c);
    put_wc(wc);
    struct cell *l = g->line[g->r];
    l[g->len[g->r]++] = (struct cell){ wc, w };
    g->c += w;
}

static void region_erase(struct region *g)
{
    int n = g->len[g->r];
    if (n == 0)
        return;
    int w = g->line[g->r][n - 1].width;
    g->len[g->r] = n - 1;
    g->c -= w;
    move(g->top + g->r, g->c);
    for (int i = 0; i < w; i++)
        putchar(' ');
}

static void region_kill(struct region *g)
{
    g->len[g->r] = 0;
    g->c = 0;
    clear_to_end(g->top + g->r, 0);
}

static void region_werase(struct region *g)
{
    struct cell *l = g->line[g->r];
    while (g->len[g->r] > 0 && iswspace((wint_t)l[g->len[g->r] - 1].wc))
        region_erase(g);
    while (g->len[g->r] > 0 && !iswspace((wint_t)l[g->len[g->r] - 1].wc))
        region_erase(g);
}

static void region_redraw(struct region *g)
{
    for (int i = 0; i < g->n; i++) {
        move(g->top + i, 0);
        for (int k = 0; k < g->len[i]; k++)
            put_wc(g->line[i][k].wc);
    }
}

static void draw_status(void)
{
    int r = mine.top + mine.n, c = 0;
    move(r, 0);
    for (const char *s = status_text; *s && c < cols - 1; s++, c++)
        putchar(*s);
    for (; c < cols - 1; c++)
        putchar('-');
}

static void place_cursor(void)
{
    move(mine.top + mine.r, mine.c);
    fflush(stdout);
}

static void set_status(const char *s)
{
    snprintf(status_text, sizeof status_text, "[%s]", s);
    if (screen_on) {
        draw_status();
        place_cursor();
    }
}

static void redraw(void)
{
    ti_put(cap_clear);
    region_redraw(&mine);
    region_redraw(&theirs);
    draw_status();
    place_cursor();
}

static void screen_size(void)
{
    const char *e;
    long v;
    int r = 0, c = 0;
    if ((e = getenv("LINES")) && (v = strtol(e, NULL, 10)) > 0 && v < 10000)
        r = (int)v;
    if ((e = getenv("COLUMNS")) && (v = strtol(e, NULL, 10)) > 0 && v < 10000)
        c = (int)v;
    struct winsize w;
    if ((!r || !c) && tcgetwinsize(1, &w) == 0) {
        if (!r && w.ws_row > 0)
            r = w.ws_row;
        if (!c && w.ws_col > 0)
            c = w.ws_col;
    }
    if (!r && ti_find(&ti, "lines", &v, NULL) == TI_NUM && v > 0)
        r = (int)v;
    if (!c && ti_find(&ti, "cols", &v, NULL) == TI_NUM && v > 0)
        c = (int)v;
    rows = r ? r : 24;
    cols = c ? c : 80;
}

static void screen_start(void)
{
    const char *term = getenv("TERM");
    if (!term || !*term)
        term = "vt100";
    if (ti_load(term, &ti) != 0)
        fail("%s: unknown terminal type", term);
    if (ti_find(&ti, "cup", NULL, &cap_cup) != TI_STR || ti_find(&ti, "clear", NULL, &cap_clear) != TI_STR)
        fail("%s: the terminal cannot address the cursor", term);
    if (ti_find(&ti, "el", NULL, &cap_el) != TI_STR)
        cap_el = NULL;
    screen_size();
    if (rows < 3 || cols < 10)
        fail("the screen is too small");
    int half = (rows - 1) / 2;
    region_init(&mine, 0, half);
    region_init(&theirs, half + 1, rows - half - 1);

    struct termios t;
    if (tcgetattr(0, &saved) != 0)
        fail("standard input: %s", strerror(errno));
    t = saved;
    t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSAFLUSH, &t) != 0)
        fail("standard input: %s", strerror(errno));
    tty_set = true;
    screen_on = true;
    redraw();
}

static void leave(int status)
{
    if (screen_on) {
        move(rows - 1, 0);
        putchar('\n');
    }
    restore();
    fflush(stdout);
    exit(status);
}

static void fail(const char *fmt, ...)
{
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    if (screen_on) {
        move(rows - 1, 0);
        putchar('\n');
        screen_on = false;
    }
    restore();
    printf("talk: %s\n", msg);
    fflush(stdout);
    exit(1);
}

/* ---- the meeting ---- */

static bool sock_name(struct sockaddr_un *a, const char *from, const char *to)
{
    memset(a, 0, sizeof *a);
    a->sun_family = AF_UNIX;
    int n = snprintf(a->sun_path, sizeof a->sun_path, "%s/talk.%s.%s", dir, from, to);
    return n > 0 && (size_t)n < sizeof a->sun_path;
}

/* connect to the call of from (owned by uid) to to: a descriptor, or -1 */
static int answer_call(const char *from, uid_t uid, const char *to)
{
    struct sockaddr_un a;
    struct stat st;
    if (!sock_name(&a, from, to) || lstat(a.sun_path, &st) != 0 || !S_ISSOCK(st.st_mode) || st.st_uid != uid)
        return -1;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    if (connect(fd, (struct sockaddr *)&a, sizeof a) != 0) {
        close(fd);
        return -1;
    }
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    return fd;
}

/* read a line of at most n - 1 bytes within the time limit (seconds) */
static bool read_line(int fd, char *buf, size_t n, int secs)
{
    size_t k = 0;
    time_t end = time(NULL) + secs;
    while (k + 1 < n) {
        struct pollfd p = { fd, POLLIN, 0 };
        int left = (int)(end - time(NULL));
        if (left < 0 || poll(&p, 1, left * 1000) <= 0)
            return false;
        ssize_t r = read(fd, buf + k, 1);
        if (r <= 0)
            return false;
        if (buf[k] == '\n') {
            buf[k] = '\0';
            return true;
        }
        k++;
    }
    return false;
}

static bool send_all(int fd, const char *s, size_t n)
{
    while (n > 0) {
        ssize_t k = write(fd, s, n);
        if (k < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        s += k;
        n -= (size_t)k;
    }
    return true;
}

/* the callee's side of the check */
static bool prove(int fd)
{
    char line[128], proof[PATH_MAX];
    if (!read_line(fd, line, sizeof line, 10) || strncmp(line, "talk 1 ", 7) != 0
        || strspn(line + 7, "0123456789abcdef") != strlen(line + 7) || !line[7])
        return false;
    snprintf(proof, sizeof proof, "%s/talk.proof.%s", dir, line + 7);
    int pf = open(proof, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (pf < 0)
        return false;
    close(pf);
    bool ok = send_all(fd, "ok\n", 3) && read_line(fd, line, sizeof line, 10) && strcmp(line, "yes") == 0;
    unlink(proof);
    return ok;
}

static void random_hex(char *out, size_t n)
{
    unsigned char b[16];
    int rf = open("/dev/urandom", O_RDONLY);
    ssize_t got = rf >= 0 ? read(rf, b, sizeof b) : -1;
    if (rf >= 0)
        close(rf);
    if (got != (ssize_t)sizeof b) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        srand((unsigned)ts.tv_nsec ^ (unsigned)getpid() ^ (unsigned)ts.tv_sec);
        for (size_t i = 0; i < sizeof b; i++)
            b[i] = (unsigned char)(rand() >> 4);
    }
    size_t k = 0;
    for (size_t i = 0; i < sizeof b && k + 2 < n; i++)
        k += (size_t)snprintf(out + k, n - k, "%02x", b[i]);
}

/* the caller's side of the check */
static bool check_callee(int fd)
{
    char num[40], line[128], proof[PATH_MAX];
    random_hex(num, sizeof num);
    snprintf(line, sizeof line, "talk 1 %s\n", num);
    if (!send_all(fd, line, strlen(line)) || !read_line(fd, line, sizeof line, 10) || strcmp(line, "ok") != 0)
        return false;
    snprintf(proof, sizeof proof, "%s/talk.proof.%s", dir, num);
    struct stat st;
    bool ok = lstat(proof, &st) == 0 && S_ISREG(st.st_mode) && st.st_uid == peer_uid;
    send_all(fd, ok ? "yes\n" : "no\n", ok ? 4 : 3);
    return ok;
}

/* the callee's terminal: its pathname (malloc'd); fails if there is none */
static char *callee_tty(const char *want)
{
    if (!utx_open(NULL))
        fail("the user accounting database: %s", strerror(errno));
    struct utx u;
    int logged = 0;
    char best[300] = "";
    time_t bestat = 0;
    while (utx_next(&u)) {
        if (u.type != USER_PROCESS || strcmp(u.user, peer) != 0)
            continue;
        if (want && strcmp(u.line, want) != 0)
            continue;
        logged++;
        char *p = utx_ttypath(u.line);
        struct stat st;
        bool ok = p && stat(p, &st) == 0 && ((st.st_mode & (S_IWGRP | S_IWOTH)) || geteuid() == 0);
        free(p);
        if (ok && (!best[0] || st.st_atime > bestat)) {
            snprintf(best, sizeof best, "%s", u.line);
            bestat = st.st_atime;
        }
    }
    utx_close();
    if (logged == 0) {
        if (want)
            fail("%s is not logged on %s", peer, want);
        fail("%s is not logged on", peer);
    }
    if (!best[0])
        fail("%s has messages disabled", peer);
    char *p = utx_ttypath(best);
    if (!p)
        fail("out of memory");
    return p;
}

static void ring(const char *tty)
{
    int fd = open(tty, O_WRONLY | O_NOCTTY | O_APPEND | O_NONBLOCK);
    if (fd < 0)
        fail("%s: %s", tty, strerror(errno));
    struct utsname un;
    char when[16] = "?";
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (tm)
        strftime(when, sizeof when, "%H:%M", tm);
    char msg[2048];
    int n = snprintf(msg, sizeof msg,
                     "\r\n\aMessage from talk@%s at %s ...\r\n"
                     "talk: connection requested by %s\r\n"
                     "talk: respond with: talk %s\r\n",
                     uname(&un) >= 0 ? un.nodename : "localhost", when, me, me);
    bool ok = n > 0 && send_all(fd, msg, (size_t)n);
    int e = errno;
    close(fd);
    if (!ok)
        fail("%s: %s", tty, strerror(e));
}

/* ---- typing ---- */

static int conn = -1;
static bool gone;               /* the other party has left */
static cc_t c_erase, c_kill, c_werase, c_eof;
static bool iexten;

/* what is sent for byte c that is not part of a printable character */
static size_t visible(unsigned char c, char *out)
{
    size_t o = 0;
    if (c >= 0x80) {
        out[o++] = 'M';
        out[o++] = '-';
        c &= 0x7f;
    }
    if (c < 0x20) {
        out[o++] = '^';
        out[o++] = (char)(c + '@');
    } else if (c == 0x7f) {
        out[o++] = '^';
        out[o++] = '?';
    } else {
        out[o++] = (char)c;
    }
    return o;
}

static void send_bytes(const char *s, size_t n)
{
    if (conn >= 0 && !gone && !send_all(conn, s, n)) {
        gone = true;
        set_status("Your party has left; type the end-of-file or interrupt character to exit");
    }
}

/* show the bytes s[0..n) (to be sent) in region g, and send them */
static void show_and_send(struct region *g, const char *s, size_t n, bool send)
{
    mbstate_t ps;
    memset(&ps, 0, sizeof ps);
    for (size_t i = 0; i < n;) {
        wchar_t wc;
        size_t k = mbrtowc(&wc, s + i, n - i, &ps);
        if (k == (size_t)-1 || k == (size_t)-2 || k == 0) {
            memset(&ps, 0, sizeof ps);
            region_putwc(g, L'?');
            i++;
            continue;
        }
        region_putwc(g, wc);
        i += k;
    }
    if (send)
        send_bytes(s, n);
}

static unsigned char pend[MB_LEN_MAX];  /* an incomplete character typed */
static size_t npend;

static void typed_bytes(void);

static void typed(unsigned char c)
{
    if (c == c_eof && c_eof != _POSIX_VDISABLE)
        leave(0);
    if (c == 0x0c) {            /* <control>-L */
        redraw();
        return;
    }
    if (gone || conn < 0)
        return;
    if (npend == 0) {
        if (c == c_erase && c_erase != _POSIX_VDISABLE) {
            region_erase(&mine);
            char w = WIRE_ERASE;
            send_bytes(&w, 1);
            return;
        }
        if (c == c_kill && c_kill != _POSIX_VDISABLE) {
            region_kill(&mine);
            char w = WIRE_KILL;
            send_bytes(&w, 1);
            return;
        }
        if (iexten && c == c_werase && c_werase != _POSIX_VDISABLE) {
            region_werase(&mine);
            char w = WIRE_WERASE;
            send_bytes(&w, 1);
            return;
        }
        if (c == '\n' || c == '\r') {
            region_newline(&mine);
            send_bytes("\n", 1);
            return;
        }
        if (c == '\a') {
            send_bytes("\a", 1);
            return;
        }
    }
    pend[npend++] = c;
    typed_bytes();
}

/* take the complete characters of pend */
static void typed_bytes(void)
{
    while (npend > 0) {
        wchar_t wc;
        mbstate_t ps;
        memset(&ps, 0, sizeof ps);
        size_t k = mbrtowc(&wc, (char *)pend, npend, &ps);
        if (k == (size_t)-2 && npend < MB_LEN_MAX)
            return;             /* wait for the rest */
        if (k != (size_t)-1 && k != (size_t)-2 && k != 0
            && (iswprint((wint_t)wc) || iswspace((wint_t)wc)) && wc != L'\n' && wc != L'\r'
            && wc != L'\v' && wc != L'\f') {
            show_and_send(&mine, (char *)pend, k, true);
        } else {
            k = 1;
            char out[8];
            size_t o = visible(pend[0], out);
            show_and_send(&mine, out, o, true);
        }
        npend -= k;
        memmove(pend, pend + k, npend);
    }
}

/* ---- what the other party types ---- */

static char rbuf[MB_LEN_MAX];
static size_t nrbuf;

static void received(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (nrbuf == 0 && c < 0x20) {
            switch (c) {
            case WIRE_ERASE: region_erase(&theirs); break;
            case WIRE_KILL: region_kill(&theirs); break;
            case WIRE_WERASE: region_werase(&theirs); break;
            case '\n': region_newline(&theirs); break;
            case '\a': putchar('\a'); break;
            case '\t': region_putwc(&theirs, L'\t'); break;
            default: region_putwc(&theirs, L'?'); break;
            }
            continue;
        }
        rbuf[nrbuf++] = (char)c;
        for (;;) {
            wchar_t wc;
            mbstate_t ps;
            memset(&ps, 0, sizeof ps);
            size_t k = mbrtowc(&wc, rbuf, nrbuf, &ps);
            if (k == (size_t)-2 && nrbuf < MB_LEN_MAX)
                break;
            if (k == (size_t)-1 || k == (size_t)-2 || k == 0 || !(iswprint((wint_t)wc) || wc == L' ')) {
                region_putwc(&theirs, L'?');
                k = 1;
            } else {
                region_putwc(&theirs, wc);
            }
            nrbuf -= k;
            memmove(rbuf, rbuf + k, nrbuf);
            if (nrbuf == 0)
                break;
        }
    }
    place_cursor();
}

/* ---- the main loop ---- */

static int listener = -1;
static char *ring_tty;

static void connected(int fd)
{
    if (listener >= 0) {
        close(listener);
        listener = -1;
    }
    if (have_sock) {
        unlink(mysock);
        have_sock = false;
    }
    conn = fd;
    set_status("Connection established");
    putchar('\a');
    place_cursor();
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    int a = 1;
    if (a < argc && strcmp(argv[a], "--") == 0)
        a++;
    if (argc - a < 1 || argc - a > 2 || (argv[a][0] == '-' && argv[a][1] != '\0')) {
        printf("usage: talk address [terminal]\n");
        return 2;
    }
    peer = argv[a];
    const char *want = argc - a == 2 ? argv[a + 1] : NULL;
    if (want) {
        if (strncmp(want, "/dev/", 5) == 0)
            want += 5;
        else
            want = utx_linename(want);
    }
    if (!isatty(0))
        fail("the standard input is not a terminal");
    if (!isatty(1))
        fail("the standard output is not a terminal");
    if (strchr(peer, '/') || strchr(peer, '@') || strchr(peer, '!') || strchr(peer, ':'))
        fail("%s: only users of this system can be talked to", peer);
    struct passwd *pw = getpwnam(peer);
    if (!pw)
        fail("%s: no such user", peer);
    peer_uid = pw->pw_uid;
    me = getlogin();
    if (!me || !*me || !(pw = getpwnam(me)) || pw->pw_uid != getuid()) {
        pw = getpwuid(getuid());
        if (!pw)
            fail("who you are is not known");
        me = pw->pw_name;
    }
    me = strdup(me);
    peer = strdup(peer);
    if (!me || !peer)
        fail("out of memory");
    dir = utx_hook("POSIX_TALKDIR");
    if (!dir)
        dir = "/tmp";

    struct termios t;
    if (tcgetattr(0, &t) != 0)
        fail("standard input: %s", strerror(errno));
    c_erase = t.c_cc[VERASE];
    c_kill = t.c_cc[VKILL];
    c_werase = t.c_cc[VWERASE];
    c_eof = t.c_cc[VEOF];
    iexten = (t.c_lflag & IEXTEN) != 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_int;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    /* answer a call, or call */
    struct sockaddr_un addr;
    if (!sock_name(&addr, me, peer) || !sock_name(&addr, peer, me))
        fail("the names are too long for %s", dir);
    status_text[0] = '\0';
    int fd = answer_call(peer, peer_uid, me);
    if (fd >= 0 && !prove(fd)) {
        close(fd);
        fd = -1;
    }
    if (fd < 0) {
        ring_tty = callee_tty(want);
        sock_name(&addr, me, peer);
        struct stat st;
        if (lstat(addr.sun_path, &st) == 0) {
            if (st.st_uid != getuid() || !S_ISSOCK(st.st_mode))
                fail("%s: in the way", addr.sun_path);
            unlink(addr.sun_path);
        }
        listener = socket(AF_UNIX, SOCK_STREAM, 0);
        if (listener < 0)
            fail("socket: %s", strerror(errno));
        fcntl(listener, F_SETFD, FD_CLOEXEC);
        if (bind(listener, (struct sockaddr *)&addr, sizeof addr) != 0)
            fail("%s: %s", addr.sun_path, strerror(errno));
        memcpy(mysock, addr.sun_path, sizeof mysock);
        have_sock = true;
        if (chmod(mysock, 0666) != 0 || listen(listener, 4) != 0)
            fail("%s: %s", mysock, strerror(errno));
    }
    screen_start();
    if (fd >= 0) {
        connected(fd);
    } else {
        ring(ring_tty);
        set_status("Waiting for your party to respond");
    }
    time_t rung = time(NULL);

    for (;;) {
        if (interrupted)
            leave(0);
        struct pollfd p[2];
        int np = 0;
        p[np++] = (struct pollfd){ 0, POLLIN, 0 };
        int other = conn >= 0 && !gone ? conn : listener;
        if (other >= 0)
            p[np++] = (struct pollfd){ other, POLLIN, 0 };
        int wait = conn < 0 ? 1000 : -1;
        int r = poll(p, (nfds_t)np, wait);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            fail("poll: %s", strerror(errno));
        }
        if (conn < 0) {
            /* both called at once: the higher name answers */
            if (strcmp(me, peer) > 0 && (fd = answer_call(peer, peer_uid, me)) >= 0) {
                if (prove(fd))
                    connected(fd);
                else
                    close(fd);
            }
            if (conn < 0 && time(NULL) - rung >= RING_EVERY) {
                ring(ring_tty);
                set_status("Ringing your party again");
                rung = time(NULL);
            }
        }
        if (np > 1 && (p[1].revents & (POLLIN | POLLHUP | POLLERR))) {
            if (conn < 0 && listener >= 0) {
                int c = accept(listener, NULL, NULL);
                if (c >= 0) {
                    fcntl(c, F_SETFD, FD_CLOEXEC);
                    if (check_callee(c))
                        connected(c);
                    else
                        close(c);
                }
            } else if (conn >= 0) {
                char buf[4096];
                ssize_t n = read(conn, buf, sizeof buf);
                if (n < 0 && errno == EINTR)
                    continue;
                if (n <= 0) {
                    gone = true;
                    set_status("Your party has left; type the end-of-file or interrupt character to exit");
                } else {
                    received(buf, (size_t)n);
                }
            }
        }
        if (p[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            unsigned char buf[256];
            ssize_t n = read(0, buf, sizeof buf);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                leave(0);
            for (ssize_t i = 0; i < n; i++)
                typed(buf[i]);
            place_cursor();
        }
    }
}
