
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "statement.h"

#define STAT_NAB_MORE		(STAT_MORE + 1)


/* this state machine works for pdftotext layout output:
 * pdftotext -nodiag -nopgbrk -layout S20250331.pdf
 */
static int nab_state_machine_creditcard(int state, char *s, Transaction *bsm) {

	char	*p;

	switch (state) {
	case STAT_INIT:
		transaction_reset(bsm);
		print_csv_title(bsm, NULL);
		return STAT_BEGIN;
	
	case STAT_FILTER:
		if (strstr(s, "Your balance and interest breakdown")) {
			return STAT_EXIT;
		}
		if (!isspace(*s) && !isdigit(*s)) {
			return STAT_MORE;
		}
		if (!StrNCmp(bare(s), "Page ")) {
			return STAT_MORE;
		}
		if (state == STAT_NAB_MORE) {
			if (!isspace(*s)) {
				return STAT_MORE;
			}
		}
		break;

	case STAT_BEGIN:	/* waiting for Transaction begin */
		if ((s = take_time(s, &bsm->tm_acct)) == NULL) {
			break;
		}

		/* Transaction begin: need more description or received all */
		log("%d<<< %s\n", state, s);
		if ((s = take_time(bare(s), &bsm->tm_trans)) == NULL) {
			break;
		}

		s = bare(s);
		if ((p = strstr(s, TRANSGAP)) == NULL) {
			break;
		}
		*p = 0;
		morphcat(bsm->acc_no, s, sizeof(bsm->acc_no));

		s = bare(skip(p+1));
		if ((p = strstr(s, TRANSGAP)) == NULL) {
			break;
		}
		*p = 0;
		morphcat(bsm->transaction, s, sizeof(bsm->transaction));

		s = bare(skip(p+1));
		if (!strrcmp(s, " CR")) {
			take_amount(s, bsm->credit, sizeof(bsm->credit));
		} else {
			take_amount(s, bsm->debit, sizeof(bsm->debit));
		}
		return STAT_NAB_MORE;

	case STAT_NAB_MORE:	/* waiting for transaction descriptions */
		if (take_time(s, NULL)) {
			/* another transaction begin */
			return nab_state_machine_creditcard(STAT_OUTPUT, s, bsm);
		}
		log("%d<<< %s\n", state, s);
		if (BlankLine(s)) {
			return nab_state_machine_creditcard(STAT_OUTPUT, NULL, bsm);
		}
		safecat(bsm->transaction, " ", sizeof(bsm->transaction));
		morphcat(bsm->transaction, s, sizeof(bsm->transaction));
		break;

	case STAT_OUTPUT:		/* finish one transaction */
		print_transaction(bsm);
		transaction_reset(bsm);
		if (s) {
			return nab_state_machine_creditcard(STAT_BEGIN, s, bsm);
		}
		return STAT_BEGIN;
	}
	return state;
}


smachine_t nab_main(FILE *fin) {

	char	buf[512];
	int	i;

	/* read magic from 5 lines */
	for (i = 0; (i < 5) && fgets(buf, sizeof(buf), fin); i++) {
		if (strstr(buf, "NAB ")) {
			return nab_state_machine_creditcard;
		}
	}
	rewind(fin);
	return NULL;
}

