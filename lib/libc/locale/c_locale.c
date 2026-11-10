/* The C (POSIX) locale, built in; and the character data of C.UTF-8. */
#include <limits.h>
#include "locale_impl.h"

static const char cmax = CHAR_MAX;

static const char *const c_ctype[] = {
	[14] = "ANSI_X3.4-1968",
};

static const char *const c_numeric[] = {
	".", "", "", 0, 0, "ANSI_X3.4-1968",
};

#define T(i) [i]
static const char *const c_time[159] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
	"January", "February", "March", "April", "May", "June", "July", "August", "September",
	"October", "November", "December",
	"AM", "PM",
	"%a %b %e %H:%M:%S %Y", "%m/%d/%y", "%H:%M:%S", "%I:%M:%S %p",
	T(108) = "%a %b %e %H:%M:%S %Z %Y",
	T(110) = "ANSI_X3.4-1968",
	T(111) = "January", "February", "March", "April", "May", "June", "July", "August", "September",
	"October", "November", "December",
	T(135) = "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static const char *const c_monetary[] = {
	"", "", "", "", "", "", "",
	&cmax, &cmax, &cmax, &cmax, &cmax, &cmax, &cmax, &cmax,
	"-",
	&cmax, &cmax, &cmax, &cmax, &cmax, &cmax,
};

static const char *const c_messages[] = {
	"^[yY]", "^[nN]", "", "", "ANSI_X3.4-1968",
};

static const char *const none[1];

#define N(a) (sizeof a / sizeof *a)
const struct __lcat __c_cat[6] = {
	{ .cat = LC_CTYPE, .name = "C", .n = N(c_ctype), .elem = c_ctype, .cs = CS_C, .mbmax = 1 },
	{ .cat = LC_NUMERIC, .name = "C", .n = N(c_numeric), .elem = c_numeric },
	{ .cat = LC_TIME, .name = "C", .n = N(c_time), .elem = c_time },
	{ .cat = LC_COLLATE, .name = "C", .n = 0, .elem = none },
	{ .cat = LC_MONETARY, .name = "C", .n = N(c_monetary), .elem = c_monetary },
	{ .cat = LC_MESSAGES, .name = "C", .n = N(c_messages), .elem = c_messages },
};

static const char *const u_ctype[] = {
	[13] = 0,
	[14] = "UTF-8",
};

const struct __lcat __c_utf8_ctype = {
	.cat = LC_CTYPE, .name = "C.UTF-8", .n = N(u_ctype), .elem = u_ctype, .cs = CS_UTF8, .mbmax = 6,   /* as glibc says */
};

const struct __locale_struct __c_locale_obj = {
	{ &__c_cat[0], &__c_cat[1], &__c_cat[2], &__c_cat[3], &__c_cat[4], &__c_cat[5] }
};

struct __locale_struct __global_locale = {
	{ &__c_cat[0], &__c_cat[1], &__c_cat[2], &__c_cat[3], &__c_cat[4], &__c_cat[5] }
};

const char *__lc_str(const struct __lcat *c, unsigned idx)
{
	const char *s = idx < c->n ? c->elem[idx] : 0;
	return s ? s : "";
}
