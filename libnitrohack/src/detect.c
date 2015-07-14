/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* DynaHack may be freely redistributed.  See license for details. */

/*
 * Detection routines, including crystal ball, magic mapping, and search
 * command.
 */

#include "hack.h"
#include "artifact.h"

static struct obj *o_in(struct obj*, char);
static struct obj *o_material(struct obj*,unsigned);
static void do_dknown_of(struct obj *);
static boolean check_map_spot(int,int,char,unsigned);
static boolean clear_stale_map(char,unsigned);
static void sense_trap(struct trap *,xchar,xchar,int);
static void show_map_spot(int,int);
static void findone(int,int,void *);
static void openone(int,int,void *);
static const char *level_distance(d_level *);

/* Recursively search obj for an object in class oclass and return 1st found */
static struct obj *o_in(struct obj *obj, char oclass)
{
    struct obj *otmp;
    struct obj *temp;

    if (obj->oclass == oclass) return obj;

    if (Has_contents(obj)) {
	for (otmp = obj->cobj; otmp; otmp = otmp->nobj)
	    if (otmp->oclass == oclass) return otmp;
	    else if (Has_contents(otmp) && (temp = o_in(otmp, oclass)))
		return temp;
    }
    return NULL;
}

/* Recursively search obj for an object made of specified material and return 1st found */
static struct obj *o_material(struct obj *obj, unsigned material)
{
    struct obj* otmp;
    struct obj *temp;

    if (objects[obj->otyp].oc_material == material) return obj;

    if (Has_contents(obj)) {
	for (otmp = obj->cobj; otmp; otmp = otmp->nobj)
	    if (objects[otmp->otyp].oc_material == material) return otmp;
	    else if (Has_contents(otmp) && (temp = o_material(otmp, material)))
		return temp;
    }
    return NULL;
}

static void do_dknown_of(struct obj *obj)
{
    struct obj *otmp;

    obj->dknown = 1;
    if (Has_contents(obj)) {
	for (otmp = obj->cobj; otmp; otmp = otmp->nobj)
	    do_dknown_of(otmp);
    }
}

/* Check whether the location has an outdated object displayed on it. */
static boolean check_map_spot(int x, int y, char oclass, unsigned material)
{
	int memobj;
	struct obj *otmp;
	struct monst *mtmp;

	memobj = level->locations[x][y].mem_obj;
	if (memobj) {
	    /* there's some object shown here */
	    if (oclass == ALL_CLASSES) {
		return ( !(level->objects[x][y] ||     /* stale if nothing here */
			    ((mtmp = m_at(level, x,y)) != 0 && mtmp->minvent)));
	    } else {
		if (material && objects[memobj - 1].oc_material == material) {
			/* the object shown here is of interest because material matches */
			for (otmp = level->objects[x][y]; otmp; otmp = otmp->nexthere)
				if (o_material(otmp, GOLD)) return FALSE;
			/* didn't find it; perhaps a monster is carrying it */
			if ((mtmp = m_at(level, x,y)) != 0) {
				for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
					if (o_material(otmp, GOLD)) return FALSE;
		        }
			/* detection indicates removal of this object from the map */
			return TRUE;
		}
	        if (oclass && objects[memobj - 1].oc_class == oclass) {
			/* the object shown here is of interest because its class matches */
			for (otmp = level->objects[x][y]; otmp; otmp = otmp->nexthere)
				if (o_in(otmp, oclass)) return FALSE;
			/* didn't find it; perhaps a monster is carrying it */
			if ((mtmp = m_at(level, x,y)) != 0) {
				for (otmp = mtmp->minvent; otmp; otmp = otmp->nobj)
					if (o_in(otmp, oclass)) return FALSE;
		        }
			/* detection indicates removal of this object from the map */
			return TRUE;
	        }
	    }
	}
	return FALSE;
}

/*
   When doing detection, remove stale data from the map display (corpses
   rotted away, objects carried away by monsters, etc) so that it won't
   reappear after the detection has completed.  Return true if noticeable
   change occurs.
 */
static boolean clear_stale_map(char oclass, unsigned material)
{
	int zx, zy;
	boolean change_made = FALSE;

	for (zx = 1; zx < COLNO; zx++)
	    for (zy = 0; zy < ROWNO; zy++)
		if (check_map_spot(zx, zy, oclass,material)) {
		    unmap_object(zx, zy);
		    change_made = TRUE;
		}

	return change_made;
}

/* look for gold, on the floor or in monsters' possession */
int gold_detect(struct obj *sobj, boolean *scr_known)
{
    struct obj *obj;
    struct monst *mtmp;
    int uw = u.uinwater;
    struct obj *temp;
    boolean stale;

    *scr_known = stale = clear_stale_map(COIN_CLASS, sobj->blessed ? GOLD : 0);

    /* look for gold carried by monsters (might be in a container) */
    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
    	if (DEADMONSTER(mtmp)) continue;	/* probably not needed in this case but... */
	if (findgold(mtmp->minvent) || monsndx(mtmp->data) == PM_GOLD_GOLEM) {
	    *scr_known = TRUE;
	    goto outgoldmap;	/* skip further searching */
	} else for (obj = mtmp->minvent; obj; obj = obj->nobj)
	    if (sobj->blessed && o_material(obj, GOLD)) {
	    	*scr_known = TRUE;
	    	goto outgoldmap;
	    } else if (o_in(obj, COIN_CLASS)) {
		*scr_known = TRUE;
		goto outgoldmap;	/* skip further searching */
	    }
    }
    
    /* look for gold objects */
    for (obj = level->objlist; obj; obj = obj->nobj) {
	if (sobj->blessed && o_material(obj, GOLD)) {
	    *scr_known = TRUE;
	    if (obj->ox != u.ux || obj->oy != u.uy) goto outgoldmap;
	} else if (o_in(obj, COIN_CLASS)) {
	    *scr_known = TRUE;
	    if (obj->ox != u.ux || obj->oy != u.uy) goto outgoldmap;
	}
    }

    if (!*scr_known) {
	/* no gold found on floor or monster's inventory.
	   adjust message if you have gold in your inventory */
	char buf[BUFSZ];
	if (youmonst.data == &mons[PM_GOLD_GOLEM]) {
		sprintf(buf, "You feel like a million %s!", currency(2L));
	} else if (hidden_gold() || money_cnt(invent))
		strcpy(buf, "You feel worried about your future financial situation.");
	else
		strcpy(buf, "You feel materially poor.");
	strange_feeling(sobj, buf);
	return 1;
    }
    /* only under me - no separate display required */
    if (stale) doredraw();
    pline("You notice some gold between your %s.", makeplural(body_part(FOOT)));
    return 0;

outgoldmap:
    cls();

    u.uinwater = 0;
    /* Discover gold locations. */
    for (obj = level->objlist; obj; obj = obj->nobj) {
    	if (sobj->blessed && (temp = o_material(obj, GOLD))) {
	    if (temp != obj) {
		temp->ox = obj->ox;
		temp->oy = obj->oy;
	    }
	    map_object(temp,1);
	} else if ((temp = o_in(obj, COIN_CLASS))) {
	    if (temp != obj) {
		temp->ox = obj->ox;
		temp->oy = obj->oy;
	    }
	    map_object(temp,1);
	}
    }
    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
    	if (DEADMONSTER(mtmp)) continue;	/* probably overkill here */
	if (findgold(mtmp->minvent) || monsndx(mtmp->data) == PM_GOLD_GOLEM) {
	    struct obj gold;
	    gold = zeroobj;
	    gold.otyp = GOLD_PIECE;
	    gold.ox = mtmp->mx;
	    gold.oy = mtmp->my;
	    map_object(&gold,1);
	} else for (obj = mtmp->minvent; obj; obj = obj->nobj)
	    if (sobj->blessed && (temp = o_material(obj, GOLD))) {
		temp->ox = mtmp->mx;
		temp->oy = mtmp->my;
		map_object(temp,1);
		break;
	    } else if ((temp = o_in(obj, COIN_CLASS))) {
		temp->ox = mtmp->mx;
		temp->oy = mtmp->my;
		map_object(temp,1);
		break;
	    }
    }
    
    newsym(u.ux,u.uy);
    pline("You feel very greedy, and sense gold!");
    exercise(A_WIS, TRUE);
    win_pause_output(P_MAP);
    doredraw();
    u.uinwater = uw;
    if (Underwater) under_water(2);
    if (u.uburied) under_ground(2);
    return 0;
}

/* returns 1 if nothing was detected		*/
/* returns 0 if something was detected		*/
int food_detect(struct obj *sobj, boolean *scr_known)
{
    struct obj *obj;
    struct monst *mtmp;
    int ct = 0, ctu = 0;
    boolean confused = (Confusion || (sobj && sobj->cursed)), stale;
    char oclass = confused ? POTION_CLASS : FOOD_CLASS;
    const char *what = confused ? "something" : "food";
    int uw = u.uinwater;

    stale = clear_stale_map(oclass, 0);

    for (obj = level->objlist; obj; obj = obj->nobj)
	if (o_in(obj, oclass)) {
	    if (obj->ox == u.ux && obj->oy == u.uy) ctu++;
	    else ct++;
	}
    for (mtmp = level->monlist; mtmp && !ct; mtmp = mtmp->nmon) {
	/* no DEADMONSTER(mtmp) check needed since dmons never have inventory */
	for (obj = mtmp->minvent; obj; obj = obj->nobj)
	    if (o_in(obj, oclass)) {
		ct++;
		break;
	    }
    }
    
    if (!ct && !ctu) {
	*scr_known = stale && !confused;
	if (stale) {
	    doredraw();
	    pline("You sense a lack of %s nearby.", what);
	    if (sobj && sobj->blessed) {
		if (!u.uedibility) pline("Your %s starts to tingle.", body_part(NOSE));
		u.uedibility = 1;
	    }
	} else if (sobj) {
	    char buf[BUFSZ];
	    sprintf(buf, "Your %s twitches%s.", body_part(NOSE),
			(sobj->blessed && !u.uedibility) ? " then starts to tingle" : "");
	    if (sobj->blessed && !u.uedibility) {
		boolean savebeginner = flags.beginner;	/* prevent non-delivery of */
		flags.beginner = FALSE;			/* 	message            */
		strange_feeling(sobj, buf);
		flags.beginner = savebeginner;
		u.uedibility = 1;
	    } else
		strange_feeling(sobj, buf);
	}
	return !stale;
    } else if (!ct) {
	*scr_known = TRUE;
	pline("You %s %s nearby.", sobj ? "smell" : "sense", what);
	if (sobj && sobj->blessed) {
		if (!u.uedibility) pline("Your %s starts to tingle.", body_part(NOSE));
		u.uedibility = 1;
	}
    } else {
	struct obj *temp;
	*scr_known = TRUE;
	cls();
	u.uinwater = 0;
	for (obj = level->objlist; obj; obj = obj->nobj)
	    if ((temp = o_in(obj, oclass)) != 0) {
		if (temp != obj) {
		    temp->ox = obj->ox;
		    temp->oy = obj->oy;
		}
		map_object(temp,1);
	    }
	for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
	    /* no DEADMONSTER(mtmp) check needed since dmons never have inventory */
	    for (obj = mtmp->minvent; obj; obj = obj->nobj)
		if ((temp = o_in(obj, oclass)) != 0) {
		    temp->ox = mtmp->mx;
		    temp->oy = mtmp->my;
		    map_object(temp,1);
		    break;	/* skip rest of this monster's inventory */
		}
	newsym(u.ux,u.uy);
	if (sobj) {
	    if (sobj->blessed) {
	    	pline("Your %s %s to tingle and you smell %s.", body_part(NOSE),
	    		u.uedibility ? "continues" : "starts", what);
		u.uedibility = 1;
	    } else
		pline("Your %s tingles and you smell %s.", body_part(NOSE), what);
	}
	else pline("You sense %s.", what);
	win_pause_output(P_MAP);
	exercise(A_WIS, TRUE);
	doredraw();
	u.uinwater = uw;
	if (Underwater) under_water(2);
	if (u.uburied) under_ground(2);
    }
    return 0;
}

/*
 * Used for scrolls, potions, spells, and crystal balls.  Returns:
 *
 *	1 - nothing was detected
 *	0 - something was detected
 */
int object_detect(struct obj *detector, /* object doing the detecting */
		  int class, /* an object class, 0 for all */
		  boolean quiet /* don't output any message */)
{
    int x, y;
    char stuff[BUFSZ];
    int is_cursed = (detector && detector->cursed);
    int do_dknown = (detector && (detector->oclass == POTION_CLASS ||
				    detector->oclass == SPBOOK_CLASS) &&
			detector->blessed);
    int ct = 0, ctu = 0;
    struct obj *obj, *otmp = NULL;
    struct monst *mtmp;
    int uw = u.uinwater;

    if (class < 0 || class >= MAXOCLASSES) {
	warning("object_detect:  illegal class %d", class);
	class = 0;
    }

    if (Hallucination || (Confusion && class == SCROLL_CLASS))
	strcpy(stuff, "something");
    else
    	strcpy(stuff, class ? oclass_names[class] : "objects");

    if (do_dknown) for (obj = invent; obj; obj = obj->nobj) do_dknown_of(obj);

    for (obj = level->objlist; obj; obj = obj->nobj) {
	if (!class || o_in(obj, class)) {
	    if (obj->ox == u.ux && obj->oy == u.uy) ctu++;
	    else ct++;
	}
	if (do_dknown) do_dknown_of(obj);
    }

    for (obj = level->buriedobjlist; obj; obj = obj->nobj) {
	if (!class || o_in(obj, class)) {
	    if (obj->ox == u.ux && obj->oy == u.uy) ctu++;
	    else ct++;
	}
	if (do_dknown) do_dknown_of(obj);
    }

    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
	if (DEADMONSTER(mtmp)) continue;
	for (obj = mtmp->minvent; obj; obj = obj->nobj) {
	    if (!class || o_in(obj, class)) ct++;
	    if (do_dknown) do_dknown_of(obj);
	}
	if ((is_cursed && mtmp->m_ap_type == M_AP_OBJECT &&
	    (!class || class == objects[mtmp->mappearance].oc_class)) ||
	    (findgold(mtmp->minvent) && (!class || class == COIN_CLASS))) {
	    ct++;
	    break;
	}
    }

    if (!clear_stale_map(!class ? ALL_CLASSES : class, 0) && !ct) {
	if (!ctu) {
	    if (detector)
		if (!quiet) strange_feeling(detector, "You feel a lack of something.");
	    return 1;
	}

	if (!quiet) pline("You sense %s nearby.", stuff);
	return 0;
    }

    if (!quiet) cls();

    u.uinwater = 0;
/*
 *	Map all buried objects first.
 */
    for (obj = level->buriedobjlist; obj; obj = obj->nobj)
	if (!class || (otmp = o_in(obj, class))) {
	    if (class) {
		if (otmp != obj) {
		    otmp->ox = obj->ox;
		    otmp->oy = obj->oy;
		}
		map_object(otmp, 1);
	    } else
		map_object(obj, 1);
	}
    /*
     * If we are mapping all objects, map only the top object of a pile or
     * the first object in a monster's inventory.  Otherwise, go looking
     * for a matching object class and display the first one encountered
     * at each location.
     *
     * Objects on the floor override buried objects.
     */
    for (x = 1; x < COLNO; x++)
	for (y = 0; y < ROWNO; y++)
	    for (obj = level->objects[x][y]; obj; obj = obj->nexthere)
		if (!class || (otmp = o_in(obj, class))) {
		    if (class) {
			if (otmp != obj) {
			    otmp->ox = obj->ox;
			    otmp->oy = obj->oy;
			}
			map_object(otmp, 1);
		    } else
			map_object(obj, 1);
		    break;
		}

    /* Objects in the monster's inventory override floor objects. */
    for (mtmp = level->monlist ; mtmp ; mtmp = mtmp->nmon) {
	if (DEADMONSTER(mtmp)) continue;
	for (obj = mtmp->minvent; obj; obj = obj->nobj)
	    if (!class || (otmp = o_in(obj, class))) {
		if (!class)
		    otmp = obj;
		otmp->ox = mtmp->mx;		/* at monster location */
		otmp->oy = mtmp->my;
		map_object(otmp, 1);
		break;
	    }
	/* Allow a mimic to override the detected objects it is carrying. */
	if (is_cursed && mtmp->m_ap_type == M_AP_OBJECT &&
		(!class || class == objects[mtmp->mappearance].oc_class)) {
	    struct obj temp;
	    temp = zeroobj;
	    temp.otyp = mtmp->mappearance;	/* needed for obj_to_glyph() */
	    temp.ox = mtmp->mx;
	    temp.oy = mtmp->my;
	    temp.corpsenm = PM_TENGU;		/* if mimicing a corpse */
	    map_object(&temp, 1);
	} else if (findgold(mtmp->minvent) && (!class || class == COIN_CLASS)) {
	    struct obj gold;
	    gold = zeroobj;
	    gold.otyp = GOLD_PIECE;
	    gold.ox = mtmp->mx;
	    gold.oy = mtmp->my;
	    map_object(&gold, 1);
	}
    }

    newsym(u.ux,u.uy);
    if (!quiet) {
	pline("You detect the %s of %s.", ct ? "presence" : "absence", stuff);
	win_pause_output(P_MAP);
	/*
	 * What are we going to do when the hero does an object detect while blind
	 * and the detected object covers a known pool?
	 */
    }
    doredraw();	/* this will correctly reset vision */

    u.uinwater = uw;
    if (Underwater) under_water(2);
    if (u.uburied) under_ground(2);
    return 0;
}

/*
 * Used by: crystal balls, potions, fountains
 *
 * Returns 1 if nothing was detected.
 * Returns 0 if something was detected.
 */
int monster_detect(struct obj *otmp,	/* detecting object (if any) */
		   int mclass		/* monster class, 0 for all */)
{
    struct monst *mtmp;
    int mcnt = 0;


    /* Note: This used to just check level->monlist for a non-zero value
     * but in versions since 3.3.0 level->monlist can test TRUE due to the
     * presence of dmons, so we have to find at least one
     * with positive hit-points to know for sure.
     */
    for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon)
    	if (!DEADMONSTER(mtmp)) {
		mcnt++;
		break;
	}

    if (!mcnt) {
	if (otmp)
	    strange_feeling(otmp, Hallucination ?
			    "You get the heebie jeebies." :
			    "You feel threatened.");
	return 1;
    } else {
	boolean woken = FALSE;

	cls();
	for (mtmp = level->monlist; mtmp; mtmp = mtmp->nmon) {
	    if (DEADMONSTER(mtmp)) continue;
	    if (!mclass || mtmp->data->mlet == mclass ||
		(mtmp->data == &mons[PM_LONG_WORM] && mclass == S_WORM_TAIL))
		    if (mtmp->mx > 0) {
			dbuf_set(level, mtmp->mx, mtmp->my, NULL, S_unexplored,
				 0, 0, 0, 0, 0,
				 0, dbuf_monid(mtmp), 0, 0);
			/* don't be stingy - display entire worm */
			if (mtmp->data == &mons[PM_LONG_WORM])
			    detect_wsegs(mtmp,0);
		    }
	    if (otmp && otmp->cursed &&
		(mtmp->msleeping || !mtmp->mcanmove)) {
		mtmp->msleeping = mtmp->mfrozen = 0;
		mtmp->mcanmove = 1;
		woken = TRUE;
	    }
	}
	display_self();
	pline("You sense the presence of monsters.");
	if (woken)
	    pline("Monsters sense the presence of you.");
	win_pause_output(P_MAP);
	doredraw();
	if (Underwater) under_water(2);
	if (u.uburied) under_ground(2);
    }
    return 0;
}

static void sense_trap(struct trap *trap, xchar x, xchar y, int src_cursed)
{
    if (Hallucination || src_cursed) {
	struct obj obj;			/* fake object */
	obj = zeroobj;
	if (trap) {
	    obj.ox = trap->tx;
	    obj.oy = trap->ty;
	} else {
	    obj.ox = x;
	    obj.oy = y;
	}
	obj.otyp = (src_cursed) ? GOLD_PIECE : random_object();
	obj.corpsenm = random_monster();	/* if otyp == CORPSE */
	map_object(&obj,1);
    } else if (trap) {
	map_trap(trap,1);
	trap->tseen = 1;
    } else {
	struct trap temp_trap;		/* fake trap */
	temp_trap.tx = x;
	temp_trap.ty = y;
	temp_trap.ttyp = BEAR_TRAP;	/* some kind of trap */
	map_trap(&temp_trap, 1);
    }

}

/* the detections are pulled out so they can	*/
/* also be used in the crystal ball routine	*/
/* returns 1 if nothing was detected		*/
/* returns 0 if something was detected		*/
int trap_detect(struct obj *sobj)
/* sobj is null if crystal ball, *scroll if gold detection scroll */
{
    struct trap *ttmp;
    struct obj *obj;
    int door;
    int uw = u.uinwater;
    boolean found = FALSE;
    coord cc;

    for (ttmp = level->lev_traps; ttmp; ttmp = ttmp->ntrap) {
	if (ttmp->tx != u.ux || ttmp->ty != u.uy)
	    goto outtrapmap;
	else found = TRUE;
    }
    for (obj = level->objlist; obj; obj = obj->nobj) {
	if ((obj->otyp == LARGE_BOX || obj->otyp == CHEST || obj->otyp == IRON_SAFE) &&
		obj->otrapped) {
	    if (obj->ox != u.ux || obj->oy != u.uy)
		goto outtrapmap;
	    else found = TRUE;
	}
    }
    for (door = 0; door < level->doorindex; door++) {
	cc = level->doors[door];
	/* make the door, and its trapped status,
	 * show up on the player's memory */
	if (!sobj || !sobj->cursed) {
	    level->locations[cc.x][cc.y].mem_door_t = 1;
	    map_background(cc.x, cc.y, FALSE);
	}
	if (level->locations[cc.x][cc.y].doormask & D_TRAPPED) {
	    if (cc.x != u.ux || cc.y != u.uy)
		goto outtrapmap;
	    else found = TRUE;
	}
    }
    if (!found) {
	char buf[42];
	sprintf(buf, "Your %s stop itching.", makeplural(body_part(TOE)));
	strange_feeling(sobj,buf);
	return 1;
    }
    /* traps exist, but only under me - no separate display required */
    pline("Your %s itch.", makeplural(body_part(TOE)));
    return 0;
outtrapmap:
    cls();

    u.uinwater = 0;
    for (ttmp = level->lev_traps; ttmp; ttmp = ttmp->ntrap)
	sense_trap(ttmp, 0, 0, sobj && sobj->cursed);

    for (obj = level->objlist; obj; obj = obj->nobj)
	if ((obj->otyp == LARGE_BOX || obj->otyp == CHEST || obj->otyp == IRON_SAFE) &&
		obj->otrapped)
	    sense_trap(NULL, obj->ox, obj->oy, sobj && sobj->cursed);

    for (door = 0; door < level->doorindex; door++) {
	cc = level->doors[door];
	if (level->locations[cc.x][cc.y].doormask & D_TRAPPED)
	    sense_trap(NULL, cc.x, cc.y, sobj && sobj->cursed);
    }

    newsym(u.ux,u.uy);
    pline("You feel %s.", sobj && sobj->cursed ? "very greedy" : "entrapped");
    win_pause_output(P_MAP);
    doredraw();
    u.uinwater = uw;
    if (Underwater) under_water(2);
    if (u.uburied) under_ground(2);
    return 0;
}


static const char *level_distance(d_level *where)
{
    schar ll = depth(&u.uz) - depth(where);
    boolean indun = (u.uz.dnum == where->dnum);

    if (ll < 0) {
	if (ll < (-8 - rn2(3)))
	    if (!indun)	return "far away";
	    else	return "far below";
	else if (ll < -1)
	    if (!indun)	return "away below you";
	    else	return "below you";
	else
	    if (!indun)	return "in the distance";
	    else	return "just below";
    } else if (ll > 0) {
	if (ll > (8 + rn2(3)))
	    if (!indun)	return "far away";
	    else	return "far above";
	else if (ll > 1)
	    if (!indun)	return "away above you";
	    else	return "above you";
	else
	    if (!indun)	return "in the distance";
	    else	return "just above";
    } else
	    if (!indun)	return "in the distance";
	    else	return "near you";
}

static const struct {
    const char *what;
    d_level *where;
} level_detects[] = {
  { "Delphi", &oracle_level },
  { "Medusa's lair", &medusa_level },
  { "a castle", &stronghold_level },
  { "the Wizard of Yendor's tower", &wiz1_level },
};

void use_crystal_ball(struct obj *obj)
{
    char ch;
    int oops;

    if (Blind) {
	pline("Too bad you can't see %s.", the(xname(obj)));
	return;
    }
    oops = (rnd(obj->blessed ? 13 : 20) > ACURR(A_INT) || obj->cursed);
    if (oops && (obj->spe > 0)) {
	switch (rnd((obj->oartifact || obj->blessed || ACURR(A_INT) >= 18) ? 4 : 5)) {
	case 1 : pline("%s too much to comprehend!", Tobjnam(obj, "are"));
	    break;
	case 2 : pline("%s you!", Tobjnam(obj, "confuse"));
	    make_confused(HConfusion + rnd(100),FALSE);
	    break;
	case 3 : if (!resists_blnd(&youmonst)) {
		pline("%s your vision!", Tobjnam(obj, "damage"));
		make_blinded(Blinded + rnd(100),FALSE);
		if (!Blind) pline("Your vision quickly clears.");
	    } else {
		pline("%s your vision.", Tobjnam(obj, "assault"));
		pline("You are unaffected!");
	    }
	    break;
	case 4 : pline("%s your mind!", Tobjnam(obj, "zap"));
	    make_hallucinated(HHallucination + rnd(100),FALSE,0L);
	    break;
	case 5 : pline("%s!", Tobjnam(obj, "explode"));
	    useup(obj);
	    obj = 0;	/* it's gone */
	    losehp(rnd(30), "exploding crystal ball", KILLED_BY_AN);
	    break;
	}
	if (obj) consume_obj_charge(obj, TRUE);
	return;
    }

    if (Hallucination) {
	if (!obj->spe) {
	    pline("All you see is funky %s haze.", hcolor(NULL));
	} else {
	    switch(rnd(6)) {
	    case 1 : pline("You grok some groovy globs of incandescent lava.");
		break;
	    case 2 : pline("Whoa!  Psychedelic colors, %s!",
			   poly_gender() == 1 ? "babe" : "dude");
		break;
	    case 3 : pline("The crystal pulses with sinister %s light!",
				hcolor(NULL));
		break;
	    case 4 : pline("You see goldfish swimming above fluorescent rocks.");
		break;
	    case 5 : pline("You see tiny snowflakes spinning around a miniature farmhouse.");
		break;
	    default: pline("Oh wow... like a kaleidoscope!");
		break;
	    }
	    consume_obj_charge(obj, TRUE);
	}
	return;
    }

    /* read a single character */
    if (flags.verbose) pline("You may look for an object or monster symbol.");
    ch = query_key("What do you look for?", NULL);
    /* Don't filter out ' ' here; it has a use */
    if ((ch != def_monsyms[S_GHOST]) && strchr(quitchars,ch)) { 
	if (flags.verbose) pline("Never mind.");
	return;
    }
    pline("You peer into %s...", the(xname(obj)));
    nomul(-rnd(Free_action ? 2 : 10), "gazing into a crystal ball");
    nomovemsg = "";
    if (obj->spe <= 0)
	pline("The vision is unclear.");
    else {
	int class;
	int ret = 0;

	makeknown(CRYSTAL_BALL);
	consume_obj_charge(obj, TRUE);

	/* special case: accept ']' as synonym for mimic
	 * we have to do this before the def_char_to_objclass check
	 */
	if (ch == DEF_MIMIC_DEF) ch = DEF_MIMIC;

	if ((class = def_char_to_objclass(ch)) != MAXOCLASSES)
		ret = object_detect(NULL, class, FALSE);
	else if ((class = def_char_to_monclass(ch)) != MAXMCLASSES)
		ret = monster_detect(NULL, class);
	else switch(ch) {
		case '^':
		    ret = trap_detect(NULL);
		    break;
		default:
		    {
		    int i = rn2(SIZE(level_detects));
		    pline("You see %s, %s.",
			level_detects[i].what,
			level_distance(level_detects[i].where));
		    }
		    ret = 0;
		    break;
	}

	if (ret) {
	    if (!rn2(100))  /* make them nervous */
		pline("You see the Wizard of Yendor gazing out at you.");
	    else pline("The vision is unclear.");
	}
    }
    return;
}

static void show_map_spot(int x, int y)
{
    struct rm *loc;

    if (Confusion && rn2(7)) return;
    loc = &level->locations[x][y];

    loc->seenv = SVALL;

    /* Secret corridors are found, but not secret doors. */
    if (loc->typ == SCORR) {
	loc->typ = CORR;
	unblock_point(x,y);
    }

    /* if we don't remember an object or trap there, map it */
    if (!loc->mem_obj && !loc->mem_trap) {
	if (level->flags.hero_memory) {
	    magic_map_background(x,y,0);
	    newsym(x,y);			/* show it, if not blocked */
	} else {
	    magic_map_background(x,y,1);	/* display it */
	}
    }
}


void do_mapping(void)
{
    int zx, zy;
    int uw = u.uinwater;

    u.uinwater = 0;
    for (zx = 1; zx < COLNO; zx++)
	for (zy = 0; zy < ROWNO; zy++)
	    show_map_spot(zx, zy);
    exercise(A_WIS, TRUE);
    u.uinwater = uw;
    if (!level->flags.hero_memory || Underwater) {
	flush_screen();			/* flush temp screen */
	win_pause_output(P_MAP);	/* wait */
	doredraw();
    }
}


void do_vicinity_map(void)
{
    int zx, zy;
    int lo_y = (u.uy-5 < 0 ? 0 : u.uy-5),
	hi_y = (u.uy+6 > ROWNO ? ROWNO : u.uy+6),
	lo_x = (u.ux-9 < 1 ? 1 : u.ux-9),	/* avoid column 0 */
	hi_x = (u.ux+10 > COLNO ? COLNO : u.ux+10);

    for (zx = lo_x; zx < hi_x; zx++)
	for (zy = lo_y; zy < hi_y; zy++)
	    show_map_spot(zx, zy);

    if (!level->flags.hero_memory || Underwater) {
	flush_screen();			/* flush temp screen */
	win_pause_output(P_MAP);	/* wait */
	doredraw();
    }
}

/* convert a secret door into a normal door */
void cvt_sdoor_to_door(struct rm *loc, const d_level *dlev)
{
	int newmask = loc->doormask & ~WM_MASK;

	if (Is_rogue_level(dlev))
	    /* rogue didn't have doors, only doorways */
	    newmask = D_NODOOR;
	else
	    /* newly exposed door is closed */
	    if (!(newmask & D_LOCKED)) newmask |= D_CLOSED;

	loc->typ = DOOR;
	loc->doormask = newmask;
}


static void findone(int zx, int zy, void *num)
{
	struct trap *ttmp;
	struct monst *mtmp;

	if (level->locations[zx][zy].typ == SDOOR) {
		cvt_sdoor_to_door(&level->locations[zx][zy], &u.uz); /* .typ = DOOR */
		magic_map_background(zx, zy, 0);
		newsym(zx, zy);
		(*(int*)num)++;
	} else if (level->locations[zx][zy].typ == SCORR) {
		level->locations[zx][zy].typ = CORR;
		unblock_point(zx,zy);
		magic_map_background(zx, zy, 0);
		newsym(zx, zy);
		(*(int*)num)++;
	} else if ((ttmp = t_at(level, zx, zy)) != 0) {
		if (!ttmp->tseen && ttmp->ttyp != STATUE_TRAP) {
			ttmp->tseen = 1;
			newsym(zx,zy);
			(*(int*)num)++;
		}
	} else if ((mtmp = m_at(level, zx, zy)) != 0) {
		if (mtmp->m_ap_type) {
			seemimic(mtmp);
			(*(int*)num)++;
		}
		if (mtmp->mundetected &&
		    (is_hider(mtmp->data) || mtmp->data->mlet == S_EEL)) {
			mtmp->mundetected = 0;
			newsym(zx, zy);
			(*(int*)num)++;
		}
		if (!canspotmon(level, mtmp) && !level->locations[zx][zy].mem_invis)
			map_invisible(zx, zy);
	} else if (level->locations[zx][zy].mem_invis) {
		unmap_object(zx, zy);
		newsym(zx, zy);
		(*(int*)num)++;
	}
}


static void openone(int zx, int zy, void *num)
{
	struct trap *ttmp;
	struct obj *otmp;

	if (OBJ_AT(zx, zy)) {
		for (otmp = level->objects[zx][zy];
				otmp; otmp = otmp->nexthere) {
		    if (Is_box(otmp) && otmp->olocked) {
			otmp->olocked = 0;
			(*(int*)num)++;
		    }
		}
		/* let it fall to the next cases. could be on trap. */
	}
	if (level->locations[zx][zy].typ == SDOOR || (level->locations[zx][zy].typ == DOOR &&
		      (level->locations[zx][zy].doormask & (D_CLOSED|D_LOCKED)))) {
		if (level->locations[zx][zy].typ == SDOOR)
		    cvt_sdoor_to_door(&level->locations[zx][zy], &u.uz); /* .typ = DOOR */
		if (level->locations[zx][zy].doormask & D_TRAPPED) {
		    if (distu(zx, zy) < 3) b_trapped("door", 0);
		    else Norep("You %s an explosion!",
				cansee(zx, zy) ? "see" :
				   (flags.soundok ? "hear" :
						"feel the shock of"));
		    wake_nearto(zx, zy, 11*11);
		    level->locations[zx][zy].doormask = D_NODOOR;
		} else
		    level->locations[zx][zy].doormask = D_ISOPEN;
		unblock_point(zx, zy);
		newsym(zx, zy);
		(*(int*)num)++;
	} else if (level->locations[zx][zy].typ == SCORR) {
		level->locations[zx][zy].typ = CORR;
		unblock_point(zx, zy);
		newsym(zx, zy);
		(*(int*)num)++;
	} else if ((ttmp = t_at(level, zx, zy)) != 0) {
		if (!ttmp->tseen && ttmp->ttyp != STATUE_TRAP) {
		    ttmp->tseen = 1;
		    newsym(zx,zy);
		    (*(int*)num)++;
		}
	} else if (find_drawbridge(&zx, &zy)) {
		/* make sure it isn't an open drawbridge */
		open_drawbridge(zx, zy);
		(*(int*)num)++;
	}
}

int findit(void)	/* returns number of things found */
{
	int num = 0;

	if (u.uswallow) return 0;
	do_clear_area(u.ux, u.uy, BOLT_LIM, findone, &num);
	return num;
}

int openit(void)	/* returns number of things found and opened */
{
	int num = 0;

	if (u.uswallow) {
		if (is_animal(u.ustuck->data)) {
			if (Blind) pline("Its mouth opens!");
			else pline("%s opens its mouth!", Monnam(u.ustuck));
		}
		expels(u.ustuck, u.ustuck->data, TRUE);
		return -1;
	}

	do_clear_area(u.ux, u.uy, BOLT_LIM, openone, &num);
	return num;
}


void find_trap(struct trap *trap)
{
    int tt = what_trap(trap->ttyp);
    boolean cleared = FALSE;

    trap->tseen = 1;
    exercise(A_WIS, TRUE);
    if (Blind)
	feel_location(trap->tx, trap->ty);
    else
	newsym(trap->tx, trap->ty);

    if (level->locations[trap->tx][trap->ty].mem_obj ||
	level->locations[trap->tx][trap->ty].mem_invis) {
    	/* There's too much clutter to see your find otherwise */
	cls();
	map_trap(trap, 1);
	display_self();
	cleared = TRUE;
    }

    pline("You find %s.", an(trapexplain[tt-1]));

    if (cleared) {
	win_pause_output(P_MAP);	/* wait */
	doredraw();
    }
}


int dosearch0(int aflag)
{
	xchar x, y;
	struct trap *trap;
	struct monst *mtmp;

	if (u.uswallow) {
		if (!aflag)
			pline("What are you looking for?  The exit?");
	} else {
	    int fund = (uwep && uwep->oartifact &&
		    spec_ability(uwep, SPFX_SEARCH)) ?
		    uwep->spe : 0;
	    if (ublindf && ublindf->otyp == LENSES && !Blind)
		    fund += 2; /* JDS: lenses help searching */
	    if (fund > 5) fund = 5;
	    for (x = u.ux-1; x < u.ux+2; x++)
	      for (y = u.uy-1; y < u.uy+2; y++) {
		if (!isok(x,y)) continue;
		if (x != u.ux || y != u.uy) {
		    if (Blind && !aflag) feel_location(x,y);
		    if (level->locations[x][y].typ == SDOOR) {
			if (rnl(7-fund)) continue;
			cvt_sdoor_to_door(&level->locations[x][y], &u.uz); /* .typ = DOOR */
			exercise(A_WIS, TRUE);
			nomul(0, NULL);
			if (Blind && !aflag)
			    feel_location(x,y);	/* make sure it shows up */
			else
			    newsym(x,y);
			if (flags.verbose) pline("You find a secret door.");
		    } else if (level->locations[x][y].typ == SCORR) {
			if (rnl(7-fund)) continue;
			level->locations[x][y].typ = CORR;
			unblock_point(x,y);	/* vision */
			exercise(A_WIS, TRUE);
			nomul(0, NULL);
			newsym(x,y);
			if (flags.verbose) pline("You find a secret passage.");
		    } else {
		/* Be careful not to find anything in an SCORR or SDOOR */
			if ((mtmp = m_at(level, x, y)) && !aflag) {
			    if (mtmp->m_ap_type) {
				seemimic(mtmp);
		find:		exercise(A_WIS, TRUE);
				if (!canspotmon(level, mtmp)) {
				    if (level->locations[x][y].mem_invis) {
					/* found invisible monster in a square
					 * which already has an 'I' in it.
					 * Logically, this should still take
					 * time and lead to a return(1), but if
					 * we did that the player would keep
					 * finding the same monster every turn.
					 */
					continue;
				    } else {
					pline("You feel an unseen monster!");
					map_invisible(x, y);
				    }
				} else if (!sensemon(mtmp))
				    pline("You find %s.", a_monnam(mtmp));
				return 1;
			    }
			    if (!canspotmon(level, mtmp)) {
				if (mtmp->mundetected &&
				   (is_hider(mtmp->data) || mtmp->data->mlet == S_EEL))
					mtmp->mundetected = 0;
				newsym(x,y);
				goto find;
			    }
			}

			/* see if an invisible monster has moved--if Blind,
			 * feel_location() already did it
			 */
			if (!aflag && !mtmp && !Blind &&
				    level->locations[x][y].mem_invis) {
			    unmap_object(x,y);
			    newsym(x,y);
			}

			if ((trap = t_at(level, x,y)) && !trap->tseen && !rnl(8)) {
			    nomul(0, NULL);

			    if (trap->ttyp == STATUE_TRAP) {
				if (activate_statue_trap(trap, x, y, FALSE))
				    exercise(A_WIS, TRUE);
				return 1;
			    } else {
				find_trap(trap);
			    }
			}
		    }
		}
	    }
	}
	return 1;
}


int dosearch(void)
{
	return dosearch0(0);
}

/* Pre-map the sokoban levels (called during level creation)
 * Don't use map_<foo>: Those functions are meant to be used when the player
 * is on the level, which is not the case here. */
void sokoban_detect(struct level *lev)
{
	int x, y;
	struct trap *ttmp;
	struct obj *obj;

	/* Map the background and boulders */
	for (x = 1; x < COLNO; x++)
	    for (y = 0; y < ROWNO; y++) {
		struct rm *loc = &lev->locations[x][y];
		int cmap;

		/* Set seen vector for all locations so travelling works
		 * on pre-mapped levels. */
		loc->seenv = SVALL;
		if (loc->typ == SDOOR)
		    loc->typ = DOOR;
		else if (loc->typ == SCORR)
		    loc->typ = CORR;
		loc->waslit = (loc->typ != CORR) ? TRUE : loc->lit;
		cmap = back_to_cmap(lev, x, y);

		loc->mem_bg = cmap;
		if (cmap == S_vodoor || cmap == S_hodoor ||
		    cmap == S_vcdoor || cmap == S_hcdoor) {
		    lev->locations[x][y].mem_door_l = 1;
		    lev->locations[x][y].mem_door_t = 1;
		} else {
		    lev->locations[x][y].mem_door_l = 0;
		    lev->locations[x][y].mem_door_t = 0;
		}

		for (obj = lev->objects[x][y]; obj; obj = obj->nexthere)
		    if (obj->otyp == BOULDER)
			loc->mem_obj = what_obj(BOULDER) + 1;
	    }

	/* Map the traps */
	for (ttmp = lev->lev_traps; ttmp; ttmp = ttmp->ntrap) {
	    ttmp->tseen = 1;
	    lev->locations[ttmp->tx][ttmp->ty].mem_trap = what_trap(ttmp->ttyp);
	}
}


/*detect.c*/
