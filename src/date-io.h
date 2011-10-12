/*** date-io.h -- helper for formats, parsing, printing, escaping, etc. */
#if !defined INCLUDED_date_io_h_
#define INCLUDED_date_io_h_

#include <stdlib.h>
#include <stdio.h>
#include <stdio_ext.h>
#include "date-core.h"
#include "strops.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect(!!(_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect(!!(_x), 0)
#endif
#if !defined UNUSED
# define UNUSED(x)	__attribute__((unused)) x##_unused
#endif

static struct dt_d_s
dt_io_strpd_ep(const char *str, const char *const *fmt, size_t nfmt, char **ep)
{
	struct dt_d_s res = {DT_UNK};

	/* init */
	if (ep) {
		*ep = NULL;
	}
	/* basic sanity check */
	if (str == NULL || strcmp(str, "now") == 0) {
		res = dt_date(DT_YMD);
	} else if (nfmt == 0) {
		res = dt_strpd(str, NULL, ep);
	} else {
		for (size_t i = 0; i < nfmt; i++) {
			if ((res = dt_strpd(str, fmt[i], ep)).typ > DT_UNK) {
				break;
			}
		}
	}
	return res;
}

static struct dt_d_s __attribute__((unused))
dt_io_strpd(const char *input, char *const *fmt, size_t nfmt)
{
	return dt_io_strpd_ep(input, fmt, nfmt, NULL);
}

static struct dt_d_s  __attribute__((unused))
dt_io_find_strpd(
	const char *str, char *const *fmt, size_t nfmt,
	const char *needle, size_t needlen, char **sp, char **ep)
{
	const char *__sp = str;
	struct dt_d_s d = {DT_UNK};

	if ((d = dt_io_strpd_ep(__sp, fmt, nfmt, ep)).typ == DT_UNK) {
		while ((__sp = strstr(__sp, needle)) &&
		       (d = dt_io_strpd_ep(
				__sp += needlen, fmt, nfmt, ep)).typ == DT_UNK);
	}
	*sp = (char*)__sp;
	return d;
}


/* needles for the grep mode */
typedef struct grep_atom_s *grep_atom_t;
typedef const struct grep_atom_s *const_grep_atom_t;

struct grep_atom_s {
	/* 4 bytes */
	char needle;
	int8_t off_min;
	int8_t off_max;
	uint8_t pad;
	/* 8 bytes */
	const char *fmt;
};

struct grep_atom_soa_s {
	size_t natoms;
	char *needle;
	int8_t *off_min;
	int8_t *off_max;
	const char **fmt;
};

static struct grep_atom_s
calc_grep_atom(const char *fmt)
{
	struct grep_atom_s res = {0};

	/* init */
	if (fmt == NULL) {
		/* standard format, %Y-%m-%d */
		res.needle = '-';
		res.off_min = -4;
		res.off_max = -4;
		goto out;
	}
	/* rest here ... */
	for (const char *fp = fmt; *fp; fp++) {
		if (*fp != '%') {
		literal:
			/* found a non-spec character that can be
			 * used as needle, we should check for the
			 * character's suitability though, a space is not
			 * the best needle to find in a haystack of
			 * english text, in fact it's more like a haystack
			 * itself */
			res.needle = *fp;
			goto out;
		}
		/* otherwise it's a %, read next char */
		switch (*++fp) {
		default:
			break;
		case '%':
			/* very good needle character methinks */
			goto literal;
		case 'n':
			/* quite good needle characters */
			res.needle = '\n';
			goto out;
		case 't':
			res.needle = '\t';
			goto out;
		case 'F':
			res.needle = '-';
			/* fall-through */
		case 'Y':
			res.off_min += -4;
			res.off_max += -4;
			break;
		case 'm':
		case 'd':
		case 'w':
		case 'c':
		case 'C':
		case 'q':
			res.off_min += -2;
			res.off_max += -1;
			break;
		case 'y':
			res.off_min += -2;
			res.off_max += -2;
			break;
		case 'a':
		case 'b':
		case 'h':
			res.off_min += -3;
			res.off_max += -3;
			break;
		case 'j':
			res.off_min += -3;
			res.off_max += -1;
			break;
		case 'A':
			/* Wednesday */
			res.off_min += -9;
			/* Friday */
			res.off_max += -6;
			break;
		case 'B':
			/* September */
			res.off_min += -9;
			/* May */
			res.off_max += -3;
			break;
		case 'Q':
			res.needle = 'Q';
			goto out;
		}
	}
out:
	/* finally assign the format */
	if (res.needle) {
		res.fmt = fmt;
	}
	return res;
}

static struct grep_atom_soa_s __attribute__((unused))
build_needle(grep_atom_t atoms, size_t natoms, char *const *fmt, size_t nfmt)
{
	struct grep_atom_soa_s res = {.natoms = 0};

	res.needle = (char*)atoms;
	res.off_min = (int8_t*)(res.needle + natoms);
	res.off_max = (int8_t*)(res.off_min + natoms);
	res.fmt = (const char**)(res.off_max + natoms);

	if (nfmt == 0) {
		size_t idx = res.natoms++;
		struct grep_atom_s a = calc_grep_atom(NULL);
		res.needle[idx] = a.needle;
		res.off_min[idx] = a.off_min;
		res.off_max[idx] = a.off_max;
		res.fmt[idx] = a.fmt;
		goto out;
	}
	/* otherwise collect needles from all formats */
	for (size_t i = 0; i < nfmt && res.natoms < natoms; i++) {
		size_t idx;
		struct grep_atom_s a;

		if ((a = calc_grep_atom(fmt[i])).needle) {
			idx = res.natoms++;
			res.needle[idx] = a.needle;
			res.off_min[idx] = a.off_min;
			res.off_max[idx] = a.off_max;
			res.fmt[idx] = a.fmt;
		}
	}
out:
	/* terminate needle with \0 */
	res.needle[res.natoms] = '\0';
	return res;
}

static struct dt_d_s  __attribute__((unused))
dt_io_find_strpd2(
	const char *str,
	const struct grep_atom_soa_s *needles,
	char **sp, char **ep)
{
	struct dt_d_s d = {DT_UNK};
	const char *needle = needles->needle;
	const char *const *fmts = needles->fmt;
	int8_t *off_min = needles->off_min;
	int8_t *off_max = needles->off_max;
	const char *p = str;

	for (size_t noff; *(p = xstrpbrkp(p, needle, &noff)); p++) {
		/* check p + min_off .. p + max_off for dates */
		const char *fmt = fmts[noff];

		for (int8_t i = off_min[noff]; i <= off_max[noff]; i++) {
			if ((d = dt_strpd(p + i, fmt, ep)).typ) {
				p += i;
				goto found;
			}
		}
	}
	/* reset to some sane defaults */
	*ep = (char*)(p = str);
found:
	*sp = (char*)p;
	return d;
}

/* formatter */
static inline size_t
dt_io_strfd_autonl(
	char *restrict buf, size_t bsz, const char *fmt, struct dt_d_s that)
{
	size_t res = dt_strfd(buf, bsz, fmt, that);

	if (res > 0 && buf[res - 1] != '\n') {
		/* auto-newline */
		buf[res++] = '\n';
	}
	return res;
}

static void __attribute__((unused))
dt_io_unescape(char *s)
{
	static const char esc_map[] = "\a\bcd\e\fghijklm\nopq\rs\tu\v";
	char *p, *q;

	if (UNLIKELY(s == NULL)) {
		return;
	} else if ((p = q = strchr(s, '\\'))) {
		do {
			if (*p != '\\' || !*++p) {
				*q++ = *p++;
			} else if (*p < 'a' || *p > 'v') {
				*q++ = *p++;
			} else {
				*q++ = esc_map[*p++ - 'a'];
			}
		} while (*p);
		*q = '\0';
	}
	return;
}


#define MAGIC_CHAR	'~'

static void __attribute__((unused))
fixup_argv(int argc, char *argv[], const char *additional)
{
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' &&
		    ((argv[i][1] >= '1' && argv[i][1] <= '9') ||
		     (additional && strchr(additional, argv[i][1])))) {
			/* assume this is meant to be an integer
			 * as opposed to an option that begins with a digit */
			argv[i][0] = MAGIC_CHAR;
		}
	}
	return;
}

static inline char*
unfixup_arg(char *arg)
{
	if (UNLIKELY(arg[0] == MAGIC_CHAR)) {
		arg[0] = '-';
	}
	return arg;
}


static int __attribute__((unused))
dt_io_write(struct dt_d_s d, const char *fmt)
{
	static char buf[64];
	size_t n;

	n = dt_io_strfd_autonl(buf, sizeof(buf), fmt, d);
	fwrite_unlocked(buf, sizeof(*buf), n, stdout);
	return (n > 0) - 1;
}

static int __attribute__((unused))
dt_io_write_sed(
	struct dt_d_s d, const char *fmt,
	const char *line, size_t llen, const char *sp, const char *ep)
{
	static char buf[64];
	size_t n;

	n = dt_strfd(buf, sizeof(buf), fmt, d);
	if (sp) {
		fwrite_unlocked(line, sizeof(char), sp - line, stdout);
	}
	fwrite_unlocked(buf, sizeof(*buf), n, stdout);
	if (ep) {
		fwrite_unlocked(ep, sizeof(char), line + llen - ep, stdout);
	}
	return (n > 0 || sp < ep) - 1;
}


/* error messages, warnings, etc. */
static void __attribute__((unused))
dt_io_warn_strpd(const char *inp)
{
	fprintf(stderr, "\
cannot make sense of `%s' using the given input formats\n", inp);
	return;
}


/* duration parser */
/* we parse durations ourselves so we can cope with the
 * non-commutativity of duration addition:
 * 2000-03-30 +1m -> 2000-04-30 +1d -> 2000-05-01
 * 2000-03-30 +1d -> 2000-03-31 +1m -> 2000-04-30 */
struct __strpdur_st_s {
	int sign;
	const char *istr;
	const char *cont;
	struct dt_dur_s curr;
	size_t ndurs;
	struct dt_dur_s *durs;
};

static inline int
__strpdur_more_p(struct __strpdur_st_s *st)
{
	return st->cont != NULL;
}

static inline void
__strpdur_free(struct __strpdur_st_s *st)
{
	if (st->durs) {
		free(st->durs);
	}
	return;
}

static int __attribute__((unused))
dt_io_strpdur(struct __strpdur_st_s *st, const char *str)
{
/* at the moment we allow only one format */
	const char *sp = NULL;
	const char *ep = NULL;
	int res = 0;

	/* check if we should continue */
	if (st->cont) {
		str = st->istr = st->cont;
	} else if ((st->istr = str)) {
		;
	} else {
		goto out;
	}

	/* read over signs and prefixes */
	sp = str;
	while (1) {
		switch (*sp++) {
		case '\0':
			res = -1;
			ep = sp;
			goto out;
		case '+':
			st->sign = 1;
			break;
		case '-':
			st->sign = -1;
			break;
		case '=':
			if (st->sign > 0) {
				st->sign++;
			} else if (st->sign < 0) {
				st->sign--;
			} else {
				st->sign = 0;
			}
			break;
		case '>':
			st->sign = 2;
			break;
		case '<':
			st->sign = -2;
			break;
		case 'p':
		case 'P':
		case ' ':
		case '\t':
		case '\n':
			continue;
		default:
			sp--;
			break;
		}
		break;
	}

	/* try reading the stuff with our strpdur() */
	if ((st->curr = dt_strpdur(sp, (char**)&ep)).typ > DT_DUR_UNK) {
		if (st->durs == NULL) {
			st->durs = calloc(16, sizeof(*st->durs));
		} else if ((st->ndurs % 16) == 0) {
			st->durs = realloc(
				st->durs,
				(16 + st->ndurs) * sizeof(*st->durs));
			memset(st->durs + st->ndurs, 0, 16 * sizeof(*st->durs));
		}
		if ((st->sign == 1 && dt_dur_neg_p(st->curr)) ||
		    (st->sign == -1 && !dt_dur_neg_p(st->curr))) {
			st->durs[st->ndurs++] = dt_neg_dur(st->curr);
		} else {
			st->durs[st->ndurs++] = st->curr;
		}
	} else {
		res = -1;
	}
out:
	if (((st->cont = ep) && *ep == '\0') || (sp == ep)) {
		st->sign = 0;
		st->cont = NULL;
	}
	return res;
}

#endif	/* INCLUDED_date_io_h_ */