/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* DynaHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Summary of all NetHack's object naming functions:
   obj_typename(otyp): entry in discovery list, from player's point of view
   "scroll of mail (KIRJE)"
   simple_typename(otyp): object type's actual name or appearance, ignoring
   user-assigned names
   "scroll of mail" (identified); "scroll labeled KIRJE" (unidentified)
   distant_name(obj,func): name object as per func, except as if it isn't
   currently examinable by the user
   "potion", "scroll", "sword", "orange potion" (if previously viewed)
   fruitname(juice): name of material fruit is made from, or fruit juice
   e.g. with "slice of pizza": "pizza" (juice==0), "pizza juice" (juice == 1)
   xname(obj): general-use name as if player is viewing an object now
   "potions of sickness", "potion" (blind), "corpse", "orange potion" (unIDed)
   mshot_xname(obj): name of fired missile in a volley
   "the 2nd dagger", "the 4th arrow"
   doname(obj): fully detailed name of object or stack as seen in inventory
   "the blessed Amulet of Yendor (being worn)", "a poisoned +4 dagger"
   doname_price(obj): like doname, but with price info for shop items
   corpse_xname(obj, ignore_oquan): describe a corpse or pile of corpses
   "horse corpse", "horse corpses"
   cxname(obj): xname, but with specific corpse naming
   "potion of sickness", "horse corpses"
   killer_xname(obj): name from the game's view, minus info like BCU and greasedness
   "scroll of mail" (even if un-IDed)
   singular(obj,func): name one object of a stack as per func
   an(str): prefix "a" or "an" to str, if necessary
   An(str): prefix "A" or "An" to str, if necessary
   the(str): prefix "the" or "The" to str, if necessary
   The(str): prefix "the" or "The" to str, if necessary
   aobjnam(obj, verb): general-purpose name with precise stack sizes, and
   optional combined verb; otherwise like cxname
   "4 horse corpses", "3 orange potions shatter!", "speed boots burn"
   Tobjnam(obj, verb): general-purpose name with imprecise stack sizes,
   prepended "The", and optional combined verb; otherwise like xname
   "The corpses", "The orange potions shatter!", "The speed boots burn"
   otense(obj, verb): Conjugate verb as if obj was verbing
   "shatters" (stack size 1), "shatter" (stack size 2)
   vtense(subj, verb): Conjgate verb as if subj was verbing
   "they","shatter" -> "shatter"; "you","shatter" -> "shatters"
   Doname2(obj): doname() with leading capital
   "The blessed Amulet of Yendor (being worn)", "A poisoned +4 dagger"
   yname(obj): like xname(), but incorporates ownership details
   "your potions called Y", "Medusa's potion of oil", "the apple named X"
   Yname2(obj): yname() with leading capital
   "Your potions called Y", "Medusa's potion of oil", "The apple named X"
   ysimple_name(obj): like simple_typename(), with ownership details
   "your orange potions", "Medusa's potion of oil", "the apple"
   Ysimple_name2(obj): ysimple_name() with leading capital
   "Your orange potions", "Medusa's potion of oil", "The apple"
   makeplural(str): returns plural version of str
   "sheep" -> "sheep", "lump of sheep" -> "lumps of sheep", "mumak" -> "mumakil"
   makesingular(str): opposite of makeplural
   readobjname(str, default_obj, from_user): convert string to object
   if "nothing" is given, default_obj is returned
   cloak_simple_name(cloak): return vague description of given cloak
   "robe", "wrapping", "apron", "smock", "cloak"
   mimic_obj_name(monster): return object that mimic is mimicking
   "gold", "orange", "whatcha-may-callit" (mimic is mimicking a ])
 */

/* "an [uncursed] greased [very rotten] partly eaten guardian naga hatchling (corpse)" */
#define PREFIX	80	/* (73) */
#define SCHAR_LIM 127
#define NUMOBUF 12

static char *strprepend(char *,const char *);
static boolean wishymatch(const char *,const char *,boolean);
static char *nextobuf(void);
static void add_erosion_words(const struct obj *obj, char *, boolean);
static char *xname2(const struct obj *obj, boolean ignore_oquan);

struct Jitem {
	int item;
	const char *name;
};

/* true for gems/rocks that should have " stone" appended to their names */
#define GemStone(typ)	(typ == FLINT ||				\
			 (objects[typ].oc_material == GEMSTONE &&	\
			  (typ != DILITHIUM_CRYSTAL && typ != RUBY &&	\
			   typ != DIAMOND && typ != SAPPHIRE &&		\
			   typ != BLACK_OPAL && 	\
			   typ != EMERALD && typ != OPAL)))


static const struct Jitem Japanese_items[] = {
	{ SHORT_SWORD, "wakizashi" },
	{ BROADSWORD, "ninja-to" },
	{ FLAIL, "nunchaku" },
	{ GLAIVE, "naginata" },
	{ LOCK_PICK, "osaku" },
	{ WOODEN_HARP, "koto" },
	{ KNIFE, "shito" },
	{ PLATE_MAIL, "tanko" },
	{ HELMET, "kabuto" },
	{ LEATHER_GLOVES, "yugake" },
	{ FOOD_RATION, "gunyoki" },
	{ POT_BOOZE, "sake" },
	{0, "" }
};


static const char *Japanese_item_name(int i);

static char *strprepend(char *s, const char *pref)
{
	int i = (int)strlen(pref);

	if (i > PREFIX) {
		warning("PREFIX too short (for %d).", i);
		return s;
	}
	s -= i;
	strncpy(s, pref, i);	/* do not copy trailing 0 */
	return s;
}


/* manage a pool of BUFSZ buffers, so callers don't have to */
static char *nextobuf(void)
{
	static char bufs[NUMOBUF][BUFSZ];
	static int bufidx = 0;

	bufidx = (bufidx + 1) % NUMOBUF;
	return bufs[bufidx];
}

char *obj_typename(int otyp)
{
	char *buf = nextobuf();
	struct objclass *ocl = &objects[otyp];
	const char *actualn = OBJ_NAME(*ocl);
	const char *dn = OBJ_DESCR(*ocl);
	const char *un = ocl->oc_uname;
	int nn = ocl->oc_name_known;

	if (Role_if (PM_SAMURAI) && Japanese_item_name(otyp))
		actualn = Japanese_item_name(otyp);
	switch(ocl->oc_class) {
	case COIN_CLASS:
		strcpy(buf, "coin");
		break;
	case POTION_CLASS:
		strcpy(buf, "potion");
		break;
	case SCROLL_CLASS:
		strcpy(buf, "scroll");
		break;
	case WAND_CLASS:
		strcpy(buf, "wand");
		break;
	case SPBOOK_CLASS:
		strcpy(buf, "spellbook");
		break;
	case RING_CLASS:
		strcpy(buf, "ring");
		break;
	case AMULET_CLASS:
		if (nn)
			strcpy(buf,actualn);
		else
			strcpy(buf,"amulet");
		if (un)
			sprintf(eos(buf)," called %s",un);
		if (dn)
			sprintf(eos(buf)," (%s)",dn);
		return buf;
	default:
		if (nn) {
			strcpy(buf, actualn);
			if (GemStone(otyp))
				strcat(buf, " stone");
			if (un)
				sprintf(eos(buf), " called %s", un);
			if (dn)
				sprintf(eos(buf), " (%s)", dn);
		} else {
			strcpy(buf, dn ? dn : actualn);
			if (ocl->oc_class == GEM_CLASS)
				strcat(buf, (ocl->oc_material == MINERAL) ?
						" stone" : " gem");
			if (un)
				sprintf(eos(buf), " called %s", un);
		}
		return buf;
	}
	/* here for ring/scroll/potion/wand */
	if (nn) {
	    if (ocl->oc_unique)
		strcpy(buf, actualn); /* avoid spellbook of Book of the Dead */
	    else
		sprintf(eos(buf), " of %s", actualn);
	}
	if (un)
		sprintf(eos(buf), " called %s", un);
	if (dn)
		sprintf(eos(buf), " (%s)", dn);
	return buf;
}

/* less verbose result than obj_typename(); either the actual name
   or the description (but not both); user-assigned name is ignored */
char *simple_typename(int otyp)
{
    char *bufp, *pp, *save_uname = objects[otyp].oc_uname;

    objects[otyp].oc_uname = 0;		/* suppress any name given by user */
    bufp = obj_typename(otyp);
    objects[otyp].oc_uname = save_uname;
    if ((pp = strstri(bufp, " (")) != 0)
	*pp = '\0';		/* strip the appended description */
    return bufp;
}

boolean obj_is_pname(const struct obj *obj)
{
    return((boolean)(obj->dknown && obj->known && obj->onamelth &&
		     /* Since there aren't any objects which are both
		        artifacts and unique, the last check is redundant. */
		     obj->oartifact && !objects[obj->otyp].oc_unique));
}

/* Give the name of an object seen at a distance.  Unlike xname/doname,
 * we don't want to set dknown if it's not set already.  The kludge used is
 * to temporarily set Blind so that xname() skips the dknown setting.  This
 * assumes that we don't want to do this too often; if this function becomes
 * frequently used, it'd probably be better to pass a parameter to xname()
 * or doname() instead.
 */
char *distant_name(const struct obj *obj, char *(*func)(const struct obj*))
{
	char *str;

	long save_Blinded = Blinded;
	Blinded = 1;
	str = (*func)(obj);
	Blinded = save_Blinded;
	return str;
}

/* convert player specified fruit name into corresponding fruit juice name
   ("slice of pizza" -> "pizza juice" rather than "slice of pizza juice") */
/* juice: whether or not to append " juice" to the name */
char *fruitname(boolean juice)
{
    char *buf = nextobuf();
    const char *fruit_nam = strstri(pl_fruit, " of ");

    if (fruit_nam)
	fruit_nam += 4;		/* skip past " of " */
    else
	fruit_nam = pl_fruit;	/* use it as is */

    sprintf(buf, "%s%s", makesingular(fruit_nam), juice ? " juice" : "");
    return buf;
}


/* Add suffixes (and only suffixes) to objects with special properties. */
static void propnames(char *buf, long props, long props_known,
		      boolean weapon, boolean has_of)
{
	boolean dump_ID_flag = program_state.gameover;
	boolean do_short;
	boolean first_of;

	/* Decide the properties that are to be appended. */
	props &= (dump_ID_flag ? ITEM_PROP_MASK : props_known);
	/* draining weapons are prefixed with "thirsty" elsewhere */
	if (weapon) props &= ~ITEM_DRLI;
	/* "vorpal" and "oilskin" are prefix-only */
	props &= ~(ITEM_VORPAL|ITEM_OILSKIN);

	/* Prevent buffer overflows from overly-long names. */
	do_short = (longbits(props) > 2);

	first_of = !has_of;

	/* Unforunately, trying to pull this out into an inline helper function
	 * leads to tons of argument-passing noise. :| */
#define propnames_CASE(pr, shortnm, weapnm, usualnm) \
	do { \
	    if (props & (pr)) { \
		sprintf(eos(buf), " %s%s %s%s", \
			((props_known & (pr)) ? "" : "["), \
			(first_of ? "of" : (do_short ? "&" : "and")), \
			(do_short ? (shortnm) : \
			    ((weapon && (weapnm)) ? (weapnm) : (usualnm))), \
			((props_known & (pr)) ? "" : "]")); \
		first_of = FALSE; \
	    } \
	} while (0)

	propnames_CASE(ITEM_FIRE, "fire", "fire", "fire resistance");
	propnames_CASE(ITEM_FROST, "cold", "frost", "cold resistance");
	/* draining weapons are prefixed with "thirsty" elsewhere */
	propnames_CASE(ITEM_DRLI, "drain", NULL, "drain resistance");
	/* "vorpal" is prefix-only */
	propnames_CASE(ITEM_REFLECTION, "refl", NULL, "reflection");
	propnames_CASE(ITEM_SPEED, "spd", NULL, "speed");
	/* "oilskin" is prefix-only */
	propnames_CASE(ITEM_POWER, "pow", NULL, "power");
	propnames_CASE(ITEM_DEXTERITY, "dex", NULL, "dexterity");
	propnames_CASE(ITEM_BRILLIANCE, "bril", NULL, "brilliance");
	propnames_CASE(ITEM_ESP, "ESP", NULL, "telepathy");
	propnames_CASE(ITEM_DISPLACEMENT, "displ", NULL, "displacement");
	propnames_CASE(ITEM_SEARCHING, "srch", NULL, "searching");
	propnames_CASE(ITEM_WARNING, "warn", NULL, "warning");
	propnames_CASE(ITEM_STEALTH, "stlth", NULL, "stealth");
	propnames_CASE(ITEM_FUMBLING, "fmbl", NULL, "fumbling");
	propnames_CASE(ITEM_CLAIRVOYANCE, "clrv", NULL, "clairvoyance");
	propnames_CASE(ITEM_DETONATIONS, "detn", NULL, "detonation");
	propnames_CASE(ITEM_HUNGER, "hungr", NULL, "hunger");
	propnames_CASE(ITEM_AGGRAVATE, "aggr", NULL, "aggravation");

#undef propnames_CASE
}


/* basic first look at the object; this used to be part of xname.
 * examining an object this way happens automatically and does not involve
 * touching (no stoning) */
void examine_object(struct obj *obj)
{
	int typ = obj->otyp;
	struct objclass *ocl = &objects[typ];
	int nn = ocl->oc_name_known;
	
	/* clean up known when it's tied to oc_name_known, eg after AD_DRIN */
	if (!nn && ocl->oc_uses_known && ocl->oc_unique) obj->known = 0;
	if (!Blind) obj->dknown = TRUE;
	if (Role_if (PM_PRIEST)) obj->bknown = TRUE;

	/* everybody can sense these for now, otherwise these will be
	 * ignored like +5 armor hidden amongst tons of +0 junk */
	if (obj->oprops & ITEM_PROP_MASK)
	    obj->oprops_known |= ITEM_MAGICAL;
}


char *xname(const struct obj *obj)
{
    return xname2(obj, FALSE);
}


char *xname_single(const struct obj *obj)
{
    return xname2(obj, TRUE);
}


static char *xname2(const struct obj *obj, boolean ignore_oquan)
{
	char *buf;
	int typ = obj->otyp;
	struct objclass *ocl = &objects[typ];
	int nn = ocl->oc_name_known ||
		 /* only reveal level prizes when in sight */
		 (Is_prize(obj) &&
		  (cansee(obj->ox, obj->oy) ||
		   /* even reveal when Sokoban prize only felt */
		   (u.ux == obj->ox && u.uy == obj->oy)));
	const char *actualn = OBJ_NAME(*ocl);
	const char *dn = OBJ_DESCR(*ocl);
	const char *un = ocl->oc_uname;
	boolean known = obj->known;
	boolean dknown = obj->dknown;
	boolean bknown = obj->bknown;
	boolean dump_ID_flag = program_state.gameover;

	buf = nextobuf() + PREFIX;	/* leave room for "17 -3 " */
	if (Role_if (PM_SAMURAI) && Japanese_item_name(typ))
		actualn = Japanese_item_name(typ);

	buf[0] = '\0';
	
	/*
	 * clean up known when it's tied to oc_name_known, eg after AD_DRIN
	 * This is only required for unique objects since the article
	 * printed for the object is tied to the combination of the two
	 * and printing the wrong article gives away information.
	 */
	if (!nn && ocl->oc_uses_known && ocl->oc_unique) known = 0;
	if (!Blind) dknown = TRUE;
	/* needed, otherwise BoH only shows up as "bag" when blind */
	if (Is_prize(obj)) dknown = TRUE;
	if (Role_if (PM_PRIEST)) bknown = TRUE;
	if (obj_is_pname(obj))
	    goto nameit;

	/*
	 * Give "magical" prefix to objects with properties as long as not all of
	 * them are known.
	 */
	if ((obj->oprops_known & ITEM_MAGICAL) && !dump_ID_flag &&
	    (obj->oprops & ~obj->oprops_known))
	    strcat(buf, "magical ");

	/* prizes suppress description of objects detected/remembered from afar */
	if (Is_prize(obj) && !un)
	    strcat(buf, "prize ");

	switch (obj->oclass) {
	    case AMULET_CLASS:
		if (!dknown) {
		    strcat(buf, "amulet");
		} else {
		    if (typ == AMULET_OF_YENDOR ||
			typ == FAKE_AMULET_OF_YENDOR)
			/* each must be identified individually */
			strcat(buf, known ? actualn : dn);
		    else if (nn)
			strcat(buf, actualn);
		    else if (un || Is_prize(obj))
			strcat(buf, "amulet"); /* u-named after props */
		    else
			sprintf(eos(buf), "%s amulet", dn);

		    propnames(buf, obj->oprops, obj->oprops_known, FALSE,
			      !!strstr(buf, " of "));

		    if (!nn && un)
			sprintf(eos(buf), " called %s", un);
		}
		break;
	    case WEAPON_CLASS:
		if (is_poisonable(obj) && obj->opoisoned)
		    strcat(buf, "poisoned ");
		/* fall through */
	    case VENOM_CLASS:
	    case TOOL_CLASS:
		if (typ == LENSES)
		    strcat(buf, "pair of ");

		if (!dknown) {
		    strcat(buf, dn ? dn : actualn);
		} else {
		    if ((obj->oprops & ITEM_DRLI) &&
			((obj->oprops_known & ITEM_DRLI) || dump_ID_flag)) {
			if (obj->oprops_known & ITEM_DRLI)
			    strcat(buf, "thirsty ");
			else
			    strcat(buf, "[thirsty] ");
		    }
		    if ((obj->oprops & ITEM_VORPAL) &&
			((obj->oprops_known & ITEM_VORPAL) || dump_ID_flag)) {
			if (obj->oprops_known & ITEM_VORPAL)
			    strcat(buf, "vorpal ");
			else
			    strcat(buf, "[vorpal] ");
		    }

		    if (nn)
			strcat(buf, actualn);
		    else if (un)
			strcat(buf, dn ? dn : actualn); /* u-named after props */
		    else if (Is_prize(obj) && obj->oclass != WEAPON_CLASS)
			strcat(buf, "tool");
		    else
			strcat(buf, dn ? dn : actualn);

		    /* avoid "pair of lenses and fire" */
		    propnames(buf, obj->oprops, obj->oprops_known, TRUE,
			      !!strstr((typ == LENSES ?
					    strstr(buf, "pair of ") + 8 :
					    buf), " of "));

		    if (!nn && un)
			sprintf(eos(buf), " called %s", un);
		}
		/* If we use an() here we'd have to remember never to use */
		/* it whenever calling doname() or xname(). */
		if (typ == FIGURINE)
		    sprintf(eos(buf), " of a%s %s",
			strchr(vowels,*(mons_mname(&mons[obj->corpsenm]))) ? "n" : "",
			mons_mname(&mons[obj->corpsenm]));
		break;
	    case ARMOR_CLASS:
		if (Is_dragon_scales(typ)) strcat(buf, "set of ");
		if (is_boots(obj) || is_gloves(obj)) strcat(buf, "pair of ");

		if (dknown && (obj->oprops & ITEM_OILSKIN) &&
		    ((obj->oprops_known & ITEM_OILSKIN) || dump_ID_flag)) {
		    if (obj->oprops_known & ITEM_OILSKIN)
			strcat(buf, "oilskin ");
		    else
			strcat(buf, "[oilskin] ");
		}

		if (obj->otyp >= ELVEN_SHIELD && obj->otyp <= ORCISH_SHIELD
				&& !dknown) {
			strcpy(buf, "shield");
			propnames(buf, obj->oprops, obj->oprops_known, FALSE,
				  FALSE);
			break;
		}
		if (obj->otyp == SHIELD_OF_REFLECTION && !dknown) {
			strcpy(buf, "smooth shield");
			propnames(buf, obj->oprops, obj->oprops_known, FALSE,
				  FALSE);
			break;
		}

		if (nn) {
			strcat(buf, actualn);
			/* allow "pair of boots of fire resistance" */
			propnames(buf, obj->oprops, obj->oprops_known, FALSE,
				  !!strstr(actualn, " of "));
		} else if (un || Is_prize(obj)) {
			if (is_boots(obj))
				strcat(buf,"boots");
			else if (is_gloves(obj))
				strcat(buf,"gloves");
			else if (is_cloak(obj))
				strcat(buf,"cloak");
			else if (is_helmet(obj))
				strcat(buf,"helmet");
			else if (is_shield(obj))
				strcat(buf,"shield");
			else
				strcat(buf,"armor");
			if (un) {
				propnames(buf, obj->oprops, obj->oprops_known, FALSE,
					  FALSE);
				strcat(buf, " called ");
				strcat(buf, un);
			}
		} else {
			strcat(buf, dn);
			/* skip "set of"/"pair of" for proper wording of
			 * "set of draken scales of fire resistance" */
			propnames(buf, obj->oprops, obj->oprops_known, FALSE,
				  !!strstr((Is_dragon_scales(typ) ?
						strstr(buf, "set of ") + 7:
					    (is_boots(obj) || is_gloves(obj)) ?
						strstr(buf, "pair of ") + 8:
						buf), " of "));
		}
		break;
	    case FOOD_CLASS:
		if (typ == SLIME_MOLD) {
			struct fruit *f;

			for (f=ffruit; f; f = f->nextf) {
				/* Object naming may occur due to bones trimming,
				 * where fids may be negative. */
				if (abs(f->fid) == obj->spe) {
					strcat(buf, f->fname);
					break;
				}
			}
			if (!f) warning("Bad fruit #%d?", obj->spe);
			break;
		} else if (typ == CREAM_PIE && piday()) {
			strcat(buf, "irrational pie");
			break;
		}

		strcat(buf, actualn);
		if (typ == TIN && known) {
		    if (obj->spe > 0)
			strcat(buf, " of spinach");
		    else if (obj->corpsenm == NON_PM)
		        strcat(buf, "empty tin");
		    else if (vegetarian(&mons[obj->corpsenm]))
			sprintf(eos(buf), " of %s", mons_mname(&mons[obj->corpsenm]));
		    else
			sprintf(eos(buf), " of %s meat", mons_mname(&mons[obj->corpsenm]));
		}
		break;
	    case COIN_CLASS:
	    case CHAIN_CLASS:
		strcat(buf, actualn);
		break;
	    case ROCK_CLASS:
		if (typ == STATUE)
		    sprintf(eos(buf), "%s%s of %s%s",
			(Role_if (PM_ARCHEOLOGIST) && (obj->spe & STATUE_HISTORIC)) ? "historic " : "" ,
			actualn,
			type_is_pname(&mons[obj->corpsenm]) ? "" :
			  (mons[obj->corpsenm].geno & G_UNIQ) ? "the " :
			    (strchr(vowels,*(mons_mname(&mons[obj->corpsenm]))) ?
								"an " : "a "),
			mons_mname(&mons[obj->corpsenm]));
		else strcat(buf, actualn);
		break;
	    case BALL_CLASS:
		sprintf(eos(buf), "%sheavy iron ball",
			(obj->owt > ocl->oc_weight) ? "very " : "");
		break;
	    case POTION_CLASS:
		if (dknown && obj->odiluted)
			strcat(buf, "diluted ");
		if (nn || un || !dknown) {
			strcat(buf, "potion");
			if (!dknown) break;
			if (nn) {
			    strcat(buf, " of ");
			    if (typ == POT_WATER &&
				bknown && (obj->blessed || obj->cursed)) {
				strcat(buf, obj->blessed ? "holy " : "unholy ");
			    }
			    strcat(buf, actualn);
			} else {
				strcat(buf, " called ");
				strcat(buf, un);
			}
		} else {
			if (!Is_prize(obj)) {
			    strcat(buf, dn);
			    strcat(buf, " ");
			}
			strcat(buf, "potion");
		}
		break;
	case SCROLL_CLASS:
		if (!dknown) {
		    strcat(buf, "scroll");
		    break;
		}
		if (nn) {
			strcat(buf, "scroll of ");
			strcat(buf, actualn);
		} else if (un) {
			strcat(buf, "scroll called ");
			strcat(buf, un);
		} else if (Is_prize(obj)) {
			strcat(buf, "scroll");
		} else if (ocl->oc_magic) {
			strcat(buf, "scroll labeled ");
			strcat(buf, dn);
		} else {
			strcat(buf, dn);
			strcat(buf, " scroll");
		}
		break;
	case WAND_CLASS:
		if (!dknown)
			strcat(buf, "wand");
		else if (nn)
			sprintf(eos(buf), "wand of %s", actualn);
		else if (un)
			sprintf(eos(buf), "wand called %s", un);
		else if (Is_prize(obj))
			strcat(buf, "wand");
		else
			sprintf(eos(buf), "%s wand", dn);
		break;
	case SPBOOK_CLASS:
		if (!dknown) {
			strcat(buf, "spellbook");
		} else if (nn) {
			if (typ != SPE_BOOK_OF_THE_DEAD)
			    strcat(buf, "spellbook of ");
			strcat(buf, actualn);
		} else if (un) {
			sprintf(eos(buf), "spellbook called %s", un);
		} else if (Is_prize(obj)) {
			strcat(buf, "spellbook");
		} else {
			sprintf(eos(buf), "%s spellbook", dn);
		}
		break;
	case RING_CLASS:
		if (!dknown) {
		    strcat(buf, "ring");
		} else {
		    if (nn)
			sprintf(eos(buf), "ring of %s", actualn);
		    else if (un)
			strcat(buf, "ring"); /* u-named after props */
		    else if (Is_prize(obj))
			strcat(buf, "ring");
		    else
			sprintf(eos(buf), "%s ring", dn);

		    propnames(buf, obj->oprops, obj->oprops_known, FALSE, nn);

		    if (!nn && un)
			sprintf(eos(buf), " called %s", un);
		}
		break;
	case GEM_CLASS:
	    {
		const char *rock =
			    (ocl->oc_material == MINERAL) ? "stone" : "gem";
		if (!dknown) {
		    strcat(buf, rock);
		} else if (!nn) {
		    if (un) sprintf(eos(buf), "%s called %s", rock, un);
		    else if (Is_prize(obj)) strcat(buf, rock);
		    else sprintf(eos(buf), "%s %s", dn, rock);
		} else {
		    strcat(buf, actualn);
		    if (GemStone(typ)) strcat(buf, " stone");
		}
		break;
	    }
	default:
		sprintf(buf,"glorkum %d %d %d", obj->oclass, typ, obj->spe);
	}
	if (!ignore_oquan && obj->quan != 1L) strcpy(buf, makeplural(buf));

	if (obj->onamelth && dknown) {
		strcat(buf, " named ");
nameit:
		strcat(buf, ONAME(obj));
	}

	if (!strncmpi(buf, "the ", 4)) buf += 4;
	return buf;
}

/* xname() output augmented for multishot missile feedback */
char *mshot_xname(const struct obj *obj)
{
    char tmpbuf[BUFSZ];
    char *onm = xname(obj);

    if (m_shot.n > 1 && m_shot.o == obj->otyp) {
	/* copy xname's result so that we can reuse its return buffer */
	strcpy(tmpbuf, onm);
	/* "the Nth arrow"; value will eventually be passed to an() or
	   The(), both of which correctly handle this "the " prefix */
	sprintf(onm, "the %d%s %s", m_shot.i, ordin(m_shot.i), tmpbuf);
    }

    return onm;
}


/* used for naming "the unique_item" instead of "a unique_item" */
boolean the_unique_obj(const struct obj *obj)
{
    if (!obj->dknown)
	return FALSE;
    else if (obj->otyp == FAKE_AMULET_OF_YENDOR && !obj->known)
	return TRUE;		/* lie */
    else
	return (boolean)(objects[obj->otyp].oc_unique &&
			 (obj->known || obj->otyp == AMULET_OF_YENDOR));
}

static void add_erosion_words(const struct obj *obj, char *prefix,
			      boolean in_final_dump)
{
	boolean iscrys = (obj->otyp == CRYSKNIFE);


	if (!is_damageable(obj) && !iscrys) return;

	/* The only cases where any of these bits do double duty are for
	 * rotted food and diluted potions, which are all not is_damageable().
	 */
	if (obj->oeroded && !iscrys) {
		switch (obj->oeroded) {
			case 2:	strcat(prefix, "very "); break;
			case 3:	strcat(prefix, "thoroughly "); break;
		}			
		strcat(prefix, is_rustprone(obj) ? "rusty " : "burnt ");
	}
	if (obj->oeroded2 && !iscrys) {
		switch (obj->oeroded2) {
			case 2:	strcat(prefix, "very "); break;
			case 3:	strcat(prefix, "thoroughly "); break;
		}			
		strcat(prefix, is_corrodeable(obj) ? "corroded " :
			"rotted ");
	}
	if (obj->oerodeproof && (in_final_dump || obj->rknown))
		sprintf(eos(prefix), "%s%s%s ",
			obj->rknown ? "" : "[",
			iscrys ? "fixed" :
			is_rustprone(obj) ? "rustproof" :
			is_corrodeable(obj) ? "corrodeproof" :	/* "stainless"? */
			is_flammable(obj) ? "fireproof" : "",
			obj->rknown ? "" : "]");
}


static char *doname_base(const struct obj *obj, boolean with_worn, boolean with_price)
{
	boolean ispoisoned = FALSE;
	char prefix[PREFIX];
	char tmpbuf[PREFIX+1];
	/* when we have to add something at the start of prefix instead of the
	 * end (strcat is used on the end)
	 */
	char *tmp, *bp = xname(obj);

	boolean dump_ID_flag = program_state.gameover;
	/* display ID in addition to appearance */
	boolean do_ID = dump_ID_flag && !objects[obj->otyp].oc_name_known;
	boolean do_known = dump_ID_flag && !obj->known;
	boolean do_dknown = dump_ID_flag && !obj->dknown;
	boolean do_bknown = dump_ID_flag && !obj->bknown;

	if (!dump_ID_flag) {
	    /* early exit */
	} else if (exist_artifact(obj->otyp, (tmp = ONAME(obj)))) {
	    if (do_dknown || do_known) {
		sprintf(eos(bp), " [%s]", tmp);
	    }
	    /* if already known as an artifact, don't bother showing the base type */
	} else if (obj->otyp == EGG && obj->corpsenm >= LOW_PM &&
		   !(obj->known || mvitals[obj->corpsenm].mvflags & MV_KNOWS_EGG)) {
	    sprintf(bp, "[%s] egg%s", mons_mname(&mons[obj->corpsenm]),
		    obj->quan > 1 ? "s" : "");
	} else if (do_ID || do_dknown) {
	    char *cp = nextobuf();
	    if (Role_if(PM_SAMURAI) && (tmp = (char*)Japanese_item_name(obj->otyp)))
		strcpy(cp, tmp);
	    else if (obj->otyp == POT_WATER && (obj->blessed || obj->cursed))
		sprintf(cp, "%sholy water", obj->blessed ? "" : "un");
	    else
		strcpy(cp, OBJ_NAME(objects[obj->otyp]));

	    /* hideous post-processing: try to merge ID and appearance naturally
	     * The cases are significant, to avoid matching fruit names.
	     * General rules, barring bugs:
	     *   thing of foo [thing of actual] -> thing of foo [of actual]
	     *     (no such objects)
	     *   foo thing [thing of actual] -> foo thing [of actual]
	     *     eg. square amulet [of strangulation]
	     *   thing of foo [actual thing] -> thing of foo [of actual]
	     *     eg. scroll labeled DUAM XNAHT [of amnesia]
	     *   foo thing [actual thing] -> foo thing [actual]
	     *     eg. mud boots [speed boots]
	     *   thing [thing of actual] -> thing [of actual]
	     *     eg. bag [of holding]
	     *   thing [actual thing] -> [actual] thing
	     *     eg. [wax] candle
	     */
	    switch (obj->oclass) {
	    case COIN_CLASS:
		*cp = '\0';
		break;
	    case AMULET_CLASS:
		if (obj->otyp == AMULET_VERSUS_POISON) cp += sizeof("amulet"); /* versus poison */
		else if (obj->otyp == FAKE_AMULET_OF_YENDOR) *strstr(cp, " of the Amulet of Yendor") = '\0'; /* cheap plastic imitation */
		else if (obj->otyp == AMULET_OF_YENDOR) *cp = '\0'; /* is its own description */
		else cp += sizeof("amulet");
		break;
	    case WEAPON_CLASS:
		if ((tmp = strstr(cp, " dagger"))) *tmp = '\0';
		else if ((tmp = strstr(cp, " bow"))) *tmp = '\0';
		else if ((tmp = strstr(cp, " arrow"))) *tmp = '\0';
		else if ((tmp = strstr(cp, " short sword"))) *tmp = '\0';
		else if ((tmp = strstr(cp, " broadsword"))) *tmp = '\0';
		else if ((tmp = strstr(cp, " spear"))) *tmp = '\0';
		break;
	    case ARMOR_CLASS:
		if (obj->otyp == DWARVISH_CLOAK) strcpy(cp, "dwarvish");
		/* only remove "cloak" if unIDed is already "opera cloak" */
		else if (strstr(bp, "cloak")) {
		    if ((tmp = strstr(cp, " cloak"))) *tmp = '\0'; /* elven */
		    else if (strstr(cp, "cloak of ")) cp += sizeof("cloak"); /* other */
		}
		else if (obj->otyp == LEATHER_GLOVES) strcpy(cp, "leather");
		else if ((tmp = strstr(cp, " gloves"))) *tmp = '\0'; /* other */
		else if ((tmp = strstr(cp, " boots"))) *tmp = '\0';
		else if ((tmp = strstr(cp, " shoes"))) *tmp = '\0'; /* iron */
		else if (strstr(cp, "helm of ")) cp += sizeof("helm");
		else if (strstr(cp, "shield of ")) cp += sizeof("shield"); /* of reflection */
		else if ((tmp = strstr(cp, " shield"))) *tmp = '\0';
		else if ((tmp = strstr(cp, " ring mail"))) *tmp = '\0'; /* orcish */
		else if ((tmp = strstr(cp, " chain mail"))) *tmp = '\0'; /* orcish */
		break;
	    case TOOL_CLASS:
		if ((tmp = strstr(cp, " candle")) ||
		    (tmp = strstr(cp, " horn")) ||
		    (tmp = strstr(cp, " lamp")) ||
		    (tmp = strstr(cp, " flute")) ||
		    (tmp = strstr(cp, " harp")) ||
		    (tmp = strstr(cp, " whistle"))) {
			*tmp = '\0';
			memmove(cp + 1, cp, strlen(cp) + 1);
			*cp = '[';
			strcat(cp, "] ");
			bp = strprepend(bp, cp);
			*cp = '\0';
		}
		else if (strstr(cp, "horn of ")) cp += sizeof("horn"); /* of plenty */
		else if (obj->otyp == LEATHER_DRUM) strcpy(cp, "leather");
		else if (obj->otyp == DRUM_OF_EARTHQUAKE) strcpy(cp, "of earthquake");
		else if ((tmp = strstr(cp, "bag of "))) cp += sizeof("bag");
		break;
	    case GEM_CLASS:
		if (strstr(cp, "worthless piece")) strcpy(cp, "worthless glass");
		break;
	    case VENOM_CLASS:
		/* Technically, this doesn't follow the rules... if anyone cares. */
		if ((tmp = strstr(cp, " venom"))) *tmp = '\0';
		break;
	    }
	    /* end post-processing */

	    /*
	     * Add the true object type name, with extra parentheses.
	     * This prevents things like gauntlets of dexterity with the
	     * stealth property where neither is known appearing like:
	     *
	     *  old gloves [of stealth gauntlets of dexterity]
	     *
	     * Using parentheses makes the above look like:
	     *
	     *  old gloves [of stealth (gauntlets of dexterity)]
	     *
	     * The wand-charge bracket collapser conveniently removes the
	     * parentheses when they aren't needed.
	     *
	     *  old gloves [gauntlets of dexterity]
	     *  old gloves [of stealth] called foo [gauntlets of dexterity]
	     */
	    if (strlen(cp) > 0) {
		if (obj->oclass == POTION_CLASS ||
		    obj->oclass == SCROLL_CLASS ||
		    (obj->oclass == SPBOOK_CLASS &&
		     obj->otyp != SPE_BOOK_OF_THE_DEAD) ||
		    obj->oclass == WAND_CLASS) {
		    /* no object properties on these */
		    sprintf(eos(bp), " [of %s]", cp);
		} else if (obj->oclass == RING_CLASS) {
		    sprintf(eos(bp), " [(of %s)]", cp);
		} else {
		    sprintf(eos(bp), " [(%s)]", cp);
		}
	    }
	} else if (obj->otyp == TIN && do_known) {
	    if (obj->spe > 0)
		strcat(bp, " [of spinach]");
	    else if (obj->corpsenm == NON_PM)
		strcat(bp, " [empty]");
	    else if (vegetarian(&mons[obj->corpsenm]))
		sprintf(eos(bp), " [of %s]", mons_mname(&mons[obj->corpsenm]));
	    else
		sprintf(eos(bp), " [of %s meat]", mons_mname(&mons[obj->corpsenm]));
	} else if (obj->otyp == POT_WATER && (obj->blessed || obj->cursed) &&
		   do_bknown) {
	    sprintf(bp, "potion of [%sholy] water", obj->cursed ? "un" : "");
	}

	/* When using xname, we want "poisoned arrow", and when using
	 * doname, we want "poisoned +0 arrow".  This kludge is about the only
	 * way to do it, at least until someone overhauls xname() and doname(),
	 * combining both into one function taking a parameter.
	 */
	/* must check opoisoned--someone can have a weirdly-named fruit */
	if (!strncmp(bp, "poisoned ", 9) && obj->opoisoned) {
		bp += 9;
		ispoisoned = TRUE;
	}

	if (obj->quan != 1L)
		sprintf(prefix, "%d ", obj->quan);
	else if (obj_is_pname(obj) || the_unique_obj(obj)) {
		if (!strncmpi(bp, "the ", 4))
		    bp += 4;
		strcpy(prefix, "the ");
	} else
		strcpy(prefix, "a ");

#ifdef INVISIBLE_OBJECTS
	if (obj->oinvis) strcat(prefix,"invisible ");
#endif

	if ((obj->bknown || do_bknown) &&
	    obj->oclass != COIN_CLASS &&
	    (obj->otyp != POT_WATER || !objects[POT_WATER].oc_name_known
		|| (!obj->cursed && !obj->blessed))) {
	    /* allow 'blessed clear potion' if we don't know it's holy water;
	     * always allow "uncursed potion of water"
	     */
	    if (obj->cursed)
		strcat(prefix, do_bknown ? "[cursed] " : "cursed ");
	    else if (obj->blessed)
		strcat(prefix, do_bknown ? "[blessed] " : "blessed ");
	    else if (iflags.show_uncursed ||
		((!obj->known || !objects[obj->otyp].oc_charged ||
		      (obj->oclass == ARMOR_CLASS ||
		       obj->oclass == RING_CLASS))
		/* For most items with charges or +/-, if you know how many
		 * charges are left or what the +/- is, then you must have
		 * totally identified the item, so "uncursed" is unneccesary,
		 * because an identified object not described as "blessed" or
		 * "cursed" must be uncursed.
		 *
		 * If the charges or +/- is not known, "uncursed" must be
		 * printed to avoid ambiguity between an item whose curse
		 * status is unknown, and an item known to be uncursed.
		 */
			&& obj->otyp != FAKE_AMULET_OF_YENDOR
			&& obj->otyp != AMULET_OF_YENDOR
			&& !Role_if (PM_PRIEST)))
		strcat(prefix, do_bknown ? "[uncursed] " : "uncursed ");
	}

	if (obj->greased) strcat(prefix, "greased ");

	switch(obj->oclass) {
	case AMULET_CLASS:
		if (with_worn && (obj->owornmask & W_AMUL))
			strcat(bp, " (being worn)");
		break;
	case WEAPON_CLASS:
		if (ispoisoned)
			strcat(prefix, "poisoned ");
plus:
		add_erosion_words(obj, prefix, dump_ID_flag);
		if (obj->known || do_known) {
			sprintf(eos(prefix), "%s%s%s ",
				do_known ? "[" : "",
				sitoa(obj->spe),
				do_known ? "]" : "");
		}
		break;
	case ARMOR_CLASS:
		if (with_worn && (obj->owornmask & W_ARMOR))
			strcat(bp, (obj == uskin) ? " (embedded in your skin)" :
				" (being worn)");
		goto plus;
	case TOOL_CLASS:
		/* weptools already get this done when we go to the +n code */
		if (!is_weptool(obj))
		    add_erosion_words(obj, prefix, dump_ID_flag);
		if (with_worn &&
		    (obj->owornmask & (W_TOOL /* blindfold */
				| W_SADDLE))) {
			strcat(bp, " (being worn)");
			break;
		}
		if (obj->otyp == LEASH && obj->leashmon != 0) {
			strcat(bp, " (in use)");
			break;
		}
		if (is_weptool(obj))
			goto plus;
		if (obj->otyp == CANDELABRUM_OF_INVOCATION) {
			if (!obj->spe)
			    strcpy(tmpbuf, "no");
			else
			    sprintf(tmpbuf, "%d", obj->spe);
			sprintf(eos(bp), " (%s candle%s%s)",
				tmpbuf, plur(obj->spe),
				!obj->lamplit ? " attached" : ", lit");
		} else if (obj->otyp == OIL_LAMP || obj->otyp == MAGIC_LAMP ||
			obj->otyp == BRASS_LANTERN || Is_candle(obj)) {
			if (Is_candle(obj) &&
			    obj->age < 20L * (long)objects[obj->otyp].oc_cost)
				strcat(prefix, "partly used ");
			if (obj->lamplit)
				strcat(bp, " (lit)");
		}
		if (ignitable(obj) && obj->known &&
		    obj->otyp != MAGIC_LAMP && !artifact_light(obj)) {
		    long timeout = obj->lamplit ?
				   report_timer(obj->olev, BURN_OBJECT, obj) :
				   moves;
		    /* obj->age is the fuel remaining when the timer runs out,
		     * so we add it to the remaining timer turns to get
		     * the full fuel count.
		     */
		    sprintf(eos(bp), " (%d:%ld)",
			    obj->recharged, obj->age + (timeout - moves));
		    break;
		}
		if (objects[obj->otyp].oc_charged)
		    goto charges;
		break;
	case SPBOOK_CLASS:
#define MAX_SPELL_STUDY 3 /* spell.c */
		if (dump_ID_flag && obj->spestudied > MAX_SPELL_STUDY / 2)
			strcat(prefix, "[faint] ");
		break;
	case WAND_CLASS:
		add_erosion_words(obj, prefix, dump_ID_flag);
charges:
		if (obj->known || do_known) {
		    sprintf(eos(bp), " %s%u:%d%s",
			    do_known ? "[(" : "(",
			    obj->recharged,
			    obj->spe,
			    do_known ? ")]" : ")");
		}
		break;
	case POTION_CLASS:
		if (obj->otyp == POT_OIL && obj->lamplit)
		    strcat(bp, " (lit)");
		break;
	case RING_CLASS:
		add_erosion_words(obj, prefix, dump_ID_flag);
ring:
		if (with_worn) {
		    if (obj->owornmask & W_RINGR) strcat(bp, " (on right ");
		    if (obj->owornmask & W_RINGL) strcat(bp, " (on left ");
		    if (obj->owornmask & W_RING) {
			strcat(bp, body_part(HAND));
			strcat(bp, ")");
		    }
		}
		if ((obj->known || do_known) && objects[obj->otyp].oc_charged) {
		    sprintf(eos(prefix), "%s%s%s ",
			    do_known ? "[" : "",
			    sitoa(obj->spe),
			    do_known ? "]" : "");
		}
		break;
	case FOOD_CLASS:
		/* eat.c: edibility_prompts() */
		if (obj->otyp == CORPSE &&
		    obj->corpsenm != PM_ACID_BLOB &&
		    obj->corpsenm != PM_LIZARD &&
		    obj->corpsenm != PM_LICHEN) {
			long age = moves - peek_at_iced_corpse_age(obj);
			long bucmod = obj->cursed ? 2 : obj->blessed ? -2 : 0;
			long mayberot = age / 10L + bucmod;
			long surerot = age / 29L + bucmod;
			if (surerot > 5L) strcat(prefix, "very rotten ");
			else if (mayberot > 5L) strcat(prefix, "rotten ");
		}
		if (obj->otyp == CORPSE && obj->odrained) {
		    if (wizard && obj->oeaten < drainlevel(obj))
			strcpy(tmpbuf, "over-drained ");
		    else
			sprintf(tmpbuf, "%sdrained ",
				(obj->oeaten > drainlevel(obj)) ? "partly " : "");
		} else if (obj->oeaten) {
		    strcpy(tmpbuf, "partly eaten ");
		} else {
		    tmpbuf[0] = '\0';
		}
		strcat(prefix, tmpbuf);
		if (obj->otyp == CORPSE) {
		    if (mons[obj->corpsenm].geno & G_UNIQ) {
			sprintf(prefix, "%s%s ",
				(type_is_pname(&mons[obj->corpsenm]) ?
					"" : "the "),
				s_suffix(mons_mname(&mons[obj->corpsenm])));
			strcat(prefix, tmpbuf);
		    } else {
			strcat(prefix, mons_mname(&mons[obj->corpsenm]));
			strcat(prefix, " ");
		    }
		} else if (obj->otyp == EGG) {
		    if (dump_ID_flag && stale_egg(obj))
			strcat(prefix, "[stale] ");
		    if (obj->corpsenm >= LOW_PM &&
			    (obj->known ||
			    mvitals[obj->corpsenm].mvflags & MV_KNOWS_EGG)) {
			strcat(prefix, mons_mname(&mons[obj->corpsenm]));
			strcat(prefix, " ");
			if (obj->spe)
			    strcat(bp, " (laid by you)");
		    }
		}
		if (obj->otyp == MEAT_RING) goto ring;
		break;
	case BALL_CLASS:
	case CHAIN_CLASS:
		add_erosion_words(obj, prefix, dump_ID_flag);
		if (with_worn && (obj->owornmask & W_BALL))
			strcat(bp, " (chained to you)");
			break;
	}

	if (with_worn && (obj->owornmask & W_WEP) && !mrg_to_wielded) {
		if (obj->quan != 1L) {
			strcat(bp, " (wielded)");
		} else {
			const char *hand_s = body_part(HAND);

			if (bimanual(obj)) hand_s = makeplural(hand_s);
			sprintf(eos(bp), " (weapon in %s)", hand_s);
		}
	}
	if (with_worn && (obj->owornmask & W_SWAPWEP)) {
		if (u.twoweap)
			sprintf(eos(bp), " (wielded in other %s)",
				body_part(HAND));
		else
			strcat(bp, " (alternate weapon; not wielded)");
	}
	if (with_worn && (obj->owornmask & W_QUIVER)) strcat(bp, " (in quiver)");
	if (obj->unpaid) {
		xchar ox, oy; 
		long quotedprice = unpaid_cost(obj);
		struct monst *shkp = NULL;

		if (Has_contents(obj) &&
		    get_obj_location(obj, &ox, &oy, BURIED_TOO|CONTAINED_TOO) &&
		    costly_spot(ox, oy) &&
		    (shkp = shop_keeper(level, *in_rooms(level, ox, oy, SHOPBASE))))
			quotedprice += contained_cost(obj, shkp, 0L, FALSE, TRUE);
		sprintf(eos(bp), " (unpaid, %ld %s)",
			quotedprice, currency(quotedprice));
	} else if (with_price) {
		int price = shop_item_cost(obj);
		if (price > 0)
		    sprintf(eos(bp), " (%d %s)", price, currency(price));
	}
	if (!strncmp(prefix, "a ", 2) &&
			(strchr(vowels, *(prefix+2) ? *(prefix+2) : *bp) ||
			 (dump_ID_flag && !strncmp(prefix+2, "[uncursed", 9)))
			&& (*(prefix+2) || (strncmp(bp, "uranium", 7)
				&& strncmp(bp, "unicorn", 7)
				&& strncmp(bp, "eucalyptus", 10)))) {
		strcpy(tmpbuf, prefix);
		strcpy(prefix, "an ");
		strcpy(prefix+3, tmpbuf+2);
	}
	/* merge bracketed attribs
	 * eg. [rustproof] [+1] -> [rustproof +1] */
	tmp = prefix;
	while ((tmp = strstr(tmp, "] ["))) {
		*tmp = ' ';
		memmove(tmp + 1, tmp + 3, strlen(tmp + 3) + 1);
	}
	bp = strprepend(bp, prefix);
	if (obj->otyp != SLIME_MOLD) {
		tmp = bp;
		while ((tmp = strstr(tmp, "] ["))) {
			*tmp = ' ';
			memmove(tmp + 1, tmp + 3, strlen(tmp + 3) + 1);
		}
		/* turn [(n:n)] wand charges into [n:n]
		 * but avoid [(foo of bar) (n:n)] becoming [foo of bar) (n:n] */
		if ((tmp = strstr(bp, "[("))) {
			char *tmp2 = strstr(tmp, ")]");
			char *tmp3 = strchr(tmp, ')');
			if (tmp2 && tmp2 == tmp3) {
				memmove(tmp2, tmp2 + 1, strlen(tmp2 + 1) + 1);
				memmove(tmp + 1, tmp + 2, strlen(tmp + 2) + 1);
			}
		}
	}
	return bp;
}


char *doname(const struct obj *obj)
{
    return doname_base(obj, TRUE, FALSE);
}


char *doname_noworn(const struct obj *obj)
{
    return doname_base(obj, FALSE, FALSE);
}


char *doname_price(const struct obj *obj)
{
    return doname_base(obj, TRUE, TRUE);
}


/* used from invent.c */
boolean not_fully_identified_core(const struct obj *otmp, boolean ignore_bknown)
{
    /* gold doesn't have any interesting attributes [yet?] */
    if (otmp->oclass == COIN_CLASS) return FALSE;	/* always fully ID'd */

    /* check fundamental ID hallmarks first */
    if (!otmp->known || !otmp->dknown || (!ignore_bknown && !otmp->bknown) ||
	    !objects[otmp->otyp].oc_name_known)	/* ?redundant? */
	return TRUE;
    if ((otmp->oprops_known & otmp->oprops & ITEM_PROP_MASK) != otmp->oprops)
	return TRUE;
    if (otmp->oartifact && undiscovered_artifact(otmp->oartifact))
	return TRUE;
    /* otmp->rknown is the only item of interest if we reach here */
       /*
	*  Note:  if a revision ever allows scrolls to become fireproof or
	*  rings to become shockproof, this checking will need to be revised.
	*  `rknown' ID only matters if xname() will provide the info about it.
	*/
    if (otmp->rknown || (otmp->oclass != ARMOR_CLASS &&
			 otmp->oclass != WEAPON_CLASS &&
			 !is_weptool(otmp) &&		    /* (redunant) */
			 otmp->oclass != BALL_CLASS))	    /* (useless) */
	return FALSE;
    else	/* lack of `rknown' only matters for vulnerable objects */
	return (boolean)(is_rustprone(otmp) ||
			 is_corrodeable(otmp) ||
			 is_flammable(otmp));
}


boolean not_fully_identified(const struct obj *otmp)
{
    return not_fully_identified_core(otmp, FALSE);
}

char *corpse_xname(const struct obj *otmp,
		   boolean ignore_oquan)	/* to force singular */
{
	char *nambuf = nextobuf();

	sprintf(nambuf, "%s corpse", mons_mname(&mons[otmp->corpsenm]));

	if (ignore_oquan || otmp->quan < 2)
	    return nambuf;
	else
	    return makeplural(nambuf);
}


/* xname, unless it's a corpse, then corpse_xname(obj, FALSE) */
char *cxname(const struct obj *obj)
{
	if (obj->otyp == CORPSE)
	    return corpse_xname(obj, FALSE);
	return xname(obj);
}


/* xname, unless it's a corpse, then corpse_xname(obj, TRUE)
 * but ignore the quantity in either case */
char *cxname2(const struct obj *obj)
{
	if (obj->otyp == CORPSE)
	    return corpse_xname(obj, TRUE);
	return xname2(obj, TRUE);
}


/* treat an object as fully ID'd when it might be used as reason for death */
char *killer_xname(const struct obj *obj_orig)
{
    struct obj *obj;
    unsigned save_ocknown;
    char *buf, *save_ocuname;
    int osize;

    /* copy the object. */
    osize = sizeof(struct obj) + obj_orig->onamelth + obj_orig->oxlth;
    obj = malloc(osize);
    memcpy(obj, obj_orig, osize);
    
    /* killer name should be more specific than general xname; however, exact
       info like blessed/cursed and rustproof makes things be too verbose */
    obj->known = obj->dknown = 1;
    obj->bknown = obj->rknown = obj->greased = 0;
    /* if character is a priest[ess], bknown will get toggled back on */
    obj->blessed = obj->cursed = 0;
    /* "killed by poisoned <obj>" would be misleading when poison is
       not the cause of death and "poisoned by poisoned <obj>" would
       be redundant when it is, so suppress "poisoned" prefix */
    obj->opoisoned = 0;
    /* strip user-supplied name; artifacts keep theirs */
    if (!obj->oartifact) obj->onamelth = 0;
    /* temporarily identify the type of object */
    save_ocknown = objects[obj->otyp].oc_name_known;
    objects[obj->otyp].oc_name_known = 1;
    save_ocuname = objects[obj->otyp].oc_uname;
    objects[obj->otyp].oc_uname = 0;	/* avoid "foo called bar" */

    buf = xname(obj);
    if (obj->quan == 1L) buf = obj_is_pname(obj) ? the(buf) : an(buf);

    objects[obj->otyp].oc_name_known = save_ocknown;
    objects[obj->otyp].oc_uname = save_ocuname;
    
    free(obj);
    return buf;
}

/*
 * Used if only one of a collection of objects is named (e.g. in eat.c).
 */
const char *singular(struct obj *otmp, char *(*func)(const struct obj*))
{
	long savequan;
	char *nam;

	/* Note: using xname for corpses will not give the monster type */
	if (otmp->otyp == CORPSE && func == xname)
		return corpse_xname(otmp, TRUE);

	savequan = otmp->quan;
	otmp->quan = 1L;
	nam = (*func)(otmp);
	otmp->quan = savequan;
	return nam;
}

char *an(const char *str)
{
	char *buf = nextobuf();

	buf[0] = '\0';

	if (strncmpi(str, "the ", 4) &&
	    strcmp(str, "molten lava") &&
	    strcmp(str, "iron bars") &&
	    strcmp(str, "muddy swamp") &&
	    strcmp(str, "ice")) {
		if (strchr(vowels, *str) &&
		    strncmp(str, "one-", 4) &&
		    strncmp(str, "useful", 6) &&
		    strncmp(str, "unicorn", 7) &&
		    strncmp(str, "uranium", 7) &&
		    strncmp(str, "eucalyptus", 10))
			strcpy(buf, "an ");
		else
			strcpy(buf, "a ");
	}

	strcat(buf, str);
	return buf;
}

char *An(const char *str)
{
	char *tmp = an(str);
	*tmp = highc(*tmp);
	return tmp;
}

/*
 * Prepend "the" if necessary; assumes str is a subject derived from xname.
 * Use type_is_pname() for monster names, not the().  the() is idempotent.
 */
char *the(const char *str)
{
	char *buf = nextobuf();
	boolean insert_the = FALSE;

	if (!strncmpi(str, "the ", 4)) {
	    buf[0] = lowc(*str);
	    strcpy(&buf[1], str+1);
	    return buf;
	} else if (*str < 'A' || *str > 'Z') {
	    /* not a proper name, needs an article */
	    insert_the = TRUE;
	} else {
	    /* Probably a proper name, might not need an article */
	    char *tmp, *named, *called;
	    int l;

	    /* some objects have capitalized adjectives in their names */
	    if (((tmp = strrchr(str, ' ')) || (tmp = strrchr(str, '-'))) &&
	       (tmp[1] < 'A' || tmp[1] > 'Z'))
		insert_the = TRUE;
	    else if (tmp && strchr(str, ' ') < tmp) {	/* has spaces */
		/* it needs an article if the name contains "of" */
		tmp = strstri(str, " of ");
		named = strstri(str, " named ");
		called = strstri(str, " called ");
		if (called && (!named || called < named)) named = called;

		if (tmp && (!named || tmp < named))	/* found an "of" */
		    insert_the = TRUE;
		/* stupid special case: lacks "of" but needs "the" */
		else if (!named && (l = strlen(str)) >= 31 &&
		      !strcmp(&str[l - 31], "Platinum Yendorian Express Card"))
		    insert_the = TRUE;
	    }
	}
	if (insert_the)
	    strcpy(buf, "the ");
	else
	    buf[0] = '\0';
	strcat(buf, str);

	return buf;
}

char *The(const char *str)
{
    char *tmp = the(str);
    *tmp = highc(*tmp);
    return tmp;
}

/* returns "count cxname(otmp)" or just cxname(otmp) if count == 1 */
char *aobjnam(const struct obj *otmp, const char *verb)
{
	char *bp = cxname(otmp);
	char prefix[PREFIX];

	if (otmp->quan != 1L) {
		sprintf(prefix, "%d ", otmp->quan);
		bp = strprepend(bp, prefix);
	}

	if (verb) {
	    strcat(bp, " ");
	    strcat(bp, otense(otmp, verb));
	}
	return bp;
}

/* like aobjnam, but prepend "The", not count, and use xname */
char *Tobjnam(const struct obj *otmp, const char *verb)
{
	char *bp = The(xname(otmp));

	if (verb) {
	    strcat(bp, " ");
	    strcat(bp, otense(otmp, verb));
	}
	return bp;
}

/* return form of the verb (input plural) if xname(otmp) were the subject */
char *otense(const struct obj *otmp, const char *verb)
{
	char *buf;

	/*
	 * verb is given in plural (without trailing s).  Return as input
	 * if the result of xname(otmp) would be plural.  Don't bother
	 * recomputing xname(otmp) at this time.
	 */
	if (!is_plural(otmp))
	    return vtense(NULL, verb);

	buf = nextobuf();
	strcpy(buf, verb);
	return buf;
}

/* various singular words that vtense would otherwise categorize as plural */
static const char * const special_subjs[] = {
	"erinys",
	"manes",		/* this one is ambiguous */
	"Cyclops",
	"Hippocrates",
	"Pelias",
	"aklys",
	"amnesia",
	"paralysis",
	0
};

/* return form of the verb (input plural) for present tense 3rd person subj */
char *vtense(const char *subj, const char *verb)
{
	char *buf = nextobuf();
	int len, ltmp;
	const char *sp, *spot;
	const char * const *spec;

	/*
	 * verb is given in plural (without trailing s).  Return as input
	 * if subj appears to be plural.  Add special cases as necessary.
	 * Many hard cases can already be handled by using otense() instead.
	 * If this gets much bigger, consider decomposing makeplural.
	 * Note: monster names are not expected here (except before corpse).
	 *
	 * special case: allow null sobj to get the singular 3rd person
	 * present tense form so we don't duplicate this code elsewhere.
	 */
	if (subj) {
	    if (!strncmpi(subj, "a ", 2) || !strncmpi(subj, "an ", 3))
		goto sing;
	    spot = NULL;
	    for (sp = subj; (sp = strchr(sp, ' ')) != 0; ++sp) {
		if (!strncmp(sp, " of ", 4) ||
		    !strncmp(sp, " from ", 6) ||
		    !strncmp(sp, " called ", 8) ||
		    !strncmp(sp, " named ", 7) ||
		    !strncmp(sp, " labeled ", 9)) {
		    if (sp != subj) spot = sp - 1;
		    break;
		}
	    }
	    len = (int) strlen(subj);
	    if (!spot) spot = subj + len - 1;

	    /*
	     * plural: anything that ends in 's', but not '*us' or '*ss'.
	     * Guess at a few other special cases that makeplural creates.
	     */
	    if ((*spot == 's' && spot != subj &&
			(*(spot-1) != 'u' && *(spot-1) != 's')) ||
		((spot - subj) >= 4 && !strncmp(spot-3, "eeth", 4)) ||
		((spot - subj) >= 3 && !strncmp(spot-3, "feet", 4)) ||
		((spot - subj) >= 2 && !strncmp(spot-1, "ia", 2)) ||
		((spot - subj) >= 2 && !strncmp(spot-1, "ae", 2))) {
		/* check for special cases to avoid false matches */
		len = (int)(spot - subj) + 1;
		for (spec = special_subjs; *spec; spec++) {
		    ltmp = strlen(*spec);
		    if (len == ltmp && !strncmpi(*spec, subj, len)) goto sing;
		    /* also check for <prefix><space><special_subj>
		       to catch things like "the invisible erinys" */
		    if (len > ltmp && *(spot - ltmp) == ' ' &&
			   !strncmpi(*spec, spot - ltmp + 1, ltmp)) goto sing;
		}

		return strcpy(buf, verb);
	    }
	    /*
	     * 3rd person plural doesn't end in telltale 's';
	     * 2nd person singular behaves as if plural.
	     */
	    if (!strcmpi(subj, "they") || !strcmpi(subj, "you"))
		return strcpy(buf, verb);
	}

 sing:
	len = strlen(verb);
	spot = verb + len - 1;

	if (!strcmp(verb, "are"))
	    strcpy(buf, "is");
	else if (!strcmp(verb, "have"))
	    strcpy(buf, "has");
	else if (strchr("zxs", *spot) ||
		 (len >= 2 && *spot=='h' && strchr("cs", *(spot-1))) ||
		 (len == 2 && *spot == 'o')) {
	    /* Ends in z, x, s, ch, sh; add an "es" */
	    strcpy(buf, verb);
	    strcat(buf, "es");
	} else if (*spot == 'y' && (!strchr(vowels, *(spot-1)))) {
	    /* like "y" case in makeplural */
	    strcpy(buf, verb);
	    strcpy(buf + len - 1, "ies");
	} else {
	    strcpy(buf, verb);
	    strcat(buf, "s");
	}

	return buf;
}

/* capitalized variant of doname() */
char *Doname2(const struct obj *obj)
{
	char *s = doname(obj);

	*s = highc(*s);
	return s;
}

/* returns "your xname(obj)" or "Foobar's xname(obj)" or "the xname(obj)" */
char *yname(const struct obj *obj)
{
	char *outbuf = nextobuf();
	char *s = shk_your(outbuf, obj);	/* assert( s == outbuf ); */
	int space_left = BUFSZ - strlen(s) - sizeof " ";

	return strncat(strcat(s, " "), cxname(obj), space_left);
}

/* capitalized variant of yname() */
char *Yname2(const struct obj *obj)
{
	char *s = yname(obj);

	*s = highc(*s);
	return s;
}

/* returns "your simple_typename(obj->otyp)"
 * or "Foobar's simple_typename(obj->otyp)"
 * or "the simple_typename(obj-otyp)"
 */
char *ysimple_name(const struct obj *obj)
{
	char *outbuf = nextobuf();
	char *s = shk_your(outbuf, obj);	/* assert( s == outbuf ); */
	int space_left = BUFSZ - strlen(s) - sizeof " ";

	return strncat(strcat(s, " "), simple_typename(obj->otyp), space_left);
}

/* capitalized variant of ysimple_name() */
char *Ysimple_name2(const struct obj *obj)
{
	char *s = ysimple_name(obj);

	*s = highc(*s);
	return s;
}

static const char *const wrp[] = {
	"wand", "ring", "potion", "scroll", "gem", "amulet",
	"spellbook", "spell book",
	/* for non-specific wishes */
	"weapon", "armor", "armour", "tool", "food", "comestible",
};
static const char wrpsym[] = {
	WAND_CLASS, RING_CLASS, POTION_CLASS, SCROLL_CLASS, GEM_CLASS,
	AMULET_CLASS, SPBOOK_CLASS, SPBOOK_CLASS,
	WEAPON_CLASS, ARMOR_CLASS, ARMOR_CLASS, TOOL_CLASS, FOOD_CLASS,
	FOOD_CLASS
};


/* Plural routine; chiefly used for user-defined fruits.  We have to try to
 * account for everything reasonable the player has; something unreasonable
 * can still break the code.  However, it's still a lot more accurate than
 * "just add an s at the end", which Rogue uses...
 *
 * Also used for plural monster names ("Wiped out all homunculi.")
 * and body parts.
 *
 * Also misused by muse.c to convert 1st person present verbs to 2nd person.
 */
char *makeplural(const char *oldstr)
{
	/* Note: cannot use strcmpi here -- it'd give MATZot, CAVEMeN,... */
	char *spot;
	char *str = nextobuf();
	const char *excess = NULL;
	int len;

	while (*oldstr==' ') oldstr++;
	if (!*oldstr) {
		warning("plural of null?");
		strcpy(str, "s");
		return str;
	}
	strcpy(str, oldstr);

	/*
	 * Skip changing "pair of" to "pairs of".  According to Webster, usual
	 * English usage is use pairs for humans, e.g. 3 pairs of dancers,
	 * and pair for objects and non-humans, e.g. 3 pair of boots.  We don't
	 * refer to pairs of humans in this game so just skip to the bottom.
	 */
	if (!strncmp(str, "pair of ", 8))
		goto bottom;

	/* Search for common compounds, ex. lump of royal jelly */
	for (spot=str; *spot; spot++) {
		if (!strncmp(spot, " of ", 4)
				/* DYWYPISI: pluralize "dart [of fire]" */
				|| !strncmp(spot, " [of ", 5)
				|| !strncmp(spot, " labeled ", 9)
				|| !strncmp(spot, " called ", 8)
				|| !strncmp(spot, " named ", 7)
				|| !strcmp(spot, " above") /* lurkers above */
				|| !strncmp(spot, " versus ", 8)
				|| !strncmp(spot, " from ", 6)
				|| !strncmp(spot, " in ", 4)
				|| !strncmp(spot, "-in-", 4) /* mother-in-law */
				|| !strncmp(spot, " on ", 4)
				|| !strncmp(spot, " a la ", 6)
				|| !strncmp(spot, " with", 5)	/* " with "? */
				|| !strncmp(spot, " de ", 4)
				|| !strncmp(spot, " d'", 3)
				|| !strncmp(spot, " du ", 4)) {
			excess = oldstr + (int) (spot - str);
			*spot = 0;
			break;
		}
	}
	spot--;
	while (*spot==' ') spot--; /* Strip blanks from end */
	*(spot+1) = 0;
	/* Now spot is the last character of the string */

	len = strlen(str);

	/* Single letters */
	if (len==1 || !letter(*spot)) {
		strcpy(spot+1, "'s");
		goto bottom;
	}

	/* Same singular and plural; mostly Japanese words except for "manes" */
	if ((len == 2 && !strcmp(str, "ya")) ||
	    (len >= 2 && !strcmp(spot-1, "ai")) || /* samurai, Uruk-hai */
	    (len >= 3 && !strcmp(spot-2, " ya")) ||
	    (len >= 4 &&
	     (!strcmp(spot-3, "fish") || !strcmp(spot-3, "tuna") ||
	      !strcmp(spot-3, "deer") || !strcmp(spot-3, "yaki"))) ||
	    (len >= 5 && (!strcmp(spot-4, "sheep") ||
			!strcmp(spot-4, "ninja") ||
			!strcmp(spot-4, "ronin") ||
			!strcmp(spot-4, "shito") ||
			!strcmp(spot-7, "shuriken") ||
			!strcmp(spot-4, "tengu") ||
			!strcmp(spot-4, "manes"))) ||
	    (len >= 6 && !strcmp(spot-5, "ki-rin")) ||
	    (len >= 6 && !strcmp(spot-5, "Nazgul")) ||
	    (len >= 7 && !strcmp(spot-6, "gunyoki")))
		goto bottom;

	/* man/men ("Wiped out all cavemen.") */
	if (len >= 3 && !strcmp(spot-2, "man") &&
			(len<6 || strcmp(spot-5, "shaman")) &&
			(len<5 || strcmp(spot-4, "human"))) {
		*(spot-1) = 'e';
		goto bottom;
	}

	/* tooth/teeth */
	if (len >= 5 && !strcmp(spot-4, "tooth")) {
		strcpy(spot-3, "eeth");
		goto bottom;
	}

	/* knife/knives, etc... */
	if (!strcmp(spot-1, "fe")) {
		strcpy(spot-1, "ves");
		goto bottom;
	} else if (*spot == 'f') {
		if (strchr("lr", *(spot-1)) || strchr(vowels, *(spot-1))) {
			strcpy(spot, "ves");
			goto bottom;
		} else if (len >= 5 && !strncmp(spot-4, "staf", 4)) {
			strcpy(spot-1, "ves");
			goto bottom;
		}
	}

	/* foot/feet (body part) */
	if (len >= 4 && !strcmp(spot-3, "foot")) {
		strcpy(spot-2, "eet");
		goto bottom;
	}

	/* ium/ia (mycelia, baluchitheria) */
	if (len >= 3 && !strcmp(spot-2, "ium")) {
		*(spot--) = (char)0;
		*spot = 'a';
		goto bottom;
	}

	/* algae, larvae, hyphae (another fungus part) */
	if ((len >= 4 && !strcmp(spot-3, "alga")) ||
	    (len >= 5 &&
	     (!strcmp(spot-4, "hypha") || !strcmp(spot-4, "larva")))) {
		strcpy(spot, "ae");
		goto bottom;
	}

	/* fungus/fungi, homunculus/homunculi, but buses, lotuses, wumpuses */
	if (len > 3 && !strcmp(spot-1, "us") &&
	    (len < 5 || (strcmp(spot-4, "lotus") &&
			 (len < 6 || strcmp(spot-5, "wumpus"))))) {
		*(spot--) = (char)0;
		*spot = 'i';
		goto bottom;
	}

	/* vortex/vortices */
	if (len >= 6 && !strcmp(spot-3, "rtex")) {
		strcpy(spot-1, "ices");
		goto bottom;
	}

	/* djinni/djinn (note: also efreeti/efreet) */
	if (len >= 6 && !strcmp(spot-5, "djinni")) {
		*spot = (char)0;
		goto bottom;
	}

	/* mumak/mumakil */
	if (len >= 5 && !strcmp(spot-4, "mumak")) {
		strcpy(spot+1, "il");
		goto bottom;
	}

	/* sis/ses (nemesis) */
	if (len >= 3 && !strcmp(spot-2, "sis")) {
		*(spot-1) = 'e';
		goto bottom;
	}

	/* erinys/erinyes */
	if (len >= 6 && !strcmp(spot-5, "erinys")) {
		strcpy(spot, "es");
		goto bottom;
	}

	/* mouse/mice,louse/lice (not a monster, but possible in food names) */
	if (len >= 5 && !strcmp(spot-3, "ouse") && strchr("MmLl", *(spot-4))) {
		strcpy(spot-3, "ice");
		goto bottom;
	}

	/* matzoh/matzot, possible food name */
	if (len >= 6 && (!strcmp(spot-5, "matzoh")
					|| !strcmp(spot-5, "matzah"))) {
		strcpy(spot-1, "ot");
		goto bottom;
	}
	if (len >= 5 && (!strcmp(spot-4, "matzo")
					|| !strcmp(spot-5, "matza"))) {
		strcpy(spot, "ot");
		goto bottom;
	}

	/* child/children (for wise guys who give their food funny names) */
	if (len >= 5 && !strcmp(spot-4, "child")) {
		strcpy(spot, "dren");
		goto bottom;
	}

	/* note: -eau/-eaux (gateau, bordeau...) */
	/* note: ox/oxen, VAX/VAXen, goose/geese */

	/* Ends in z, x, s, ch, sh; add an "es" */
	if (strchr("zxs", *spot)
			|| (len >= 2 && *spot=='h' && strchr("cs", *(spot-1)))
	/* Kludge to get "tomatoes" and "potatoes" right */
			|| (len >= 4 && !strcmp(spot-2, "ato"))) {
		strcpy(spot+1, "es");
		goto bottom;
	}

	/* Ends in y preceded by consonant (note: also "qu") change to "ies" */
	if (*spot == 'y' &&
	    (!strchr(vowels, *(spot-1)))) {
		strcpy(spot, "ies");
		goto bottom;
	}

	/* Default: append an 's' */
	strcpy(spot+1, "s");

bottom:	if (excess) strcpy(eos(str), excess);
	return str;
}


struct o_range {
	const char *name, oclass;
	int  f_o_range, l_o_range;
};

/* wishable subranges of objects */
static const struct o_range o_ranges[] = {
	{ "bag",	TOOL_CLASS,   SACK,	      BAG_OF_TRICKS },
	{ "lamp",	TOOL_CLASS,   OIL_LAMP,	      MAGIC_LAMP },
	{ "candle",	TOOL_CLASS,   TALLOW_CANDLE,  WAX_CANDLE },
	{ "horn",	TOOL_CLASS,   TOOLED_HORN,    HORN_OF_PLENTY },
	{ "shield",	ARMOR_CLASS,  SMALL_SHIELD,   SHIELD_OF_REFLECTION },
	{ "helm",	ARMOR_CLASS,  ELVEN_LEATHER_HELM, HELM_OF_TELEPATHY },
	{ "gloves",	ARMOR_CLASS,  LEATHER_GLOVES, GAUNTLETS_OF_DEXTERITY },
	{ "gauntlets",	ARMOR_CLASS,  LEATHER_GLOVES, GAUNTLETS_OF_DEXTERITY },
	{ "boots",	ARMOR_CLASS,  LOW_BOOTS,      LEVITATION_BOOTS },
	{ "shoes",	ARMOR_CLASS,  LOW_BOOTS,      IRON_SHOES },
	{ "cloak",	ARMOR_CLASS,  MUMMY_WRAPPING, CLOAK_OF_DISPLACEMENT },
	{ "shirt",	ARMOR_CLASS,  HAWAIIAN_SHIRT, T_SHIRT },
	{ "dragon scales",
			ARMOR_CLASS,  GRAY_DRAGON_SCALES, YELLOW_DRAGON_SCALES },
	{ "dragon scale mail",
			ARMOR_CLASS,  GRAY_DRAGON_SCALE_MAIL, YELLOW_DRAGON_SCALE_MAIL },
	{ "sword",	WEAPON_CLASS, SHORT_SWORD,    KATANA },
	{ "venom",	VENOM_CLASS,  BLINDING_VENOM, ACID_VENOM },
	{ "gray stone",	GEM_CLASS,    LUCKSTONE,      FLINT },
	{ "grey stone",	GEM_CLASS,    LUCKSTONE,      FLINT },
};

#define BSTRCMP(base,ptr,string) ((ptr) < base || strcmp((ptr),string))
#define BSTRCMPI(base,ptr,string) ((ptr) < base || strcmpi((ptr),string))
#define BSTRNCMP(base,ptr,string,num) ((ptr)<base || strncmp((ptr),string,num))
#define BSTRNCMPI(base,ptr,string,num) ((ptr)<base||strncmpi((ptr),string,num))

/*
 * Singularize a string the user typed in; this helps reduce the complexity
 * of readobjnam, and is also used in pager.c to singularize the string
 * for which help is sought.
 */
char *makesingular(const char *oldstr)
{
	char *p, *bp;
	char *str = nextobuf();

	if (!oldstr || !*oldstr) {
		warning("singular of null?");
		str[0] = 0;
		return str;
	}
	strcpy(str, oldstr);
	bp = str;

	while (*bp == ' ') bp++;
	/* find "cloves of garlic", "worthless pieces of blue glass" */
	if ((p = strstri(bp, "s of ")) != 0) {
	    /* but don't singularize "gauntlets", "boots", "Eyes of the.." */
	    if (BSTRNCMPI(bp, p-3, "Eye", 3) &&
		BSTRNCMP(bp, p-4, "boot", 4) &&
		BSTRNCMP(bp, p-8, "gauntlet", 8))
		while ((*p = *(p+1)) != 0) p++;
	    return bp;
	}

	/* remove -s or -es (boxes) or -ies (rubies) */
	p = eos(bp);
	if (p >= bp+1 && p[-1] == 's') {
		if (p >= bp+2 && p[-2] == 'e') {
			if (p >= bp+3 && p[-3] == 'i') {
				if (!BSTRCMP(bp, p-7, "cookies") ||
				   !BSTRCMP(bp, p-4, "pies"))
					goto mins;
				strcpy(p-3, "y");
				return bp;
			}

			/* note: cloves / knives from clove / knife */
			if (!BSTRCMP(bp, p-6, "knives")) {
				strcpy(p-3, "fe");
				return bp;
			}
			if (!BSTRCMP(bp, p-6, "staves")) {
				strcpy(p-3, "ff");
				return bp;
			}
			if (!BSTRCMPI(bp, p-6, "leaves")) {
				strcpy(p-3, "f");
				return bp;
			}
			if (!BSTRCMP(bp, p-8, "vortices")) {
				strcpy(p-4, "ex");
				return bp;
			}

			/* note: nurses, axes but boxes */
			if (!BSTRCMP(bp, p-5, "boxes") ||
			    !BSTRCMP(bp, p-4, "ches")) {
				p[-2] = '\0';
				return bp;
			}

			if (!BSTRCMP(bp, p-6, "gloves") ||
			    !BSTRCMP(bp, p-6, "lenses") ||
			    !BSTRCMP(bp, p-5, "shoes") ||
			    !BSTRCMP(bp, p-6, "scales"))
				return bp;

		} else if (!BSTRCMP(bp, p-5, "boots") ||
			   !BSTRCMP(bp, p-9, "gauntlets") ||
			   !BSTRCMP(bp, p-6, "tricks") ||
			   !BSTRCMP(bp, p-9, "paralysis") ||
			   !BSTRCMP(bp, p-5, "glass") ||
			   !BSTRCMP(bp, p-4, "ness") ||
			   !BSTRCMP(bp, p-14, "shape changers") ||
			   !BSTRCMP(bp, p-15, "detect monsters") ||
			   !BSTRCMPI(bp, p-11, "Aesculapius") || /* staff */
			   !BSTRCMP(bp, p-10, "eucalyptus") ||
			   !BSTRCMP(bp, p-6, "lembas") ||
			   !BSTRCMP(bp, p-9, "iron bars") ||
			   !BSTRCMP(bp, p-5, "aklys") ||
			   !BSTRCMP(bp, p-6, "fungus"))
				return bp;
	mins:
		p[-1] = '\0';

	} else {

		if (!BSTRCMP(bp, p-5, "teeth")) {
			strcpy(p-5, "tooth");
			return bp;
		}

		if (!BSTRCMP(bp, p-5, "fungi")) {
			strcpy(p-5, "fungus");
			return bp;
		}

		/* here we cannot find the plural suffix */
	}
	return bp;
}

/* compare user string against object name string using fuzzy matching */
static boolean wishymatch(const char *u_str,	/* from user, so might be variant spelling */
			  const char *o_str,	/* from objects[], so is in canonical form */
			  boolean retry_inverted)	/* optional extra "of" handling */
{
	/* special case: wizards can wish for traps.  The object is "beartrap"
	 * and the trap is "bear trap", so to let wizards wish for both we
	 * must not fuzzymatch.
	 */
	if (wizard && !strcmp(o_str, "beartrap"))
	    return !strncmpi(o_str, u_str, 8);

	/* ignore spaces & hyphens and upper/lower case when comparing */
	if (fuzzymatch(u_str, o_str, " -", TRUE)) return TRUE;

	if (retry_inverted) {
	    const char *u_of, *o_of;
	    char *p, buf[BUFSZ];

	    /* when just one of the strings is in the form "foo of bar",
	       convert it into "bar foo" and perform another comparison */
	    u_of = strstri(u_str, " of ");
	    o_of = strstri(o_str, " of ");
	    if (u_of && !o_of) {
		strcpy(buf, u_of + 4);
		p = eos(strcat(buf, " "));
		while (u_str < u_of) *p++ = *u_str++;
		*p = '\0';
		return fuzzymatch(buf, o_str, " -", TRUE);
	    } else if (o_of && !u_of) {
		strcpy(buf, o_of + 4);
		p = eos(strcat(buf, " "));
		while (o_str < o_of) *p++ = *o_str++;
		*p = '\0';
		return fuzzymatch(u_str, buf, " -", TRUE);
	    }
	}

	/* [note: if something like "elven speed boots" ever gets added, these
	   special cases should be changed to call wishymatch() recursively in
	   order to get the "of" inversion handling] */
	if (!strncmp(o_str, "dwarvish ", 9)) {
	    if (!strncmpi(u_str, "dwarven ", 8))
		return fuzzymatch(u_str + 8, o_str + 9, " -", TRUE);
	} else if (!strncmp(o_str, "elven ", 6)) {
	    if (!strncmpi(u_str, "elvish ", 7))
		return fuzzymatch(u_str + 7, o_str + 6, " -", TRUE);
	    else if (!strncmpi(u_str, "elfin ", 6))
		return fuzzymatch(u_str + 6, o_str + 6, " -", TRUE);
	} else if (!strcmp(o_str, "aluminum")) {
		/* this special case doesn't really fit anywhere else... */
		/* (note that " wand" will have been stripped off by now) */
	    if (!strcmpi(u_str, "aluminium"))
		return fuzzymatch(u_str + 9, o_str + 8, " -", TRUE);
	}

	return FALSE;
}

/* alternate spellings; if the difference is only the presence or
   absence of spaces and/or hyphens (such as "pickaxe" vs "pick axe"
   vs "pick-axe") then there is no need for inclusion in this list;
   likewise for ``"of" inversions'' ("boots of speed" vs "speed boots") */
static const struct alt_spellings {
	const char *sp;
	int ob;
} spellings[] = {
	{ "pickax", PICK_AXE },
	{ "whip", BULLWHIP },
	{ "saber", SILVER_SABER },
	{ "silver sabre", SILVER_SABER },
	{ "smooth shield", SHIELD_OF_REFLECTION },
	{ "enchant armour", SCR_ENCHANT_ARMOR },
	{ "destroy armour", SCR_DESTROY_ARMOR },
	{ "scroll of enchant armour", SCR_ENCHANT_ARMOR },
	{ "scroll of destroy armour", SCR_DESTROY_ARMOR },
	{ "scroll of teleport", SCR_TELEPORTATION },
	{ "leather armour", LEATHER_ARMOR },
	{ "studded leather armour", STUDDED_LEATHER_ARMOR },
	{ "iron ball", HEAVY_IRON_BALL },
	{ "lantern", BRASS_LANTERN },
	{ "mattock", DWARVISH_MATTOCK },
	{ "amulet of poison resistance", AMULET_VERSUS_POISON },
	{ "stone", ROCK },
	{ "camera", EXPENSIVE_CAMERA },
	{ "tee shirt", T_SHIRT },
	{ "can", TIN },
	{ "can opener", TIN_OPENER },
	{ "kelp", KELP_FROND },
	{ "eucalyptus", EUCALYPTUS_LEAF },
	{ "lembas", LEMBAS_WAFER },
	{ "wolfsbane", SPRIG_OF_WOLFSBANE },
	{ "grapple", GRAPPLING_HOOK },
	{ "helmet of opposite alignment", HELM_OF_OPPOSITE_ALIGNMENT },
	{ "crystall ball", CRYSTAL_BALL },
	{ "crystall orb", CRYSTAL_BALL },
	{ "crystal orb", CRYSTAL_BALL },
	{ "wand of invisibility", WAN_MAKE_INVISIBLE },
	{ "wand of speed", WAN_SPEED_MONSTER },
	{ "wand of teleport", WAN_TELEPORTATION },
	{ "invisibility wand", WAN_MAKE_INVISIBLE }, /* "of" inversions won't work */
	{ "speed wand", WAN_SPEED_MONSTER },
	{ "teleport wand", WAN_TELEPORTATION },
	{ "ring of strength", RIN_GAIN_STRENGTH },
	{ "ring of constitution", RIN_GAIN_CONSTITUTION },
	{ "ring of intelligence", RIN_GAIN_INTELLIGENCE },
	{ "ring of wisdom", RIN_GAIN_WISDOM },
	{ "ring of dexterity", RIN_GAIN_DEXTERITY },
	{ "ring of accuracy", RIN_INCREASE_ACCURACY },
	{ "ring of damage", RIN_INCREASE_DAMAGE },
	{ "strength ring", RIN_GAIN_STRENGTH }, /* "of" inversions won't work */
	{ "constitution ring", RIN_GAIN_CONSTITUTION },
	{ "intelligence ring", RIN_GAIN_INTELLIGENCE },
	{ "wisdom ring", RIN_GAIN_WISDOM },
	{ "dexterity ring", RIN_GAIN_DEXTERITY },
	{ "accuracy ring", RIN_INCREASE_ACCURACY },
	{ "damage ring", RIN_INCREASE_DAMAGE },
	{ NULL, 0 },
};

/*
 * Return something wished for.  Specifying a null pointer for
 * the user request string results in a random object.  Otherwise,
 * if asking explicitly for "nothing" (or "nil") return no_wish;
 * if not an object return &zeroobj; if an error (no matching object),
 * return null.
 * If from_user is false, we're reading from the wizkit, nothing was typed in.
 */
struct obj *readobjnam(char *bp, struct obj *no_wish, boolean from_user)
{
	char *p;
	int i;
	struct obj *otmp;
	int cnt, spe, spesgn, typ, very, rechrg;
	int blessed, uncursed, iscursed, ispoisoned, isgreased, isdrained;
	int magical;
	int eroded, eroded2, erodeproof;
#ifdef INVISIBLE_OBJECTS
	int isinvisible;
#endif
	int halfeaten, halfdrained, mntmp, contents;
	int islit, unlabeled, ishistoric, isdiluted;
	const struct alt_spellings *as = spellings;
	struct fruit *f;
	int ftype = current_fruit;
	char fruitbuf[BUFSZ];
	/* Fruits may not mess up the ability to wish for real objects (since
	 * you can leave a fruit in a bones file and it will be added to
	 * another person's game), so they must be checked for last, after
	 * stripping all the possible prefixes and seeing if there's a real
	 * name in there.  So we have to save the full original name.  However,
	 * it's still possible to do things like "uncursed burnt Alaska",
	 * or worse yet, "2 burned 5 course meals", so we need to loop to
	 * strip off the prefixes again, this time stripping only the ones
	 * possible on food.
	 * We could get even more detailed so as to allow food names with
	 * prefixes that _are_ possible on food, so you could wish for
	 * "2 3 alarm chilis".  Currently this isn't allowed; options.c
	 * automatically sticks 'candied' in front of such names.
	 */

	char oclass;
	char *un, *dn, *actualn;
	const char *name=0;

	long objprops = 0L;

	/* true if object has been found by its actual name */
	boolean found_by_actualn = FALSE;

	cnt = spe = spesgn = typ = very = rechrg =
		blessed = uncursed = iscursed = isdrained = halfdrained =
		magical =
#ifdef INVISIBLE_OBJECTS
		isinvisible =
#endif
		ispoisoned = isgreased = eroded = eroded2 = erodeproof =
		halfeaten = islit = unlabeled = ishistoric = isdiluted = 0;
	mntmp = NON_PM;
#define UNDEFINED 0
#define EMPTY 1
#define SPINACH 2
	contents = UNDEFINED;
	oclass = 0;
	actualn = dn = un = 0;

	if (!bp) goto any;
	/* first, remove extra whitespace they may have typed */
	mungspaces(bp);
	/* allow wishing for "nothing" to preserve wishless conduct...
	   [now requires "wand of nothing" if that's what was really wanted] */
	if (!strcmpi(bp, "nothing") || !strcmpi(bp, "nil") ||
	    !strcmpi(bp, "none")) return no_wish;
	/* save the [nearly] unmodified choice string */
	strcpy(fruitbuf, bp);

	for (;;) {
		int l;

		if (!bp || !*bp) goto any;
		if (!strncmpi(bp, "an ", l=3) ||
		    !strncmpi(bp, "a ", l=2)) {
			cnt = 1;
		} else if (!strncmpi(bp, "the ", l=4)) {
			;	/* just increment `bp' by `l' below */
		} else if (!cnt && digit(*bp) && strcmp(bp, "0")) {
			cnt = atoi(bp);
			while (digit(*bp)) bp++;
			while (*bp == ' ') bp++;
			l = 0;
		} else if (*bp == '+' || *bp == '-') {
			spesgn = (*bp++ == '+') ? 1 : -1;
			spe = atoi(bp);
			while (digit(*bp)) bp++;
			while (*bp == ' ') bp++;
			l = 0;
		} else if (!strncmpi(bp, "blessed ", l=8) ||
			   !strncmpi(bp, "holy ", l=5)) {
			blessed = 1;
		} else if (!strncmpi(bp, "cursed ", l=7) ||
			   !strncmpi(bp, "unholy ", l=7)) {
			iscursed = 1;
		} else if (!strncmpi(bp, "uncursed ", l=9)) {
			uncursed = 1;
		} else if (!strncmpi(bp, "magical ", l=8)) {
			magical = 1;
#ifdef INVISIBLE_OBJECTS
		} else if (!strncmpi(bp, "invisible ", l=10)) {
			isinvisible = 1;
#endif
		} else if (!strncmpi(bp, "rustproof ", l=10) ||
			   !strncmpi(bp, "erodeproof ", l=11) ||
			   !strncmpi(bp, "corrodeproof ", l=13) ||
			   !strncmpi(bp, "fixed ", l=6) ||
			   !strncmpi(bp, "fireproof ", l=10) ||
			   !strncmpi(bp, "rotproof ", l=9)) {
			erodeproof = 1;
		} else if (!strncmpi(bp,"lit ", l=4) ||
			   !strncmpi(bp,"burning ", l=8)) {
			islit = 1;
		} else if (!strncmpi(bp,"unlit ", l=6) ||
			   !strncmpi(bp,"extinguished ", l=13)) {
			islit = 0;
		/* "unlabeled" and "blank" are synonymous */
		} else if (!strncmpi(bp,"unlabeled ", l=10) ||
			   !strncmpi(bp,"unlabelled ", l=11) ||
			   !strncmpi(bp,"blank ", l=6)) {
			unlabeled = 1;
		} else if (!strncmpi(bp, "poisoned ",l=9)
			  || (wizard && !strncmpi(bp, "trapped ",l=8))) {
			ispoisoned=1;
		} else if (!strncmpi(bp, "greased ",l=8)) {
			isgreased=1;
		} else if (!strncmpi(bp, "very ", l=5)) {
			/* very rusted very heavy iron ball */
			very = 1;
		} else if (!strncmpi(bp, "thoroughly ", l=11)) {
			very = 2;
		} else if (!strncmpi(bp, "rusty ", l=6) ||
			   !strncmpi(bp, "rusted ", l=7) ||
			   !strncmpi(bp, "burnt ", l=6) ||
			   !strncmpi(bp, "burned ", l=7)) {
			eroded = 1 + very;
			very = 0;
		} else if (!strncmpi(bp, "corroded ", l=9) ||
			   !strncmpi(bp, "rotted ", l=7)) {
			eroded2 = 1 + very;
			very = 0;
		} else if (!strncmpi(bp, "partly drained ", l=15)) {
			isdrained = 1;
			halfdrained = 1;
		} else if (!strncmpi(bp, "drained ", l=8)) {
			isdrained = 1;
			halfdrained = 0;
		} else if (!strncmpi(bp, "partly eaten ", l=13)) {
			halfeaten = 1;
		} else if (!strncmpi(bp, "historic ", l=9)) {
			ishistoric = 1;
		} else if (!strncmpi(bp, "diluted ", l=8)) {
			isdiluted = 1;
		} else if (!strncmpi(bp, "empty ", l=6)) {
			contents = EMPTY;
		} else if (!strncmpi(bp, "vorpal ", l=7) &&
			   strncmpi(bp, "vorpal blade", 12)) {
			objprops |= ITEM_VORPAL;
		} else if (!strncmpi(bp, "thirsty ", l=8)) {
			objprops |= ITEM_DRLI;
		} else if (!strncmpi(bp, "oilskin ", l=8) &&
			   /*
			    * Skip "oilskin cloak", except for:
			    *  - "oilskin cloak of protection"
			    *  - "oilskin cloak of invisibility"
			    *  - "oilskin cloak of magic resistance"
			    *
			    * Otherwise this function will try to read the
			    * suffix as a property later on and fail.
			    *
			    * "oilskin cloak of displacement" is okay, but
			    * handle it the same for consistency.
			    */
			   (strncmpi(bp, "oilskin cloak", 13) ||
			    !strncmpi(bp+13, " of protection", 14) ||
			    !strncmpi(bp+13, " of invisibility", 16) ||
			    !strncmpi(bp+13, " of magic resistance", 20) ||
			    !strncmpi(bp+13, " of displacement", 16)) &&
			   strncmpi(bp, "oilskin sack", 12)) {
			objprops |= ITEM_OILSKIN;
		} else break;
		bp += l;
	}
	if (!cnt) cnt = 1;		/* %% what with "gems" etc. ? */
	if (strlen(bp) > 1) {
	    if ((p = strrchr(bp, '(')) != 0) {
		if (p > bp && p[-1] == ' ') p[-1] = 0;
		else *p = 0;
		p++;
		if (!strcmpi(p, "lit)")) {
		    islit = 1;
		} else {
		    spe = atoi(p);
		    while (digit(*p)) p++;
		    if (*p == ':') {
			p++;
			rechrg = spe;
			spe = atoi(p);
			while (digit(*p)) p++;
		    }
		    if (*p != ')') {
			spe = rechrg = 0;
		    } else {
			spesgn = 1;
			p++;
			if (*p) strcat(bp, p);
		    }
		}
	    }
	}
/*
   otmp->spe is type schar; so we don't want spe to be any bigger or smaller.
   also, spe should always be positive  -- some cheaters may try to confuse
   atoi()
*/
	if (spe < 0) {
		spesgn = -1;	/* cheaters get what they deserve */
		spe = abs(spe);
	}
	if (spe > SCHAR_LIM)
		spe = SCHAR_LIM;
	if (rechrg < 0 || rechrg > 7) rechrg = 7;	/* recharge_limit */

	/* now we have the actual name, as delivered by xname, say
		green potions called whisky
		scrolls labeled "QWERTY"
		egg
		fortune cookies
		very heavy iron ball named hoei
		wand of wishing
		elven cloak
	*/
	if ((p = strstri(bp, " named ")) != 0) {
		*p = 0;
		name = p+7;
	}
	if ((p = strstri(bp, " called ")) != 0) {
		*p = 0;
		un = p+8;
		/* "helmet called telepathy" is not "helmet" (a specific type)
		 * "shield called reflection" is not "shield" (a general type)
		 */
		for (i = 0; i < SIZE(o_ranges); i++)
		    if (!strcmpi(bp, o_ranges[i].name)) {
			oclass = o_ranges[i].oclass;
			goto srch;
		    }
	}
	if ((p = strstri(bp, " labeled ")) != 0) {
		*p = 0;
		dn = p+9;
	} else if ((p = strstri(bp, " labelled ")) != 0) {
		*p = 0;
		dn = p+10;
	}
	if ((p = strstri(bp, " of spinach")) != 0) {
		*p = 0;
		contents = SPINACH;
	}

	/*
	Skip over "pair of ", "pairs of", "set of" and "sets of".

	Accept "3 pair of boots" as well as "3 pairs of boots". It is valid
	English either way.  See makeplural() for more on pair/pairs.

	We should only double count if the object in question is not
	refered to as a "pair of".  E.g. We should double if the player
	types "pair of spears", but not if the player types "pair of
	lenses".  Luckily (?) all objects that are refered to as pairs
	-- boots, gloves, and lenses -- are also not mergable, so cnt is
	ignored anyway.
	*/
	if (!strncmpi(bp, "pair of ",8)) {
		bp += 8;
		cnt *= 2;
	} else if (cnt > 1 && !strncmpi(bp, "pairs of ",9)) {
		bp += 9;
		cnt *= 2;
	} else if (!strncmpi(bp, "set of ",7)) {
		bp += 7;
	} else if (!strncmpi(bp, "sets of ",8)) {
		bp += 8;
	}

	/*
	 * Find corpse type using "of" (figurine of an orc, tin of orc meat)
	 * Otherwise catch object properties.
	 * Don't check if it's a wand or spellbook.
	 * (avoid "wand/finger of death" confusion).
	 * (ALI "potion of vampire blood" also).
	 * (Add "scroll of fire" to the list).
	 */
	if (!strstri(bp, "wand ")
	 && !strstri(bp, "spellbook ")
	 && !strstri(bp, "scroll ")
	 && !strstri(bp, "scrolls ")
	 && !strstri(bp, "potion ")
	 && !strstri(bp, "potions ")
	 && !strstri(bp, "finger ")) {
	    int l = 0;
	    int of = 4;
	    char *tmpp;

	    p = bp;

	    while (p != NULL) {
		tmpp = strstri(p, " of ");
		if (tmpp) {
		    if ((tmpp - 6 == bp && !strncmpi(tmpp - 6, "amulet", 6)) ||
			(tmpp - 5 == bp && !strncmpi(tmpp - 5, "cloak", 5)) ||
			(tmpp - 6 == bp && !strncmpi(tmpp - 6, "shield", 6)) ||
			(tmpp - 9 == bp && !strncmpi(tmpp - 9, "gauntlets", 9)) ||
			(tmpp - 4 == bp && !strncmpi(tmpp - 4, "helm", 4)) ||
			(tmpp - 4 == bp && !strncmpi(tmpp - 4, "ring", 4))) {
			p = tmpp + 4;
			continue;
		    }
		    of = 4;
		    p = tmpp;
		} else {
		    tmpp = strstri(p, " and ");
		    if (tmpp) {
			of = 5;
			p = tmpp;
		    } else {
			p = NULL;
			break;
		    }
		}

		l = 0;

		if ((mntmp = name_to_mon(p + of)) >= LOW_PM) {
		    *p = '\0';
		    p = NULL;
		} else if (!strncmpi(p + of, "fire", l=4)) {
		    objprops |= ITEM_FIRE;
		} else if (!strncmpi(p + of, "frost", l=5) ||
			   !strncmpi(p + of, "cold", l=4)) {
		    objprops |= ITEM_FROST;
		} else if (!strncmpi(p + of, "drain", l=5)) {
		    objprops |= ITEM_DRLI;
		/* no "of vorpal"; require the prefix instead */
		} else if (!strncmpi(p + of, "reflection", l=10)) {
		    objprops |= ITEM_REFLECTION;
		} else if (!strncmpi(p + of, "speed", l=5)) {
		    objprops |= ITEM_SPEED;
		/* no "of oilskin"; require the prefix instead */
		} else if (!strncmpi(p + of, "power", l=5)) {
		    objprops |= ITEM_POWER;
		} else if (!strncmpi(p + of, "dexterity", l=9)) {
		    objprops |= ITEM_DEXTERITY;
		} else if (!strncmpi(p + of, "brilliance", l=10)) {
		    objprops |= ITEM_BRILLIANCE;
		} else if (!strncmpi(p + of, "telepathy", l=9) ||
			   !strncmpi(p + of, "ESP", l=3)) {
		    objprops |= ITEM_ESP;
		} else if (!strncmpi(p + of, "displacement", l=12)) {
		    objprops |= ITEM_DISPLACEMENT;
		} else if (!strncmpi(p + of, "warning", l=7)) {
		    objprops |= ITEM_WARNING;
		} else if (!strncmpi(p + of, "searching", l=9)) {
		    objprops |= ITEM_SEARCHING;
		} else if (!strncmpi(p + of, "stealth", l=7)) {
		    objprops |= ITEM_STEALTH;
		} else if (!strncmpi(p + of, "fumbling", l=8)) {
		    objprops |= ITEM_FUMBLING;
		} else if (!strncmpi(p + of, "clairvoyance", l=12)) {
		    objprops |= ITEM_CLAIRVOYANCE;
		} else if (!strncmpi(p + of, "detonations", l=11) ||
			   !strncmpi(p + of, "detonation", l=10)) {
		    objprops |= ITEM_DETONATIONS;
		} else if (!strncmpi(p + of, "hunger", l=6)) {
		    objprops |= ITEM_HUNGER;
		} else if (!strncmpi(p + of, "aggravation", l=11)) {
		    objprops |= ITEM_AGGRAVATE;
		} else l = 0;

		if (l > 0)
		    *p = '\0';

		if (p)
		    p += of + l;
	    }
	}

	/* Find corpse type w/o "of" (red dragon scale mail, yeti corpse) */
	if (strncmpi(bp, "samurai sword", 13)) /* not the "samurai" monster! */
	if (strncmpi(bp, "wizard lock", 11)) /* not the "wizard" monster! */
	if (strncmpi(bp, "ninja-to", 8)) /* not the "ninja" rank */
	if (strncmpi(bp, "master key", 10)) /* not the "master" rank */
	if (strncmpi(bp, "Thiefbane", 9)) /* not the "thief" rank */
	if (strncmpi(bp, "Ogresmasher", 11)) /* not the "ogre" monster */
	if (strncmpi(bp, "magenta", 7)) /* not the "mage" rank */
	if (strncmpi(bp, "vampire blood", 13)) /* not the "vampire" monster */
	if (mntmp < LOW_PM && strlen(bp) > 2 &&
	    (mntmp = name_to_mon(bp)) >= LOW_PM) {
		int mntmptoo, mntmplen;	/* double check for rank title */
		char *obp = bp;
		mntmptoo = title_to_mon(bp, NULL, &mntmplen);
		bp += mntmp != mntmptoo ? (int)strlen(mons_mname(&mons[mntmp])) : mntmplen;
		if (*bp == ' ') bp++;
		else if (!strncmpi(bp, "s ", 2)) bp += 2;
		else if (!strncmpi(bp, "es ", 3)) bp += 3;
		else if (!*bp && !actualn && !dn && !un && !oclass) {
		    /* no referent; they don't really mean a monster type */
		    bp = obp;
		    mntmp = NON_PM;
		}
	}

	/* first change to singular if necessary */
	if (*bp) {
		char *sng = makesingular(bp);
		if (strcmp(bp, sng)) {
			if (cnt == 1) cnt = 2;
			strcpy(bp, sng);
		}
	}

	/* Alternate spellings (pick-ax, silver sabre, &c) */
	while (as->sp) {
		if (fuzzymatch(bp, as->sp, " -", TRUE)) {
			typ = as->ob;
			goto typfnd;
		}
		as++;
	}
	/* can't use spellings list for this one due to shuffling */
	if (!strncmpi(bp, "grey spell", 10))
		*(bp + 2) = 'a';

	/* dragon scales - assumes order of dragons */
	if ((!strcmpi(bp, "scales") || !strcmpi(bp, "dragon scales")) &&
			mntmp != NON_PM &&
			mntmp >= PM_GRAY_DRAGON && mntmp <= PM_CHROMATIC_DRAGON) {
		if (mntmp == PM_CHROMATIC_DRAGON && !wizard) {
			/* random scales when asking for chromatic dragon scales */
			typ = rn2(YELLOW_DRAGON_SCALES - GRAY_DRAGON_SCALES) +
			      GRAY_DRAGON_SCALES;
		} else {
			typ = GRAY_DRAGON_SCALES + mntmp - PM_GRAY_DRAGON;
		}
		mntmp = NON_PM;	/* no monster */
		goto typfnd;
	}

	p = eos(bp);
	if (!BSTRCMPI(bp, p-10, "holy water")) {
		typ = POT_WATER;
		if ((p-bp) >= 12 && *(p-12) == 'u')
			iscursed = 1; /* unholy water */
		else blessed = 1;
		goto typfnd;
	}
	if (unlabeled && !BSTRCMPI(bp, p-6, "scroll")) {
		typ = SCR_BLANK_PAPER;
		goto typfnd;
	}
	if (unlabeled && !BSTRCMPI(bp, p-9, "spellbook")) {
		typ = SPE_BLANK_PAPER;
		goto typfnd;
	}
	/*
	 * NOTE: Gold pieces are handled as objects nowadays, and therefore
	 * this section should probably be reconsidered as well as the entire
	 * gold/money concept.  Maybe we want to add other monetary units as
	 * well in the future. (TH)
	 */
	if (!BSTRCMPI(bp, p-10, "gold piece") || !BSTRCMPI(bp, p-7, "zorkmid") ||
	   !strcmpi(bp, "gold") || !strcmpi(bp, "money") ||
	   !strcmpi(bp, "coin") || *bp == GOLD_SYM) {
			if (cnt > 5000 && !wizard) cnt=5000;
		if (cnt < 1) cnt=1;
                otmp = mksobj(level, GOLD_PIECE, FALSE, FALSE);
		otmp->quan = cnt;
                otmp->owt = weight(otmp);
		iflags.botl=1;
		return otmp;
	}
	if (strlen(bp) == 1 &&
	   (i = def_char_to_objclass(*bp)) < MAXOCLASSES && i > ILLOBJ_CLASS
	    && (wizard || i != VENOM_CLASS)) {
		oclass = i;
		goto any;
	}

	/* Search for class names: XXXXX potion, scroll of XXXXX.  Avoid */
	/* false hits on, e.g., rings for "ring mail". */
	if (strncmpi(bp, "enchant ", 8) &&
	   strncmpi(bp, "destroy ", 8) &&
	   strncmpi(bp, "food detection", 14) &&
	   strncmpi(bp, "ring mail", 9) &&
	   strncmpi(bp, "studded leather arm", 19) &&
	   strncmpi(bp, "leather arm", 11) &&
	   strncmpi(bp, "tooled horn", 11) &&
	   strncmpi(bp, "food ration", 11) &&
	   strncmpi(bp, "meat ring", 9)
	)
	for (i = 0; i < (int)(sizeof wrpsym); i++) {
		int j = strlen(wrp[i]);
		if (!strncmpi(bp, wrp[i], j)){
			oclass = wrpsym[i];
			if (oclass != AMULET_CLASS) {
			    bp += j;
			    if (!strncmpi(bp, " of ", 4)) actualn = bp+4;
			    /* else if (*bp) ?? */
			} else
			    actualn = bp;
			goto srch;
		}
		if (!BSTRCMPI(bp, p-j, wrp[i])){
			oclass = wrpsym[i];
			p -= j;
			*p = 0;
			if (p > bp && p[-1] == ' ') p[-1] = 0;
			actualn = dn = bp;
			goto srch;
		}
	}

	/* "grey stone" check must be before general "stone" */
	for (i = 0; i < SIZE(o_ranges); i++)
	    if (!strcmpi(bp, o_ranges[i].name)) {
		typ = rnd_class(o_ranges[i].f_o_range, o_ranges[i].l_o_range);
		goto typfnd;
	    }

	if (!BSTRCMPI(bp, p-6, " stone")) {
		p[-6] = 0;
		oclass = GEM_CLASS;
		dn = actualn = bp;
		goto srch;
	} else if (!strcmpi(bp, "looking glass")) {
		;	/* avoid false hit on "* glass" */
	} else if (!BSTRCMPI(bp, p-6, " glass") || !strcmpi(bp, "glass")) {
		char *g = bp;
		if (strstri(g, "broken")) return NULL;
		if (!strncmpi(g, "worthless ", 10)) g += 10;
		if (!strncmpi(g, "piece of ", 9)) g += 9;
		if (!strncmpi(g, "colored ", 8)) g += 8;
		else if (!strncmpi(g, "coloured ", 9)) g += 9;
		if (!strcmpi(g, "glass")) {	/* choose random color */
			/* 9 different kinds */
			typ = LAST_GEM + rnd(9);
			if (objects[typ].oc_class == GEM_CLASS) goto typfnd;
			else typ = 0;	/* somebody changed objects[]? punt */
		} else {		/* try to construct canonical form */
			char tbuf[BUFSZ];
			strcpy(tbuf, "worthless piece of ");
			strcat(tbuf, g);  /* assume it starts with the color */
			strcpy(bp, tbuf);
		}
	}

	actualn = bp;
	if (!dn) dn = actualn; /* ex. "skull cap" */
srch:
	/* check real names of gems first */
	if (!oclass && actualn) {
	    for (i = bases[GEM_CLASS]; i <= LAST_GEM; i++) {
		const char *zn;

		if ((zn = OBJ_NAME(objects[i])) && !strcmpi(actualn, zn)) {
		    typ = i;
		    goto typfnd;
		}
	    }
	}
	i = oclass ? bases[(int)oclass] : 1;
	while (i < NUM_OBJECTS && (!oclass || objects[i].oc_class == oclass)){
		const char *zn;

		if (actualn && (zn = OBJ_NAME(objects[i])) != 0 &&
			    wishymatch(actualn, zn, TRUE)) {
			typ = i;
			found_by_actualn = TRUE;
			goto typfnd;
		}
		if (dn && (zn = OBJ_DESCR(objects[i])) != 0 &&
			    wishymatch(dn, zn, FALSE)) {
			/* don't match extra descriptions (w/o real name) */
			if (!OBJ_NAME(objects[i])) return NULL;
			typ = i;
			goto typfnd;
		}
		if (un && (zn = objects[i].oc_uname) != 0 &&
			    wishymatch(un, zn, FALSE)) {
			typ = i;
			goto typfnd;
		}
		i++;
	}
	if (actualn) {
		const struct Jitem *j = Japanese_items;
		while (j->item) {
			if (actualn && !strcmpi(actualn, j->name)) {
				typ = j->item;
				goto typfnd;
			}
			j++;
		}
	}
	if (!strcmpi(bp, "spinach")) {
		contents = SPINACH;
		typ = TIN;
		goto typfnd;
	}
	/* Note: not strncmpi.  2 fruits, one capital, one not, are possible. */
	{
	    char *fp;
	    int l, cntf;
	    int blessedf, iscursedf, uncursedf, halfeatenf;

	    blessedf = iscursedf = uncursedf = halfeatenf = 0;
	    cntf = 0;

	    fp = fruitbuf;
	    for (;;) {
		if (!fp || !*fp) break;
		if (!strncmpi(fp, "an ", l=3) ||
		    !strncmpi(fp, "a ", l=2)) {
			cntf = 1;
		} else if (!cntf && digit(*fp)) {
			cntf = atoi(fp);
			while (digit(*fp)) fp++;
			while (*fp == ' ') fp++;
			l = 0;
		} else if (!strncmpi(fp, "blessed ", l=8)) {
			blessedf = 1;
		} else if (!strncmpi(fp, "cursed ", l=7)) {
			iscursedf = 1;
		} else if (!strncmpi(fp, "uncursed ", l=9)) {
			uncursedf = 1;
		} else if (!strncmpi(fp, "partly eaten ", l=13)) {
			halfeatenf = 1;
		} else break;
		fp += l;
	    }

	    for (f=ffruit; f; f = f->nextf) {
		char *f1 = f->fname, *f2 = makeplural(f->fname);

		if (!strncmp(fp, f1, strlen(f1)) ||
					!strncmp(fp, f2, strlen(f2))) {
			typ = SLIME_MOLD;
			blessed = blessedf;
			iscursed = iscursedf;
			uncursed = uncursedf;
			halfeaten = halfeatenf;
			cnt = cntf;
			ftype = f->fid;
			goto typfnd;
		}
	    }
	    
	    /* make it possible to wish for "fruit" */
	    if (!strcmpi(bp, "fruit")) {
		    typ = SLIME_MOLD;
		    goto typfnd;
	    }
	}

	if (!oclass && actualn) {
	    short objtyp;

	    /* Perhaps it's an artifact specified by name, not type */
	    name = artifact_name(actualn, &objtyp);
	    if (name) {
		typ = objtyp;
		goto typfnd;
	    }
	}

	/* Let wizards wish for traps --KAA */
	/* must come after objects check so wizards can still wish for
	 * trap objects like beartraps
	 */
	if (wizard && from_user) {
		int trap;

		for (trap = NO_TRAP+1; trap < TRAPNUM; trap++) {
			const char *tname;

			tname = trapexplain[trap-1];
			if (!strncmpi(tname, bp, strlen(tname))) {
				/* avoid stupid mistakes */
				if ((trap == TRAPDOOR || trap == HOLE)
				      && !can_fall_thru(level)) trap = ROCKTRAP;
				maketrap(level, u.ux, u.uy, trap);
				pline("%s.", An(tname));
				return &zeroobj;
			}
		}
		/* or some other dungeon features -dlc */
		p = eos(bp);
		if (!BSTRCMP(bp, p-8, "fountain")) {
			level->locations[u.ux][u.uy].typ = FOUNTAIN;
			level->flags.nfountains++;
			if (!strncmpi(bp, "magic ", 6))
				level->locations[u.ux][u.uy].blessedftn = 1;
			pline("A %sfountain.",
			      level->locations[u.ux][u.uy].blessedftn ? "magic " : "");
			newsym(u.ux, u.uy);
			return &zeroobj;
		}
		if (!BSTRCMP(bp, p-6, "throne")) {
			level->locations[u.ux][u.uy].typ = THRONE;
			pline("A throne.");
			newsym(u.ux, u.uy);
			return &zeroobj;
		}
		if (!BSTRCMP(bp, p-11, "magic chest")) {
			level->locations[u.ux][u.uy].typ = MAGIC_CHEST;
			pline("A magic chest.");
			newsym(u.ux, u.uy);
			return &zeroobj;
		}
		if (!BSTRCMP(bp, p-4, "sink")) {
			level->locations[u.ux][u.uy].typ = SINK;
			level->flags.nsinks++;
			pline("A sink.");
			newsym(u.ux, u.uy);
			return &zeroobj;
		}
		if (!BSTRCMP(bp, p-4, "pool")) {
			level->locations[u.ux][u.uy].typ = POOL;
			del_engr_at(level, u.ux, u.uy);
			pline("A pool.");
			/* Must manually make kelp! */
			water_damage(level->objects[u.ux][u.uy], FALSE, TRUE);
			newsym(u.ux, u.uy);
			return &zeroobj;
		}
		if (!BSTRCMP(bp, p-4, "lava")) {  /* also matches "molten lava" */
			level->locations[u.ux][u.uy].typ = LAVAPOOL;
			del_engr_at(level, u.ux, u.uy);
			pline("A pool of molten lava.");
			if (!(Levitation || Flying)) lava_effects();
			newsym(u.ux, u.uy);
			return &zeroobj;
		}
		if (!BSTRCMP(bp, p-5, "swamp")) {
			level->locations[u.ux][u.uy].typ = BOG;
			del_engr_at(level, u.ux, u.uy);
			pline("A muddy swamp.");
			if (!(Levitation || Flying || Wwalking)) swamp_effects();
			newsym(u.ux, u.uy);
			return &zeroobj;
		}

		if (!BSTRCMP(bp, p-5, "altar")) {
		    aligntyp al;

		    level->locations[u.ux][u.uy].typ = ALTAR;
		    if (!strncmpi(bp, "chaotic ", 8))
			al = A_CHAOTIC;
		    else if (!strncmpi(bp, "neutral ", 8))
			al = A_NEUTRAL;
		    else if (!strncmpi(bp, "lawful ", 7))
			al = A_LAWFUL;
		    else if (!strncmpi(bp, "unaligned ", 10))
			al = A_NONE;
		    else /* -1 - A_CHAOTIC, 0 - A_NEUTRAL, 1 - A_LAWFUL */
			al = (!rn2(6)) ? A_NONE : rn2((int)A_LAWFUL+2) - 1;
		    level->locations[u.ux][u.uy].altarmask = Align2amask( al );
		    pline("%s altar.", An(align_str(al)));
		    newsym(u.ux, u.uy);
		    return &zeroobj;
		}

		if (!BSTRCMP(bp, p-5, "grave") || !BSTRCMP(bp, p-9, "headstone")) {
		    make_grave(level, u.ux, u.uy, NULL);
		    pline("A grave.");
		    newsym(u.ux, u.uy);
		    return &zeroobj;
		}

		if (!BSTRCMP(bp, p-9, "dead tree")) {
		    level->locations[u.ux][u.uy].typ = DEADTREE;
		    pline("A dead tree.");
		    newsym(u.ux, u.uy);
		    block_point(u.ux, u.uy);
		    return &zeroobj;
		}

		if (!BSTRCMP(bp, p-4, "tree")) {
		    level->locations[u.ux][u.uy].typ = TREE;
		    pline("A tree.");
		    newsym(u.ux, u.uy);
		    block_point(u.ux, u.uy);
		    return &zeroobj;
		}

		if (!BSTRCMP(bp, p-4, "bars")) {
		    level->locations[u.ux][u.uy].typ = IRONBARS;
		    pline("Iron bars.");
		    newsym(u.ux, u.uy);
		    return &zeroobj;
		}
	}

	if (!oclass) return NULL;
any:
	if (!oclass) oclass = wrpsym[rn2((int)sizeof(wrpsym))];
typfnd:
	if (typ) oclass = objects[typ].oc_class;

	/* return a random dragon armor if user asks for an unknown
	   dragon armor with its actual name */
	if (typ && from_user && found_by_actualn &&
	    Is_dragon_armor(typ) && !wizard &&
	    !objects[typ].oc_name_known) {
	    typ = rn2(YELLOW_DRAGON_SCALES - GRAY_DRAGON_SCALES);
	    if (Is_dragon_scales(typ))
		typ += GRAY_DRAGON_SCALES;
	    else
		typ += GRAY_DRAGON_SCALE_MAIL;
	}

	/* check for some objects that are not allowed */
	if (typ && objects[typ].oc_unique) {
	   if (!wizard)
	    switch (typ) {
		case AMULET_OF_YENDOR:
		    typ = FAKE_AMULET_OF_YENDOR;
		    break;
		case CANDELABRUM_OF_INVOCATION:
		    typ = rnd_class(TALLOW_CANDLE, WAX_CANDLE);
		    break;
		case BELL_OF_OPENING:
		    typ = BELL;
		    break;
		case SPE_BOOK_OF_THE_DEAD:
		    typ = SPE_BLANK_PAPER;
		    break;
	    }
	}

	/* catch any other non-wishable objects */
	if (objects[typ].oc_nowish && !wizard)
	    return NULL;

	if (typ) {
		otmp = mksobj(level, typ, TRUE, FALSE);
	} else {
		otmp = mkobj(level, oclass, FALSE);
		if (otmp) typ = otmp->otyp;
	}

	/* strip random properties to prevent tripping the non-magic wish check */
	otmp->oprops = 0L;

	if (islit &&
		(typ == OIL_LAMP || typ == MAGIC_LAMP || typ == BRASS_LANTERN ||
		 Is_candle(otmp) || typ == POT_OIL)) {
	    place_object(otmp, level, u.ux, u.uy);  /* make it viable light source */
	    begin_burn(level, otmp, FALSE);
	    obj_extract_self(otmp);	 /* now release it for caller's use */
	}

	if (cnt > 0 && objects[typ].oc_merge && oclass != SPBOOK_CLASS &&
		(cnt < rnd(6) || wizard ||
		 (cnt <= 7 && Is_candle(otmp)) ||
		 (cnt <= 100 &&
		  ((oclass == WEAPON_CLASS && is_ammo(otmp))
				|| typ == ROCK || is_missile(otmp)))))
			otmp->quan = (long) cnt;

	if (oclass == VENOM_CLASS) otmp->spe = 1;

	if (spesgn == 0) spe = otmp->spe;
	else if (wizard) /* no alteration to spe */ ;
	else if (oclass == ARMOR_CLASS || oclass == WEAPON_CLASS ||
		 is_weptool(otmp) ||
			(oclass==RING_CLASS && objects[typ].oc_charged)) {
		if (spe > rnd(5) && spe > otmp->spe) spe = 0;
		if (spe > 2 && Luck < 0) spesgn = -1;
	} else {
		if (oclass == WAND_CLASS) {
			if (spe > 1 && spesgn == -1) spe = 1;
		} else {
			if (spe > 0 && spesgn == -1) spe = 0;
		}
		if (spe > otmp->spe) spe = otmp->spe;
	}

	if (spesgn == -1) spe = -spe;

	/* set otmp->spe.  This may, or may not, use spe... */
	switch (typ) {
		case TIN: if (contents==EMPTY) {
				otmp->corpsenm = NON_PM;
				otmp->spe = 0;
			} else if (contents==SPINACH) {
				otmp->corpsenm = NON_PM;
				otmp->spe = 1;
			}
			break;
		case SLIME_MOLD: otmp->spe = ftype;
			/* Fall through */
		case SKELETON_KEY: case CHEST: case LARGE_BOX: case IRON_SAFE:
		case HEAVY_IRON_BALL: case IRON_CHAIN: case STATUE:
			/* otmp->cobj already done in mksobj(level, ) */
				break;
		case WAN_WISHING:
			if (!wizard) {
				otmp->spe = (rn2(10) ? -1 : 0);
				break;
			}
			/* fall through, if wizard */
		default: otmp->spe = spe;
	}

	/* set otmp->corpsenm or dragon scale [mail] */
	if (mntmp >= LOW_PM) {
		if (mntmp == PM_LONG_WORM_TAIL) mntmp = PM_LONG_WORM;

		switch (typ) {
		case TIN:
			otmp->spe = 0; /* No spinach */
			if (dead_species(mntmp, FALSE)) {
			    otmp->corpsenm = NON_PM;	/* it's empty */
			} else if (!(mons[mntmp].geno & G_UNIQ) &&
				   !(mvitals[mntmp].mvflags & G_NOCORPSE) &&
				   mons[mntmp].cnutrit != 0) {
			    otmp->corpsenm = mntmp;
			}
			break;
		case CORPSE:
			if (!(mons[mntmp].geno & G_UNIQ) &&
				   !(mvitals[mntmp].mvflags & G_NOCORPSE)) {
			    /* beware of random troll or lizard corpse,
			       or of ordinary one being forced to such */
			    if (otmp->timed) obj_stop_timers(otmp);
			    if (mons[mntmp].msound == MS_GUARDIAN)
			    	otmp->corpsenm = genus(mntmp,1);
			    else
				otmp->corpsenm = mntmp;
			    start_corpse_timeout(otmp);
			}
			break;
		case FIGURINE:
			if (!(mons[mntmp].geno & G_UNIQ)
			    && !is_human(&mons[mntmp]))
				otmp->corpsenm = mntmp;
			break;
		case EGG:
			mntmp = can_be_hatched(mntmp);
			if (mntmp != NON_PM) {
			    otmp->corpsenm = mntmp;
			    if (!dead_species(mntmp, TRUE))
				attach_egg_hatch_timeout(otmp);
			    else
				kill_egg(otmp);
			}
			break;
		case STATUE: otmp->corpsenm = mntmp;
			if (Has_contents(otmp) && verysmall(&mons[mntmp]))
			    delete_contents(otmp);	/* no spellbook */
			otmp->spe = ishistoric ? STATUE_HISTORIC : 0;
			break;
		case SCALE_MAIL:
			/* Dragon mail - depends on the order of objects */
			/*		 & dragons.			 */
			if (mntmp >= PM_GRAY_DRAGON &&
			    mntmp <= PM_CHROMATIC_DRAGON) {
			    if (mntmp == PM_CHROMATIC_DRAGON && !wizard) {
				/* random dsm when asking for chromatic dsm */
				otmp->otyp = rn2(YELLOW_DRAGON_SCALES -
						 GRAY_DRAGON_SCALES) +
					     GRAY_DRAGON_SCALE_MAIL;
			    } else {
				otmp->otyp = GRAY_DRAGON_SCALE_MAIL +
					     mntmp - PM_GRAY_DRAGON;
			    }
			}
			break;
		}
	}

	/* set blessed/cursed -- setting the fields directly is safe
	 * since weight() is called below and addinv() will take care
	 * of luck */
	if (iscursed) {
		curse(otmp);
	} else if (uncursed) {
		otmp->blessed = 0;
		otmp->cursed = (Luck < 0 && !wizard);
	} else if (blessed) {
		otmp->blessed = (Luck >= 0 || wizard);
		otmp->cursed = (Luck < 0 && !wizard);
	} else if (spesgn < 0) {
		curse(otmp);
	}

	/* set random property -- asked for "magical", but nothing specific */
	if (magical && !objprops)
	    create_oprop(level, otmp, TRUE);

#ifdef INVISIBLE_OBJECTS
	if (isinvisible) otmp->oinvis = 1;
#endif

	/* set eroded */
	if (is_damageable(otmp) || otmp->otyp == CRYSKNIFE) {
	    if (eroded && (is_flammable(otmp) || is_rustprone(otmp)))
		    otmp->oeroded = eroded;
	    if (eroded2 && (is_corrodeable(otmp) || is_rottable(otmp)))
		    otmp->oeroded2 = eroded2;

	    /* set erodeproof */
	    if (erodeproof && !eroded && !eroded2)
		    otmp->oerodeproof = (Luck >= 0 || wizard);
	}

	/* set otmp->recharged */
	if (oclass == WAND_CLASS) {
	    /* prevent wishing abuse */
	    if (otmp->otyp == WAN_WISHING && !wizard)
		    rechrg = 1;
	    otmp->recharged = (unsigned)rechrg;
	}

	/* set poisoned */
	if (ispoisoned) {
	    if (is_poisonable(otmp))
		otmp->opoisoned = (Luck >= 0);
	    else if (Is_box(otmp) || typ == TIN)
		otmp->otrapped = 1;
	    else if (oclass == FOOD_CLASS)
		/* try to taint by making it as old as possible */
		otmp->age = 1L;
	}

	if (isgreased) otmp->greased = 1;

	if (isdiluted && otmp->oclass == POTION_CLASS &&
			otmp->otyp != POT_WATER)
		otmp->odiluted = 1;

	if (name) {
		const char *aname;
		short objtyp;

		/* an artifact name might need capitalization fixing */
		aname = artifact_name(name, &objtyp);
		if (aname && objtyp == otmp->otyp) name = aname;

		otmp = oname(otmp, name);
		if (otmp->oartifact) {
			otmp->quan = 1L;
			u.uconduct.wisharti++;	/* KMH, conduct */
		}
	}

	/* more wishing abuse: don't allow wishing for certain artifacts */
	/* and make them pay; charge them for the wish anyway! */
	if ((is_quest_artifact(otmp) ||
	     (otmp->oartifact && otmp->oartifact == ART_THIEFBANE) ||
	     (otmp->oartifact && rn2(nartifact_exist()) > 1)) && !wizard) {
	    artifact_exists(otmp, ONAME(otmp), FALSE);
	    obfree(otmp, NULL);
	    otmp = &zeroobj;
	    pline("For a moment, you feel something in your %s, but it disappears!",
		  makeplural(body_part(HAND)));
	}

	/* restrict and apply object properties; only one unless in wizmode */
	/* object properties aren't allowed on artifacts */
	if (objprops && !otmp->oartifact &&
	    (otmp->oclass == WEAPON_CLASS || is_weptool(otmp) ||
	     otmp->oclass == AMULET_CLASS ||
	     otmp->oclass == RING_CLASS ||
	     otmp->oclass == ARMOR_CLASS) &&
	    /* don't grant properties to objects with their own powers */
	    (wizard ||
	     (!objects[otmp->otyp].oc_oprop &&
	      /* oc_oprop isn't set for these, so check manually */
	      otmp->otyp != GAUNTLETS_OF_POWER &&
	      otmp->otyp != GAUNTLETS_OF_DEXTERITY &&
	      otmp->otyp != HELM_OF_BRILLIANCE &&
	      otmp->otyp != RIN_GAIN_STRENGTH &&
	      otmp->otyp != RIN_GAIN_CONSTITUTION &&
	      otmp->otyp != RIN_GAIN_INTELLIGENCE &&
	      otmp->otyp != RIN_GAIN_WISDOM &&
	      otmp->otyp != RIN_GAIN_DEXTERITY &&
	      otmp->otyp != RIN_INCREASE_ACCURACY &&
	      otmp->otyp != RIN_INCREASE_DAMAGE))) {
	    int item_bit, item_rnd;
	    int objpropcount;

	    /* count properties again after applying restrictions */
	    objprops = restrict_oprops(otmp, objprops);
	    objpropcount = longbits(objprops);

	    if (objpropcount > 0) {
		/* choose random property if more than one was asked for */
		if (!wizard) {
		    item_rnd = rnd(objpropcount);
		    for (item_bit = 0; item_bit < MAX_ITEM_PROPS; item_bit++) {
			if (objprops & (1 << item_bit)) {
			    item_rnd--;
			    if (!item_rnd) break;
			}
		    }
		    if (item_bit < MAX_ITEM_PROPS) {
			objprops &= 1 << item_bit;
			objpropcount = 1;	/* for completeness' sake */
		    } else {
			warning("readobjnam: random oprop selection failed");
		    }
		}
		otmp->oprops = objprops;
	    }
	}

	if (halfeaten && otmp->oclass == FOOD_CLASS) {
		if (otmp->otyp == CORPSE)
			otmp->oeaten = mons[otmp->corpsenm].cnutrit;
		else otmp->oeaten = objects[otmp->otyp].oc_nutrition;
		/* (do this adjustment before setting up object's weight) */
		consume_oeaten(otmp, 1);
	}
	if (isdrained && otmp->otyp == CORPSE && mons[otmp->corpsenm].cnutrit) {
		int amt;
		otmp->odrained = 1;
		amt = mons[otmp->corpsenm].cnutrit - drainlevel(otmp);
		if (halfdrained) {
			amt /= 2;
			if (amt == 0)
				amt++;
		}
		/* (do this adjustment before setting up object's weight) */
		consume_oeaten(otmp, -amt);
	}
	otmp->owt = weight(otmp);
	if (very && otmp->otyp == HEAVY_IRON_BALL) otmp->owt += 160;

	return otmp;
}

int rnd_class(int first, int last)
{
	int i, x, sum=0;

	if (first == last)
	    return first;
	for (i=first; i<=last; i++)
		sum += objects[i].oc_prob;
	if (!sum) /* all zero */
		return first + rn2(last-first+1);
	x = rnd(sum);
	for (i=first; i<=last; i++)
		if (objects[i].oc_prob && (x -= objects[i].oc_prob) <= 0)
			return i;
	return 0;
}

static const char *Japanese_item_name(int i)
{
	const struct Jitem *j = Japanese_items;

	while (j->item) {
		if (i == j->item)
			return j->name;
		j++;
	}
	return NULL;
}

const char *cloak_simple_name(const struct obj *cloak)
{
    if (cloak) {
	switch (cloak->otyp) {
	case ROBE:
	    return "robe";
	case MUMMY_WRAPPING:
	    return "wrapping";
	case ALCHEMY_SMOCK:
	    return (objects[cloak->otyp].oc_name_known &&
			cloak->dknown) ? "smock" : "apron";
	default:
	    break;
	}
    }
    return "cloak";
}

const char *mimic_obj_name(const struct monst *mtmp)
{
	if (mtmp->m_ap_type == M_AP_OBJECT && mtmp->mappearance != STRANGE_OBJECT) {
		int idx = objects[mtmp->mappearance].oc_descr_idx;
		if (mtmp->mappearance == GOLD_PIECE) return "gold";
		return obj_descr[idx].oc_name;
	}
	return "whatcha-may-callit";
}

/*objnam.c*/
