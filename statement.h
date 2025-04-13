
#ifndef	_STATEMENT_H_
#define _STATEMENT_H_

#ifdef DEBUG
  //#define log(...) printf(__VA_ARGS__)
  #define log(...) fprintf(stderr, "[%s:%d] ", __FILE__, __LINE__), fprintf(stderr, __VA_ARGS__)
#else
  #define log(...) ((void)0)
#endif

#define StrNCmp(s,m)	strncmp((s), (m), strlen(m))
#define StrNCpy(s,m,n)	(s)[(n)-1] = 0, strncpy((s),(m),(n)-1)
#define BlankLine(s)	(!*(bare(s)))

typedef	struct	{
	struct	tm	date_acct;	/* transaction recorded */
	struct	tm	date_trans;	/* transaction acted */

	char	transaction[512];
	char	debit[32];
	char	credit[32];
	char	balance[32];
} Transaction;


char *is_date_au(char *s);
char *take_amount(char *s, char *buf, int blen);
void line_out(FILE *fout);
int strrcmp(char *s, char *t);
char *bare(char *s);
char *strip(char *s, char *set);
char *skip(char *s);
char *chop(char *s);


#endif	/* _STATEMENT_H_ */

