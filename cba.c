
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "statement.h"

#define TRANSGAP		"  "

#define STAT_CBA_STATEMENT	(STAT_MORE + 1)
#define STAT_CBA_PERIOD		(STAT_MORE + 2)
#define STAT_CBA_DATE		(STAT_MORE + 3)
#define STAT_CBA_MORE		(STAT_MORE + 4)
#define STAT_CBA_DEBIT		(STAT_MORE + 5)
#define STAT_CBA_CREDIT		(STAT_MORE + 6)
#define STAT_CBA_BALANCE	(STAT_MORE + 7)


static int cba_state_machine_layout(int state, char *s, Transaction *bsm);
static int cba_state_machine_default(int state, char *s, Transaction *bsm);


smachine_t cba_main(FILE *fin) {

	char	buf[512];
	int	i;

	/* read magic from 5 lines */
	for (i = 0; (i < 5) && fgets(buf, sizeof(buf), fin); i++) {
		if (!StrNCmp(buf, "Your Statement")) {
			return cba_state_machine_default;
		}
		if (!strrcmp(buf, "Your Statement\n")) {
			return cba_state_machine_layout;
		}
	}
	rewind(fin);
	return NULL;
}


/* this state machine works for pdftotext layout output:
 * pdftotext -nodiag -nopgbrk -layout S20250331.pdf
 */
static int cba_state_machine_layout(int state, char *s, Transaction *bsm) {

	char	*p;

	switch (state) {
	case STAT_INIT:
		transaction_reset(bsm);
		return STAT_CBA_PERIOD;
	
	case STAT_FILTER:
		/* in layout mode, anything NOT start with ' ' is ignored */
		if (!isspace(*s)) {
			return STAT_MORE;
		}
		if (strstr(s, "Opening balance")) {
			return STAT_EXIT;
		}
		break;

	case STAT_CBA_PERIOD:	/* waiting for "THE DRIECTOR" */
		if (StrNCmp(s, "THE DRIECTOR")) {
			break;
		}
		if ((s = strstr(s, "Period")) == NULL) {
			break;		/* make sure second part exists */
		}
		/* pick out date like "1 Jan 2025 - 31 Mar 2025" */
		s = bare(skip(s));	/* skip "Period" and spaces after */
		if (!BlankLine(s) && take_time(s, &bsm->tm_acct)) {
			print_csv_title(bsm, s);
			return STAT_BEGIN;
		}
		break;

	case STAT_BEGIN:	/* waiting for Transaction begin */
		if ((p = take_time(s, &bsm->tm_acct)) == NULL) {
			break;
		}

		/* Transaction begin: need more description or received all */
		log("%d<<< %s\n", state, s);
		s = p;
		if (strrcmp(s, " CR")) {	/* waiting for more lines */
			cutcat(bsm->transaction, s, sizeof(bsm->transaction));
			return STAT_CBA_MORE;
		}
		if ((p = strstr(s, TRANSGAP)) != NULL) {
			*p++ = 0;
			cutcat(bsm->transaction, s, sizeof(bsm->transaction));
			s = bare(p);
		}
		return cba_state_machine_layout(STAT_CBA_BALANCE, s, bsm);

	case STAT_CBA_MORE:	/* waiting for transaction ends */
		if (take_time(s, NULL)) {
			/* another transaction begin */
			return cba_state_machine_layout(STAT_OUTPUT, s, bsm);
		}
		log("%d<<< %s\n", state, s);
		if (strrcmp(s, " CR")) {	/* waiting for more lines */
			if (*s) {
				safecat(bsm->transaction, " ", sizeof(bsm->transaction));
				cutcat(bsm->transaction, s, sizeof(bsm->transaction));
			}
			break;
		}
		if ((p = strstr(s, TRANSGAP)) != NULL) {
			*p++ = 0;
			if (*s) {
				safecat(bsm->transaction, " ", sizeof(bsm->transaction));
				cutcat(bsm->transaction, s, sizeof(bsm->transaction));
			}
			s = bare(p);
		}
		return cba_state_machine_layout(STAT_CBA_BALANCE, s, bsm);

	case STAT_CBA_BALANCE:	/* collecting transaction data */
		log("%d<<< %s\n", state, s);
		if ((*s == '$') && !strchr(s+1, '$')) {	/* balance only */
			take_amount(s, bsm->balance, sizeof(bsm->balance));
		} else if (*s == '$') {		/* g_credit and balance */
			take_amount(s, bsm->credit, sizeof(bsm->credit));
			if ((s = strrchr(s, '$')) != NULL) {
				take_amount(s, bsm->balance, sizeof(bsm->balance));
			}
		} else {		/* g_debit and balance */
			take_amount(s, bsm->debit, sizeof(bsm->debit));
			if ((s = strrchr(s, '$')) != NULL) {
				take_amount(s, bsm->balance, sizeof(bsm->balance));
			}
		} 
		return cba_state_machine_layout(STAT_OUTPUT, NULL, bsm);

	case STAT_OUTPUT:		/* finish one transaction */
		print_transaction(bsm);
		transaction_reset(bsm);
		if (s) {
			return cba_state_machine_layout(STAT_BEGIN, s, bsm);
		}
		return STAT_BEGIN;
	}
	return state;
}


/* this state machine works for pdftotext default output:
 * pdftotext -nodiag -nopgbrk  S20250331.pdf
 * default output of pdftotext is unaligned. DO NOT USE DEFAULT OUPUT.
 */
static int cba_state_machine_default(int state, char *s, Transaction *bsm) {

	if (s) {
		s = bare(chop(s));    /* clean up the head and tail */
	}

	switch (state) {
	case STAT_INIT:
		transaction_reset(bsm);
		return STAT_CBA_STATEMENT;

	case STAT_FILTER:
		if (strstr(s, "Opening balance")) {
			return STAT_EXIT;
		}
		break;

	case STAT_CBA_STATEMENT:	/* waiting for "Statement" */
		if (!StrNCmp(s, "Statement")) {
			return STAT_CBA_PERIOD;
		}
		break;

	case STAT_CBA_PERIOD:	/* waiting for "Period" */
		if (!StrNCmp(s, "Period")) {
			return STAT_CBA_DATE;
		}
		break;

	case STAT_CBA_DATE:	/* waiting for "1 Jan 2025 - 31 Mar 2025" */
		if (!BlankLine(s) && take_time(s, NULL)) {
			print_csv_title(bsm, s);
			return STAT_BEGIN;
		}
		break;

	case STAT_BEGIN:		/* waiting for Transaction begin */
		if ((s = take_time(s, &bsm->tm_acct)) != NULL) {
			log("%d<<< %s\n", state, s);
			/* Transaction begin:  */
			cutcat(bsm->transaction, s, sizeof(bsm->transaction));
			return STAT_CBA_MORE;
		}
		break;	/* ignore others */

	case STAT_CBA_MORE:	/* waiting for more description */
		if (take_time(s, NULL)) {	/* another transaction begin */
			log("%d<<< %s\n", state, s);
			return cba_state_machine_default(STAT_OUTPUT, s, bsm);
		}
		log("%d<<< %s\n", state, s);
		if (BlankLine(s)) {	/* no more description */
			return STAT_CBA_DEBIT;	/* read g_debit */
		}
		/* read a continues line */
		safecat(bsm->transaction, " ", sizeof(bsm->transaction));
		cutcat(bsm->transaction, s, sizeof(bsm->transaction));
		break;

	case STAT_CBA_DEBIT:	/* waiting for debit data */
		if (take_time(s, NULL)) {	/* another transaction begin */
			return cba_state_machine_default(STAT_OUTPUT, s, bsm);
		}
		if ((*s == '$') && strstr(s, "CR")) {
			/* no debit data, no credit data, balance only */
			log("%d<<< %s\n", state, s);
			take_amount(s, bsm->balance, sizeof(bsm->balance));
			return cba_state_machine_default(STAT_OUTPUT, NULL, bsm);
		} else if (*s == '$') {
			/* no debit data, credit data only */
			log("%d<<< %s\n", state, s);
			take_amount(s, bsm->credit, sizeof(bsm->credit));
			return STAT_CBA_BALANCE;
		} else if (take_amount(s, bsm->debit, sizeof(bsm->debit))) {
			/* received the debit data */
			log("%d<<< %s\n", state, s);
			return STAT_CBA_CREDIT;
		}
		break;

	case STAT_CBA_CREDIT:	/* wating for credit data */
		if (take_time(s, NULL)) {	/* another transaction begin */
			return cba_state_machine_default(STAT_OUTPUT, s, bsm);
		}
		if (*s == '$') {
			log("%d<<< %s\n", state, s);
			if (strstr(s, "CR")) {
				/* no credit data, balance only */
				take_amount(s, bsm->balance, sizeof(bsm->balance));
				return cba_state_machine_default(STAT_OUTPUT, NULL, bsm);
			} else {
				/* received the credit data */
				take_amount(s, bsm->credit, sizeof(bsm->credit));
				return STAT_CBA_BALANCE;
			}
		}
		break;

	case STAT_CBA_BALANCE:	/* waiting for balance data */
		if (take_time(s, NULL)) {	/* another transaction begin */
			return cba_state_machine_default(STAT_OUTPUT, s, bsm);
		}
		if ((*s == '$') && strstr(s, "CR")) {
			log("%d<<< %s\n", state, s);
			take_amount(s, bsm->balance, sizeof(bsm->balance));
			return cba_state_machine_default(STAT_OUTPUT, NULL, bsm);
		}
		break;
	
	case STAT_OUTPUT:		/* finish one transaction */
		print_transaction(bsm);
		transaction_reset(bsm);
		if (s) {
			return cba_state_machine_default(STAT_BEGIN, s, bsm);
		}
		return STAT_BEGIN;
	}
	return state;
}

