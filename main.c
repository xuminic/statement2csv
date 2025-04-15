
/* usage:
 * for i in CBA*.pdf; do pdftotext -nodiag -nopgbrk -layout $i - | statement2csv -n >> cba.csv; done
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "statement.h"


static	FILE	*g_fout;

static	smlookup_t	g_smlist[] = {
	cba_main,
	nab_main,
	NULL
};

static	char	*g_control[] = {
	"DateAcc,DateTra,Transaction,AccNo,Debit,Credit,Balance",
	"DateAcc,Transaction,Debit",
	"DateAcc,Transaction,Credit"
};
static	char	*g_mode;
static	char	g_selfcontrol[256];

static	char	g_date[32] = "Y-M-D";
static	int	g_no_title;

static	char    *statement2csv_help = "\
usage: statement2csv [OPTION] [raw_text]\n\
OPTION:\n\
  -o, --output FILENAME  write to the specified file\n\
  -n, --no-title         do not print title\n\
  -c, --column COLUMN    select columns of the csv table\n\
                         DateAcc,DateTra,Transaction,Debit,Credit,Balance\n\
  -t, --format FORMAT    select the printable time format\n\
                         Y-M-D, D/M/Y, ...\n\
      --help, --version\n\
\n\
Example:\n\
pdftotext -nodiag -nopgbrk -layout S20250331.pdf - | statement2csv\n\
";

static	char    *statement2csv_version = "statement2csv 0.1.0 \
Copyright (C) 2009-2025  \"Andy Xuming\" <xuming@sourceforge.net>\n\
This program comes with ABSOLUTELY NO WARRANTY.\n\
This is free software, and you are welcome to redistribute it under certain\n\
conditions. For details see see `COPYING'.\n";

#define MOREARG(c,v)    {       \
        --(c), ++(v); \
        if (((c) == 0) || (**(v) == '-') || (**(v) == '+')) { \
                fprintf(stderr, "missing parameters\n"); \
                return -1; \
        }\
}

static int transaction(FILE *fin);
static char *cmdl_set_mode(char *s);
static int help_tools(int argc, char **argv);


int main(int argc, char **argv)
{
        FILE    *fin = NULL;

	g_fout = stdout;
	g_mode = g_control[0];
        while (--argc && ((**++argv == '-') || (**argv == '+'))) {
                if (!strcmp(*argv, "-V") || !strcmp(*argv, "--version")) {
                        printf(statement2csv_version);
                        return 0;
                } else if (!strcmp(*argv, "-H") || !strcmp(*argv, "--help")) {
                        puts(statement2csv_help);
                        return 0;
		} else if (!strncmp(*argv, "--help-", 7)) {
                        return help_tools(argc, argv);
		} else if (!strcmp(*argv, "-o") || !strcmp(*argv, "--output")) {
			MOREARG(argc, argv);
			if (argv[0][0] == '-') {
				g_fout = stdout;
			} else if ((g_fout = fopen(*argv, "w")) == NULL) {
                                perror(*argv);
                                g_fout = stdout;
                        }
		} else if (!strcmp(*argv, "-c") || !strcmp(*argv, "--column")) {
			MOREARG(argc, argv);
			g_mode = cmdl_set_mode(*argv);
		} else if (!strcmp(*argv, "-t") || !strcmp(*argv, "--format")) {
			MOREARG(argc, argv);
			StrNCpy(g_date, *argv, sizeof(g_date));
		} else if (!strcmp(*argv, "-n") || !strcmp(*argv, "--no-title")) {
			g_no_title = 1;
		} else if (!strcmp(*argv, "--")) {
			break;
		} else {
			fprintf(stderr, "%s: unknown parameter.\n", *argv);
                        return -1;
		}
	}

	/* input from stdin */
	if (argc == 0) {
		transaction(stdin);
	} else while (argc--) {
		if (!strcmp(*argv, "--")) {
			transaction(stdin);
		} else if ((fin = fopen(*argv, "r")) != NULL) {
			transaction(fin);
			fclose(fin);
		} else {
			perror(*argv);
		}
		argv++;
	}

	if (g_fout != stdout) {
		fclose(g_fout);
	}
	return 0;
}


static int transaction(FILE *fin) {

	Transaction	bsm;
	smachine_t      state_machine = NULL;
	char		*s, buf[2048];
	int		n, state;

	for (state = 0; g_smlist[state]; state++) {
		if ((state_machine = g_smlist[state](fin)) != NULL) {
			break;
		}
	}
	if (state_machine == NULL) {
		fprintf(stderr, "unrecognized file format\n");
		return -1;
	}

	/* initialize the state machine */
	state = state_machine(STAT_INIT, NULL, &bsm);
	while (fgets(buf, sizeof(buf)-1, fin) != NULL) {
		/* preprocess the input to filter out irrelevant strings */
		if ((n = state_machine(STAT_FILTER, buf, NULL)) == STAT_EXIT) {
			break;
		} else if (n != STAT_FILTER) {
			continue;
		}

		s = bare(chop(buf));	/* clean up the head and tail */
		//printf("+++: %s\n", s);
		state = state_machine(state, s, &bsm);
		if (state == STAT_EXIT) {
			break;
		}
	}
	return 0;
}

static char *cmdl_set_mode(char *s) {

	char	*p, *k;
	int	n;

	if (isdigit(*s)) {
		n = (int)strtol(s, NULL, 0);
		if ((n < 0) || (n >= sizeof(g_control)/sizeof(char*))) {
			fprintf(stderr, "Warning: out of format range.\n");
			n = 0;
		}
		return g_control[n];
	}

	StrNCpy(g_selfcontrol, s, sizeof(g_selfcontrol));
	for (k = g_selfcontrol; k && *k; k++) {
		for (p = g_mode; p && *p; p++) {
			if (!strncasecmp(p, k, 5)) {
				break;
			}
			if ((p = strchr(p, ',')) == NULL) {
				fprintf(stderr, "Warning: unkown column %s\n", k);
				return g_control[0];
			}
		}
		if ((k = strchr(k, ',')) == NULL) {
			break;
		}
	}
	return g_selfcontrol;
}

static int help_tools(int argc, char **argv) {
	
	struct	tm	tmbuf;
	int	i;
	char	*p, buf[32];
	char	*time_sample[] = { "31 Mar ABC", "31 Mar 2025", "10/3/25", 
		"10/03/2025abc", NULL };

	memset(&tmbuf, 0, sizeof(tmbuf));
        if (!strcmp(*argv,  "--help-unit")) {
		strcpy(buf, "5/32: ");
		safecat(buf, "0123456789012345678901234567890123456789", sizeof(buf));
		puts(buf);
		strcpy(buf, "CAT: ");
		morphcat(buf, "  s      ,           0        0     0       $3.00          $0.00", sizeof(buf));
		puts(buf);
	} else if (!strcmp(*argv,  "--help-time")) {
		for (i = 0; time_sample[i]; i++) {
			p = take_time(time_sample[i], &tmbuf);
			printf("leftover: %s\n", p);
			format_time(&tmbuf, buf, sizeof(buf));
			printf("time: %s\n", buf);
		}	
	}
	return 0;
}


/* extract the timestamp from a string and convert to tm structure.
 * The timestamp is UK format like "31 Mar 2025" or "31 Mar", where
 * the year is optional. It would be deducted from elsewhere.
 * It will return NULL if no proper timestamp was found.
 * Otherwise it returns the point to the next byte after the timestamp.
 * For example with "31 Mar ABC", it will point to "ABC". 
 * Also except format like 10/3/25 or 10/03/2025
 * */
char *take_time(char *s, struct tm *date) {

	const	char	*cale[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	char	*endp;
	int	n;

	if (!isdigit(*s)) {
		return NULL;
	}

	/* check if the begining of the line is a day number (1-31) */
	n = (int)strtol(s, &endp, 10);
	if (s == endp) {
		return NULL;	/* not a day number */
	}
	if ((n < 1) || (n > 31)) {
		return NULL;	/* not a day number */
	}
	if (date) {
		date->tm_mday = n;
	}

	/* skip whitespaces or skip '/' or skip '-' */
	s = strip(endp, TIMESEP);

	/* check the month field */
	if (isdigit(*s)) {
		n = (int)strtol(s, &endp, 10) - 1;
	} else {
		for (n = 0; n < sizeof(cale)/sizeof(char*); n++) {
			if (!StrCACmp(s, cale[n])) {
				endp = hit(s, TIMESEP);
				break;
			}
		}
	}
	if ((n < 0) || (n > 11)) {
		return NULL;	/* not found month */
	}
	if (date) {
		date->tm_mon = n;
	}

	/* month should've been skipped; the rest part may or may not be the 
	 * year number. If it's not the year number, the address should be
	 * returned for further process */
	s = strip(endp, TIMESEP);
	if (!isdigit(*s)) {
		return endp;
	}

	n = (int)strtol(s, &s, 10);
	if (n < 70) {	/* should be simple form like 25 = 2025 */
		n += 2000;
	} else if (n < 100) {	/* should be simple form like 70 = 1970 */
		n += 1900;
	} else if ((n < 1900) || (n > 3000)) {	/* out of rang of years */
		return endp;	/* where is the end of string of month */
	}
	if (date) {
		date->tm_year = n - 1900;
	}
	return s;
}

/* take an amount number out of a string, remove the non-number parts.
 * the amount number is something like "32.10", "$6,325.00", with no more
 * than one dot '.', can having multiple comma ',' and can begin with
 * spaces and '$'. The take-out will be number only.
 *
 * For example, source string is "$6,325.00 CR Sydney".
 * it will remove the '$' and ',', store "6325.00" to buffer
 * then return pointer to " CR Sydney".
 *
 * Something like "10.191.2.34" will not be recognized as an amount number.
 * "123abc" or "425.00$" will not be recognized as an amount number either
 * because they are lack of space between digit and non-digit.
 * this function will return NULL and not gurantee the contents in 'buf'.
 */
char *take_amount(char *s, char *buf, int blen) {

	int	dot = 0, dig = 0;

	s = strip(s, "$ ");
	for ( ; *s && blen; s++) {
		if (isdigit(*s)) {
			*buf++ = *s; blen--; dig++;
		} else if (*s == '.') {
			*buf++ = *s; blen--; dot++;
		} else if (*s == ',') {
			continue;
		} else if (isspace(*s)) {
			break;		/* received an amount number */
		} else {
			return NULL;	/* not an amount number */
		}
		if (dot > 1) {
			return NULL;	/* more than one dot */
		}
	}
	*buf++ = 0;
	if (dig == 0) {
		return NULL;	/* no digit received */
	}
	return s;
}


/* when we clean the transaction structure, we reserver the year number
 * in case some statements not print the year explicitily */
void transaction_reset(Transaction *bsm) {
	int	year1, year2;

	year1 = bsm->tm_acct.tm_year;
	year2 = bsm->tm_trans.tm_year;
	memset(bsm, 0, sizeof(Transaction));
	bsm->tm_acct.tm_year = year1;
	bsm->tm_trans.tm_year = year2;
	if (bsm->tm_trans.tm_year == 0) {
		bsm->tm_trans.tm_year = year1;
	}
}


/* print the csv title. it can be suppressed */
void print_csv_title(Transaction *bsm, char *info) {
	if (!g_no_title) {
		if (info) {
			fprintf(g_fout, "%s\n", info);
		}
		fprintf(g_fout, "%s\n", g_mode);
	}
}

void print_transaction(Transaction *bsm) {

	char	*p, date[64];
	int	key;

	for (key = 0, p = g_mode; p && *p; p++) {
		if (!StrCACmp(p, "Debit") && isdigit(bsm->debit[0])) key++;
		if (!StrCACmp(p, "Credit") && isdigit(bsm->credit[0])) key++;
		if (!StrCACmp(p, "Balance") && isdigit(bsm->balance[0])) key++;
		if ((p = strchr(p, ',')) == NULL) {
			break;
		}
	}
	if (key == 0) {
		return;		/* no printable data */
	}
	for (p = g_mode; p && *p; p++) {
		if (!StrCACmp(p, "DateAcc")) {	/* for accounting date */
			format_time(&bsm->tm_acct, date, sizeof(date));
			fprintf(g_fout, "%s", date);
		} else if (!StrCACmp(p, "DateTra")) {	/* for transaction date */
			format_time(&bsm->tm_trans, date, sizeof(date));
			fprintf(g_fout, "%s", date);
		} else if (!StrCACmp(p, "Transact")) {
			fprintf(g_fout, "%s", bsm->transaction);
		} else if (!StrCACmp(p, "Debit")) {
			fprintf(g_fout, "%s", bsm->debit);
		} else if (!StrCACmp(p, "Credit")) {
			fprintf(g_fout, "%s", bsm->credit);
		} else if (!StrCACmp(p, "Balance")) {
			fprintf(g_fout, "%s", bsm->balance);
		} else if (!StrCACmp(p, "AccNo")) {
			fprintf(g_fout, "%s", bsm->acc_no);
		}
		if ((p = strchr(p, ',')) == NULL) {
			break;
		}
		fprintf(g_fout, ",");
	}
	fprintf(g_fout, "\n");
}

/* output the timestamp in specified format. */
int format_time(struct tm *date, char *buf, int blen) {
	char	*p, num[16];

	buf[0] = 0;
	if (date->tm_mday == 0) {
		return -1;
	}

	for (p = g_date; *p; p++) {
		switch (*p) {
			case 'Y': sprintf(num, "%d", date->tm_year + 1900); break;
			case 'M': sprintf(num, "%d", date->tm_mon + 1); break;
			case 'D': sprintf(num, "%d", date->tm_mday); break;
			case 'H': sprintf(num, "%d", date->tm_hour); break;
			case 'm': sprintf(num, "%d", date->tm_min); break;
			case 'S': sprintf(num, "%d", date->tm_sec); break;
			default: num[0] = *p, num[1] = 0; break;
		}
		strcat(buf, num);
	}
	return 0;
}

/* compare the part of the string from right side.
 * for exmaple: 
 *  s = "ABCEDFG" matches "DFG"
 *  s = "ABCEDFG" not matches "ABC"
 *  s = "DFG" not matches "ABCEDFG", the candidate longer than the source
 */
int strrcmp(char *s, char *t) {
	char	*p = s + strlen(s) - strlen(t);
	return (p < s) ? -1 : strcmp(p, t);
}


/* remove the whitespaces from the head of the string.
 * for example: "   abc  def  " will be bared to "abc  def  "
 * the whitespace are: space, form-feed ('\f'), new‐line ('\n'), 
 * carriage return ('\r'), horizontal tab ('\t'), and vertical tab ('\v').
 */
char *bare(char *s) {
	while (isspace(*s)) s++;
	return s;
}

/* remove the characters in the "set" from the head of the string.
 * for example, if "set" is "$ /", then " $/  abc  def  " will be stripped
 * to "abc  def  ".
 * NOTE that the space in "set" means not only a space but a whitespace.
 * the whitespace are: space, form-feed ('\f'), new‐line ('\n'), 
 * carriage return ('\r'), horizontal tab ('\t'), and vertical tab ('\v').
 */
#define InSet(s,c)	(strchr((s),(c)) || (strchr((s),' ') && isspace(c)))
char *strip(char *s, char *set) {
	if (!set) return bare(s);
	while (*s && InSet(set, *s)) s++;
	return s;
}


/* on the contrary of bare(), skip() skips anything but whitespaces.
 * for example, "abc  def  " will be skipped to "  def  "
 */
char *skip(char *s) {
	while (*s && !isspace(*s)) s++;
	return s;
}

/* skip the characters untill it is hit by characters in the "set".
 * for example, if "set" is "$ /", then "abc$/def  " will return "/def  ".
 * NOTE that the space in "set" means not only a space but a whitespace.
 * the whitespace are: space, form-feed ('\f'), new‐line ('\n'), 
 * carriage return ('\r'), horizontal tab ('\t'), and vertical tab ('\v').
 */
char *hit(char *s, char *set) {
	if (!set) return skip(s);
	while (*s && !InSet(set, *s)) s++;
	return s;
}

/* unlike bare(), chop() removes whitespaces from the tail of the string.
 * for example, "  abc  def  \n" will be chopped to "  abc  def".
 * head and tail whitespace can be all removed by bare(chop(s))
 */
char *chop(char *s)  {
	char	*tmp = s;
	for (s += strlen(s) - 1; (s > tmp) && isspace(*s); *s-- = 0);
	return tmp;
}


/* strncat() doesn't do as expected like strncpy()
 * so using safecat() instead where need to check buffer overflow */
char *safecat(char *dst, char *src, int dlen) {
	int	n = strlen(dst);

	dlen -= n;
	StrNCpy(dst + n, src, dlen);
	return dst;
}

/* similar to safecat() but it squeeze the whitespace out of the copy.
 * meanwhile it replaces comma "," with underscore "_".
 * multiple whitespace will be combined and replaced by 1 space.
 * for example: this line
 * "s        0           0        0     0       $3.00          $0.00"
 * with be catenated to "s 0 0 0 0 $3.00 $0.00"
 */
char *morphcat(char *dst, char *src, int dlen) {
	char	*p;

	p = dst + strlen(dst);
	dlen -= strlen(dst) + 1;
	while (*src && dlen) {
		if (isspace(*src)) {
			*p++ = ' '; src = bare(src);
		} else if (*src == ',') {
			*p++ = '_'; src++;
		} else {
			*p++ = *src++;
		}
		dlen--;
	}
	*p++ = 0;
	return dst;
}

	

