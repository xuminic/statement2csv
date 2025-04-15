
#ifndef	_STATEMENT_H_
#define _STATEMENT_H_

#include <time.h>

#ifdef DEBUG
  //#define log(...) printf(__VA_ARGS__)
  #define log(...) fprintf(stderr, "[%s:%d] ", __FILE__, __LINE__), fprintf(stderr, __VA_ARGS__)
#else
  #define log(...) ((void)0)
#endif

/* default definition of states. Local states start from STAT_MORE */
#define STAT_INIT	-1
#define STAT_BEGIN	0
#define STAT_OUTPUT	1
#define STAT_EXIT	2
#define STAT_FILTER	3
#define STAT_MORE	10

#define	TRANSGAP	"  "
#define	TIMESEP		"/ -"


typedef	struct	{
	struct	tm	tm_acct;	/* transaction recorded */
	struct	tm	tm_trans;	/* transaction acted */

	char	transaction[512];
	char	acc_no[128];
	char	debit[32];
	char	credit[32];
	char	balance[32];
} Transaction;

typedef int (*smachine_t)(int, char *, Transaction *);
typedef smachine_t (*smlookup_t)(FILE *);


#define StrNCmp(s,m)	strncmp((s), (m), strlen(m))
#define StrCACmp(s,m)	strncasecmp((s), (m), strlen(m))
#define StrNCpy(s,m,n)	(s)[(n)-1] = 0, strncpy((s),(m),(n)-1)
#define BlankLine(s)	(!*(bare(s)))


/* main.c */
char *take_time(char *s, struct tm *date);
char *take_amount(char *s, char *buf, int blen);
void transaction_reset(Transaction *bsm);
void print_transaction(Transaction *bsm);
void print_csv_title(Transaction *bsm, char *info);
int format_time(struct tm *date, char *buf, int blen);
int strrcmp(char *s, char *t);
char *bare(char *s);
char *strip(char *s, char *set);
char *skip(char *s);
char *hit(char *s, char *set);
char *chop(char *s);
char *safecat(char *dst, char *src, int dlen);
char *morphcat(char *dst, char *src, int dlen);

/* cba.c */
smachine_t cba_main(FILE *fin);

/* nab.c */
smachine_t nab_main(FILE *fin);

#endif	/* _STATEMENT_H_ */

