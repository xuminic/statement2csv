
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "statement.h"

static	int	g_mode;			/* 0/'0': full; '1': debit; '2': credit; '3': auto */
static	int	g_date = 0;		/* 0: SQLite; 1: Execl */ 

static	char    *statement2csv_help = "\
usage: statement2csv [OPTION] [raw_text]\n\
OPTION:\n\
  -o,  --output FILENAME  write to the specified file\n\
  -fN, --formatN          select contents of the cvs table\n\
                          N=1:debit  2:credit\n\
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

int main(int argc, char **argv)
{
        FILE    *fin = NULL, *fout = stdout;

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
				fout = stdout;
			} else if ((fout = fopen(*argv, "w")) == NULL) {
                                perror(*argv);
                                fout = stdout;
                        }
		} else if (!StrNCmp(*argv, "-f")) {
			g_mode = argv[0][2];
		} else if (!strcmp(*argv, "--format")) {
			g_mode = argv[0][8];
		} else {
			fprintf(stderr, "%s: unknown parameter.\n", *argv);
                        return -1;
		}
	}

	/* input from stdin */
        if ((argc == 0) || !strcmp(*argv, "--")) {
		fin = stdin;
	} else if ((fin = fopen(*argv, "r")) == NULL) {
		perror(*argv);
		fin = stdin;
	}

	cba_transaction(fin, fout);

	if (fout != stdout) {
		fclose(fout);
	}
	if (fin != stdin) {
		fclose(fin);
	}
	return 0;
}



/* date should be either 31 Mar 2025 or 31 Mar */
char *take_time(char *s, struct tm *date) {

	const	char	*cale[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	char	*endp;
	int	n;

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

	/* skip whitespaces */
	s = bare(endp);

	/* check the month field */
	for (n = 0; n < sizeof(cale)/sizeof(char*); n++) {
		if (!StrNCmp(s, cale[n])) {
			if (date) {
				date->tm_mon = n;
			}
			break;
		}
	}
	if (n > 11) {
		return NULL;	/* not found month */
	}

	/* skip the string of month */
	s = skip(s);

	n = (int)strtol(bare(s), &endp, 10);
	if ((n < 1900) || (n > 3000)) {	/* out of rang of years */
		return s;	/* where is the end of string of month */
	}
	if (date) {
		date->tm_year = n - 1900
	}
	return endp;
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

	while (isspace(*s) || (*s == '$')) s++;
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

void transaction_reset(Transaction *bsm) {
	int	year1, year2;

	year1 = bsm->date_acct.tm_year;
	year2 = bsm->date_trans.tm_year;
	memset(bsm, 0, sizeof(Transaction));
	bsm->date_acct.tm_year = year1;
	bsm->date_trans.tm_year = year2;
	if (bsm->date_trans.tm_year == 0) {
		bsm->date_trans.tm_year = year1;
	}
}

void print_transaction(Transaction *bsm) {
	
	char	date1[64], date2[64];

	switch (g_date) {
	case 0:
		if (bsm->date_acct.tm_mday == 0) {
			date1[0] = 0;
		} else {
			sprintf(date1, "%d-%d-%d", 
					bsm->date_acct.tm_year + 1900,
					bsm->date_acct.tm_mon + 1,
					bsm->date_acct.tm_mday);
		}
		if (bsm->date_date_trans.tm_mday == 0) {
			date2[0] = 0;
		} else {
			sprintf(date2, "%d-%d-%d", 
					bsm->date_acct.tm_year + 1900,
					bsm->date_acct.tm_mon + 1,
					bsm->date_acct.tm_mday);
		}
		break;

	case 1:
		if (bsm->date_acct.tm_mday == 0) {
			date1[0] = 0;
		} else {
			sprintf(date1, "%d/%d/%d", bsm->date_acct.tm_mday, 
					bsm->date_acct.tm_mon + 1,
					bsm->date_acct.tm_year + 1900);
		}
		if (bsm->date_date_trans.tm_mday == 0) {
			date2[0] = 0;
		} else {
			sprintf(date2, "%d/%d/%d", bsm->date_acct.tm_mday, 
					bsm->date_acct.tm_mon + 1,
					bsm->date_acct.tm_year + 1900);
		}
		break;
	}

	switch (g_mode) {
	case 0:		/* full mode */
	case '0':
		fprintf(fout, "%s,%s,%s,%s,%s\n", 
			g_date, g_transact, g_debit, g_credit, balance);
		break;
	case '1':		/* output debit */
		if (g_debit[0]) {
			fprintf(fout, "%s,%s,%s\n", 
					g_date, g_transact, g_debit);
		}
		break;
	case '2':
		if (g_credit[0]) {
			fprintf(fout, "%s,%s,%s\n", 
					g_date, g_transact, g_credit);
		}
		break;
	default:
		if (g_debit[0] && !g_credit[0]) {
			fprintf(fout, "%s,%s,%s\n", 
					g_date, g_transact, g_debit);
		} else if (!g_debit[0] && g_credit[0]) {
			fprintf(fout, "%s,%s,%s\n", 
					g_date, g_transact, g_credit);
		} else {
			fprintf(fout, "**%s,%s,%s-%s\n", 
				g_date, g_transact, g_debit, g_credit);
		}
		break;
	}
}

void print_csv_title(Transaction *bsm, char *info)
{
	if (info) {
		fprintf(fout, "%s\n", info);
	}
	if (g_mode) {
		fprintf(fout, "Date,Transaction,Amount\n");
	} else {
		fprintf(fout, "Date,Transaction,Debit,Credit,Balance\n");
	}
}


int strrcmp(char *s, char *t) {
	char	*p = s + strlen(s) - strlen(t);
	return (p < s) ? -1 : strcmp(p, t);
}

char *bare(char *s) {
	while (isspace(*s)) s++;
	return s;
}

char *strip(char *s, char *set) {
	if (!set) return bare(s);
	while (strchr(set, *s) || (strchr(set, ' ') && isspace(*s))) s++;
	return s;
}

char *skip(char *s) {
	while (!isspace(*s)) s++;
	return s;
}

char *chop(char *s)  {
	char	*tmp = s;
	for (s += strlen(s) - 1; (s > tmp) && isspace(*s); *s-- = 0);
	return tmp;
}

#if 0
static int help_tools(int argc, char **argv) {
        //if (!strcmp(*argv,  "--help-strtoms")) {
        //        test_str_to_ms();
	return 0;
}
#endif

