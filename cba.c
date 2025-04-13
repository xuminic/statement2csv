
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "statement.h"

#define TRANSGAP	"  "


static int cba_transaction(FILE *fin, FILE *fout);
static int cba_state_machine_layout(int state, char *s, FILE *fout);
static int cba_state_machine_default(int state, char *s, FILE *fout);
`

/* CBA transaction starts like:
 * 14 Jan Transfer from NetBank
 * with multiple short lines */
int cba_transaction(FILE *fin, FILE *fout) {

	char	buf[2048], *s;
	int	state;

	if (g_layout) {
		state = cba_state_machine_layout(-1, NULL, fout);
	} else {
		state = cba_state_machine_default(-1, NULL, fout);
	}
	while (fgets(buf, sizeof(buf)-1, fin) != NULL) {
		/* in layout mode, anything NOT start with ' ' is suspicious */
		if (g_layout && !isspace(buf[0])) {
			continue;
		}

		/* clean up the head and tail */
		s = bare(chop(buf));	/* clean up the head and tail */
		
		//printf("%d<<< %s\n", state, buf);
		if (g_layout) {
			state = cba_state_machine_layout(state, s, fout);
		} else {
			state = cba_state_machine_default(state, s, fout);
		}
	}
	return 0;
}


/* this state machine works for pdftotext layout output:
 * pdftotext -nodiag -nopgbrk -layout S20250331.pdf
 */
static int cba_state_machine_layout(int state, char *s, Transaction *bsm) {

	char	*p;

	switch (state) {
	case -1:	/* initial the state machine */
		g_date[0] = g_transact[0] = 0;
		g_debit[0] = g_credit[0] = balance[0] = 0;
		//return 0;
		return 10;

	case 10:	/* waiting for "THE DRIECTOR" */
		if (StrNCmp(s, "THE DRIECTOR")) {
			break;
		}
		if ((s = strstr(s, "Period")) == NULL) {
			break;		/* make sure second part exists */
		}
		/* pick out date like "1 Jan 2025 - 31 Mar 2025" */
		s = bare(skip(s));	/* skip "Period" and spaces after */
		if (!BlankLine(s) && is_date_au(s)) {
			fprintf(bsm, "Statement Period: %s\n", s);
			if (g_mode) {
				fprintf(bsm, "Date,Transaction,Amount\n");
			} else {
				fprintf(bsm, "Date,Transaction,Debit,Credit,Balance\n");
			}
			return 0;
		}
		break;

	case 0:		/* waiting for Transaction begin */
		if ((s = is_date_au(s)) == NULL) {
			break;
		}

		/* Transaction begin: move to state 1 or 2 */
		log("%d<<< %s\n", state, s);
		//sprintf(g_date, "%d/%d/%d", g_day, g_mon + 1, g_year);
		sprintf(g_date, "%d-%02d-%02d", g_year, g_mon + 1, g_day);
		if (strrcmp(s, " CR")) {	/* waiting for more lines */
			strcat(g_transact, s);
			return 1;
		}
		if ((p = strstr(s, TRANSGAP)) != NULL) {
			*p++ = 0;
			strcat(g_transact, s);
			s = bare(p);
		}
		return cba_state_machine_layout(2, s, bsm);

	case 1:		/* waiting for transaction ends */
		if (is_date_au(s) != NULL) {
			/* another transaction begin */
			return cba_state_machine_layout(5, s, bsm);
		}
		log("%d<<< %s\n", state, s);
		if (strrcmp(s, " CR")) {	/* waiting for more lines */
			if (*s) {
				strcat(g_transact, " ");
				strcat(g_transact, s);
			}
			break;
		}
		if ((p = strstr(s, TRANSGAP)) != NULL) {
			*p++ = 0;
			if (*s) {
				strcat(g_transact, " ");
				strcat(g_transact, s);
			}
			s = bare(p);
		}
		return cba_state_machine_layout(2, s, bsm);

	case 2:		/* collecting transaction data */
		log("%d<<< %s\n", state, s);
		if ((*s == '$') && !strchr(s+1, '$')) {	/* balance only */
			take_amount(s, balance, sizeof(balance));
		} else if (*s == '$') {		/* g_credit and balance */
			take_amount(s, g_credit, sizeof(g_credit));
			if ((s = strrchr(s, '$')) != NULL) {
				take_amount(s, balance, sizeof(balance));
			}
		} else {		/* g_debit and balance */
			take_amount(s, g_debit, sizeof(g_debit));
			if ((s = strrchr(s, '$')) != NULL) {
				take_amount(s, balance, sizeof(balance));
			}
		} 
		return cba_state_machine_layout(5, NULL, bsm);

	case 5:		/* finish one transaction */
		print_transaction(bsm);
		cba_state_machine_layout(-1, NULL, bsm);
		if (s) {
			return cba_state_machine_layout(0, s, bsm);
		}
		return 0;
	}
	return state;
}


/* this state machine works for pdftotext default output:
 * pdftotext -nodiag -nopgbrk  S20250331.pdf
 */
static int cba_state_machine_default(int state, char *s, Transaction *bsm) {

	switch (state) {
	case -1:	/* initial the state machine */
		transaction_reset(bsm);
		//return 0;
		return 10;
	
	case 10:	/* waiting for "Statement" */
		if (!strncmp(s, "Statement", 9)) {
			return 11;
		}
		break;

	case 11:	/* waiting for "Period" */
		if (!strncmp(s, "Period", 6)) {
			return 12;
		}
		break;

	case 12:	/* waiting for "1 Jan 2025 - 31 Mar 2025" */
		if (!BlankLine(s) && take_time(s, NULL)) {
			print_csv_title(bsm, s);
			return 0;
		}
		break;

	case 0:		/* waiting for Transaction begin */
		if ((s = take_time(s, &bsm->date_acct)) != NULL) {
			log("%d<<< %s\n", state, s);
			/* Transaction begin: move to state 1 */
			strcat(bsm->transaction, s);
			return 1;
		}
		break;	/* ignore others */

	case 1:		/* waiting for transaction contents */
		if (take_time(s, NULL)) {
			/* another transaction begin */
			return cba_state_machine_default(5, s, bsm);
		}
		log("%d<<< %s\n", state, s);
		if (BlankLine(s)) {
			return 2;	/* read g_debit */
		}
		/* read a continues line */
		strcat(bsm->transaction, " ");
		strcat(bsm->transaction, s);
		break;

	case 2:		/* waiting for debit data */
		if (take_time(s, NULL)) {
			/* another transaction begin */
			return cba_state_machine_default(5, s, bsm);
		}
		if ((*s == '$') && strstr(s, "CR")) {
			log("%d<<< %s\n", state, s);
			take_amount(s, bsm->balance, sizeof(bsm->balance));
			return cba_state_machine_default(5, NULL, bsm);
		} else if (*s == '$') {
			log("%d<<< %s\n", state, s);
			take_amount(s, bsm->credit, sizeof(bsm->credit));
			return 4;
		} else if (take_amount(s, bsm->debit, sizeof(bsm->debit))) {
			log("%d<<< %s\n", state, s);
			return 3;
		}
		break;

	case 3:		/* wating for credit data */
		if (take_time(s, NULL)) {
			/* another transaction begin */
			return cba_state_machine_default(5, s, bsm);
		}
		if (*s == '$') {
			log("%d<<< %s\n", state, s);
			if (strstr(s, "CR")) {
				take_amount(s, bsm->balance, sizeof(bsm->balance));
				return cba_state_machine_default(5, NULL, bsm);
			} else {
				take_amount(s, bsm->credit, sizeof(bsm->credit));
				return 4;
			}
		}
		break;

	case 4:		/* waiting for balance data */
		if (take_time(s, NULL)) {
			/* another transaction begin */
			return cba_state_machine_default(5, s, bsm);
		}
		if ((*s == '$') && strstr(s, "CR")) {
			log("%d<<< %s\n", state, s);
			take_amount(s, bsm->balance, sizeof(bsm->balance));
			return cba_state_machine_default(5, NULL, bsm);
		}
		break;
	
	case 5:		/* finish one transaction */
		print_transaction(bsm);
		cba_state_machine_default(-1, NULL, bsm);
		if (s) {
			return cba_state_machine_default(0, s, bsm);
		}
		return 0;
	}
	return state;
}

