/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* DynaHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "lev.h"
#include "dlb.h"

/*	[note: this comment is fairly old, but still accurate for 3.1]
 * Rumors have been entirely rewritten to speed up the access.  This is
 * essential when working from floppies.  Using fseek() the way that's done
 * here means rumors following longer rumors are output more often than those
 * following shorter rumors.  Also, you may see the same rumor more than once
 * in a particular game (although the odds are highly against it), but
 * this also happens with real fortune cookies.  -dgk
 */

/*	3.1
 * The rumors file consists of a "do not edit" line, a hexadecimal number
 * giving the number of bytes of useful/true rumors, followed by those
 * true rumors (one per line), followed by the useless/false/misleading/cute
 * rumors (also one per line).  Number of bytes of untrue rumors is derived
 * via fseek(EOF)+ftell().
 *
 * The oracles file consists of a "do not edit" comment, a decimal count N
 * and set of N+1 hexadecimal fseek offsets, followed by N multiple-line
 * records, separated by "---" lines.  The first oracle is a special case,
 * and placed there by 'makedefs'.
 */

static void init_rumors(dlb *);
static void init_oracles(dlb *);
static void outoracle(boolean, boolean);

static int true_rumor_start,  true_rumor_size,  true_rumor_end,
	   false_rumor_start, false_rumor_size, false_rumor_end;
static int oracle_flg = 0;  /* -1=>don't use, 0=>need init, 1=>init done */
static unsigned oracle_cnt = 0;
static int *oracle_loc = 0;


static void init_rumors(dlb *fp)
{
	char line[BUFSZ];

	dlb_fgets(line, sizeof line, fp); /* skip "don't edit" comment */
	dlb_fgets(line, sizeof line, fp);
	if (sscanf(line, "%6x\n", &true_rumor_size) == 1 &&
	    true_rumor_size > 0L) {
	    dlb_fseek(fp, 0L, SEEK_CUR);
	    true_rumor_start  = dlb_ftell(fp);
	    true_rumor_end    = true_rumor_start + true_rumor_size;
	    dlb_fseek(fp, 0L, SEEK_END);
	    false_rumor_end   = dlb_ftell(fp);
	    false_rumor_start = true_rumor_end;	/* ok, so it's redundant... */
	    false_rumor_size  = false_rumor_end - false_rumor_start;
	} else
	    true_rumor_size = -1L;	/* init failed */
}

/* exclude_cookie is a hack used because we sometimes want to get rumors in a
 * context where messages such as "You swallowed the fortune!" that refer to
 * cookies should not appear.  This has no effect for true rumors since none
 * of them contain such references anyway.
 */
char *getrumor(int truth, /* 1=true, -1=false, 0=either */
	       char *rumor_buf,
	       boolean exclude_cookie)
{
	dlb	*rumors;
	int tidbit, beginning;
	char	*endp, line[BUFSZ], xbuf[BUFSZ];

	rumor_buf[0] = '\0';
	if (true_rumor_size < 0L)	/* we couldn't open RUMORFILE */
		return rumor_buf;

	rumors = dlb_fopen(RUMORFILE, "r");

	if (rumors) {
	    int count = 0;
	    int adjtruth;

	    do {
		rumor_buf[0] = '\0';
		if (true_rumor_size == 0L) {	/* if this is 1st outrumor() */
		    init_rumors(rumors);
		    if (true_rumor_size < 0L) {	/* init failed */
			sprintf(rumor_buf, "Error reading \"%.80s\".",
				RUMORFILE);
			return rumor_buf;
		    }
		}
		/*
		 *	input:      1    0   -1
		 *	 rn2 \ +1  2=T  1=T  0=F
		 *	 adj./ +0  1=T  0=F -1=F
		 */
		switch (adjtruth = truth + rn2(2)) {
		  case  2:	/*(might let a bogus input arg sneak thru)*/
		  case  1:  beginning = true_rumor_start;
			    tidbit = mt_random() % true_rumor_size;
			break;
		  case  0:	/* once here, 0 => false rather than "either"*/
		  case -1:  beginning = false_rumor_start;
			    tidbit = mt_random() % false_rumor_size;
			break;
		  default:
			    warning("strange truth value for rumor");
			return strcpy(rumor_buf, "Oops...");
		}
		dlb_fseek(rumors, beginning + tidbit, SEEK_SET);
		dlb_fgets(line, sizeof line, rumors);
		if (!dlb_fgets(line, sizeof line, rumors) ||
		    (adjtruth > 0 && dlb_ftell(rumors) > true_rumor_end)) {
			/* reached end of rumors -- go back to beginning */
			dlb_fseek(rumors, beginning, SEEK_SET);
			dlb_fgets(line, sizeof line, rumors);
		}
		if ((endp = strchr(line, '\n')) != 0) *endp = 0;
		strcat(rumor_buf, xcrypt(line, xbuf));
	    } while (count++ < 50 && exclude_cookie && (strstri(rumor_buf, "fortune") || strstri(rumor_buf, "pity")));
	    dlb_fclose(rumors);
	    if (count >= 50)
		warning("Can't find non-cookie rumor?");
	    else
		exercise(A_WIS, (adjtruth > 0));
	} else {
		pline("Can't open rumors file!");
		true_rumor_size = -1;	/* don't try to open it again */
	}
	return rumor_buf;
}

void outrumor(int truth, /* 1=true, -1=false, 0=either */
	      int mechanism)
{
	static const char fortune_msg[] =
		"This cookie has a scrap of paper inside.";
	const char *line;
	char buf[BUFSZ];
	boolean reading = (mechanism == BY_COOKIE ||
			   mechanism == BY_PAPER);

	if (reading) {
	    /* deal with various things that prevent reading */
	    if (is_fainted() && mechanism == BY_COOKIE)
	    	return;
	    else if (Blind) {
		if (mechanism == BY_COOKIE)
			pline(fortune_msg);
		pline("What a pity that you cannot read it!");
	    	return;
	    }
	}
	line = getrumor(truth, buf, reading ? FALSE : TRUE);
	if (!*line)
		line = "Rumors file closed for renovation.";
	switch (mechanism) {
	    case BY_ORACLE:
	 	/* Oracle delivers the rumor */
		pline("True to her word, the Oracle %ssays: ",
		  (!rn2(4) ? "offhandedly " : (!rn2(3) ? "casually " :
		  (rn2(2) ? "nonchalantly " : ""))));
		verbalize("%s", line);
		exercise(A_WIS, TRUE);
		return;
	    case BY_COOKIE:
		pline(fortune_msg);
		/* FALLTHRU */
	    case BY_PAPER:
		pline("It reads:");
		break;
	}
	pline("%s", line);
}

static void init_oracles(dlb *fp)
{
	int i;
	char line[BUFSZ];
	unsigned int cnt = 0;

	/* this assumes we're only called once */
	dlb_fgets(line, sizeof line, fp); /* skip "don't edit" comment*/
	dlb_fgets(line, sizeof line, fp);
	if (sscanf(line, "%5d\n", &cnt) == 1 && cnt > 0) {
	    oracle_cnt = cnt;
	    oracle_loc = malloc(cnt * sizeof(int));
	    for (i = 0; i < cnt; i++) {
		dlb_fgets(line, sizeof line, fp);
		sscanf(line, "%5x\n", &oracle_loc[i]);
	    }
	}
	return;
}

void save_oracles(struct memfile *mf)
{
	int i;
	mtag(mf, 0, MTAG_ORACLES);
	mwrite32(mf, oracle_cnt);
	if (oracle_cnt)
	    for (i = 0; i < oracle_cnt; i++)
		mwrite32(mf, oracle_loc[i]);
}


void free_oracles(void)
{
	if (oracle_cnt) {
	    free(oracle_loc);
	    oracle_loc = NULL;
	    oracle_cnt = 0;
	    oracle_flg = 0;
	}
}


void restore_oracles(struct memfile *mf)
{
	int i;
	oracle_cnt = mread32(mf);
	if (oracle_cnt) {
	    oracle_loc = malloc(oracle_cnt * sizeof(int));
	    for (i = 0; i < oracle_cnt; i++)
		oracle_loc[i] = mread32(mf);
	    oracle_flg = 1;	/* no need to call init_oracles() */
	}
}

void outoracle(boolean special, boolean delphi)
{
	char	line[COLNO];
	char	*endp;
	dlb	*oracles;
	int oracle_idx;
	char xbuf[BUFSZ];

	if (oracle_flg < 0 ||			/* couldn't open ORACLEFILE */
	   (oracle_flg > 0 && oracle_cnt == 0))	/* oracles already exhausted */
		return;

	oracles = dlb_fopen(ORACLEFILE, "r");

	if (oracles) {
		struct menulist menu;
		
		if (oracle_flg == 0) {	/* if this is the first outoracle() */
			init_oracles(oracles);
			oracle_flg = 1;
			if (oracle_cnt == 0)
			    return;
		}
		/* oracle_loc[0] is the special oracle;		*/
		/* oracle_loc[1..oracle_cnt-1] are normal ones	*/
		if (oracle_cnt <= 1 && !special)
		    return;  /*(shouldn't happen)*/
		oracle_idx = special ? 0 : rnd((int) oracle_cnt - 1);
		dlb_fseek(oracles, oracle_loc[oracle_idx], SEEK_SET);
		if (!special)
		    oracle_loc[oracle_idx] = oracle_loc[--oracle_cnt];

		init_menulist(&menu);
		if (delphi)
		    add_menutext(&menu, special ?
		          "The Oracle scornfully takes all your money and says:" :
		          "The Oracle meditates for a moment and then intones:");
		else
		    add_menutext(&menu, "The message reads:");
		add_menutext(&menu, "");

		while (dlb_fgets(line, COLNO, oracles) && strcmp(line,"---\n")) {
			if ((endp = strchr(line, '\n')) != 0) *endp = 0;
			add_menutext(&menu, xcrypt(line, xbuf));
		}
		display_menu(menu.items, menu.icount, NULL, PICK_NONE, NULL);
		free(menu.items);
		dlb_fclose(oracles);
	} else {
		pline("Can't open oracles file!");
		oracle_flg = -1;	/* don't try to open it again */
	}
}


int doconsult(struct monst *oracl)
{
        int umoney = money_cnt(invent);
	int u_pay, minor_cost = 50, major_cost = 500 + 50 * u.ulevel;
	int add_xpts;
	char qbuf[QBUFSZ];

	multi = 0;

	if (!oracl) {
		pline("There is no one here to consult.");
		return 0;
	} else if (!oracl->mpeaceful) {
		pline("%s is in no mood for consultations.", Monnam(oracl));
		return 0;
	} else if (!umoney) {
		pline("You have no money.");
		return 0;
	}

	sprintf(qbuf,
		"\"Wilt thou settle for a minor consultation?\" (%d %s)",
		minor_cost, currency(minor_cost));
	switch (ynq(qbuf)) {
	    default:
	    case 'q':
		return 0;
	    case 'y':
		if (umoney < minor_cost) {
		    pline("You don't even have enough money for that!");
		    return 0;
		}
		u_pay = minor_cost;
		break;
	    case 'n':
		if (umoney <= minor_cost ||	/* don't even ask */
		    (oracle_cnt == 1 || oracle_flg < 0)) return 0;
		sprintf(qbuf,
			"\"Then dost thou desire a major one?\" (%d %s)",
			major_cost, currency(major_cost));
		if (yn(qbuf) != 'y') return 0;
		u_pay = (umoney < major_cost ? umoney : major_cost);

		break;
	}

	money2mon(oracl, u_pay);
	iflags.botl = 1;
	add_xpts = 0;	/* first oracle of each type gives experience points */
	if (u_pay == minor_cost) {
		outrumor(1, BY_ORACLE);
		if (!u.uevent.minor_oracle)
		    add_xpts = u_pay / (u.uevent.major_oracle ? 25 : 10);
		    /* 5 pts if very 1st, or 2 pts if major already done */
		u.uevent.minor_oracle = TRUE;
	} else {
		boolean cheapskate = u_pay < major_cost;
		outoracle(cheapskate, TRUE);
		if (!cheapskate && !u.uevent.major_oracle)
		    add_xpts = u_pay / (u.uevent.minor_oracle ? 25 : 10);
		    /* ~100 pts if very 1st, ~40 pts if minor already done */
		u.uevent.major_oracle = TRUE;
		historic_event(FALSE, "received advice from The Oracle.");
		exercise(A_WIS, !cheapskate);
	}
	if (add_xpts) {
		more_experienced(add_xpts, add_xpts, u_pay/50);
		newexplevel();
	}
	return 1;
}

/*rumors.c*/
