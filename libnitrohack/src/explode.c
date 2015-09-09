/*	Copyright (C) 1990 by Ken Arromdee */
/* DynaHack may be freely redistributed.  See license for details. */

#include "hack.h"

/* Note: Arrays are column first, while the screen is row first */
static const int explosion[3][3] = {
	{ E_explode1, E_explode4, E_explode7 },
	{ E_explode2, E_explode5, E_explode8 },
	{ E_explode3, E_explode6, E_explode9 }
};

/* Note: I had to choose one of three possible kinds of "type" when writing
 * this function: a wand type (like in zap.c), an adtyp, or an object type.
 * Wand types get complex because they must be converted to adtyps for
 * determining such things as fire resistance.  Adtyps get complex in that
 * they don't supply enough information--was it a player or a monster that
 * did it, and with a wand, spell, or breath weapon?  Object types share both
 * these disadvantages....
 */
void explode(int x, int y,
	     int type, /* the same as in zap.c */
	     int dam,
	     char olet,
	     int expltype)
{
	int i, j, k, damu = dam;
	boolean starting = 1;
	boolean visible, any_shield;
	int uhurt = 0; /* 0=unhurt, 1=items damaged, 2=you and items damaged */
	const char *str;
	int idamres, idamnonres;
	struct monst *mtmp;
	uchar adtyp;
	int explmask[3][3];
		/* 0=normal explosion, 1=do shieldeff, 2=do nothing */
	boolean shopdamage = FALSE;
	boolean generic = FALSE;

	if (olet == WAND_CLASS)		/* retributive strike */
		switch (Role_switch) {
			case PM_PRIEST:
			case PM_MONK:
			case PM_WIZARD: damu /= 5;
				  break;
			case PM_HEALER:
			case PM_KNIGHT: damu /= 2;
				  break;
			default:  break;
		}

	if (olet == MON_EXPLODE) {
	    str = killer;
	    killer = 0;		/* set again later as needed */
	    adtyp = AD_PHYS;
	} else
	switch (abs(type) % 10) {
		case 0: str = "magical blast";
			adtyp = AD_MAGM;
			break;
		case 1: str =   olet == BURNING_OIL ?	"burning oil" :
				olet == SCROLL_CLASS ?	"tower of flame" :
							"fireball";
			adtyp = AD_FIRE;
			break;
		case 2: str = "ball of cold";
			adtyp = AD_COLD;
			break;
		case 4: str =  (olet == WAND_CLASS) ? "death field" :
							"disintegration field";
			adtyp = AD_DISN;
			break;
		case 5: str = "ball of lightning";
			adtyp = AD_ELEC;
			break;
		case 6: str = "poison gas cloud";
			adtyp = AD_DRST;
			break;
		case 7: str = "splash of acid";
			adtyp = AD_ACID;
			break;
		default: impossible("explosion base type %d?", type); return;
	}

	any_shield = visible = FALSE;
	for (i=0; i<3; i++) for (j=0; j<3; j++) {
		if (!isok(i+x-1, j+y-1)) {
			explmask[i][j] = 2;
			continue;
		} else
			explmask[i][j] = 0;

		if (i+x-1 == u.ux && j+y-1 == u.uy) {
		    switch(adtyp) {
			case AD_PHYS:
				explmask[i][j] = 0;
				break;
			case AD_MAGM:
				explmask[i][j] = !!Antimagic;
				break;
			case AD_FIRE:
				explmask[i][j] = !!FFire_resistance;
				break;
			case AD_COLD:
				explmask[i][j] = !!FCold_resistance;
				break;
			case AD_DISN:
				explmask[i][j] = (olet == WAND_CLASS) ?
					!!(nonliving(youmonst.data) ||
					   is_demon(youmonst.data)) :
					!!(FDisint_resistance ||
					   (PDisint_resistance && rn2(10)));
				break;
			case AD_ELEC:
				explmask[i][j] = !!FShock_resistance;
				break;
			case AD_DRST:
				explmask[i][j] = !!FPoison_resistance;
				break;
			case AD_ACID:
				explmask[i][j] = !!Acid_resistance;
				break;
			default:
				impossible("explosion type %d?", adtyp);
				break;
		    }
		}
		/* can be both you and mtmp if you're swallowed */
		mtmp = m_at(level, i+x-1, j+y-1);
		if (!mtmp && i+x-1 == u.ux && j+y-1 == u.uy)
			mtmp = u.usteed;
		if (mtmp) {
		    if (mtmp->mhp < 1) explmask[i][j] = 2;
		    else switch(adtyp) {
			case AD_PHYS:
				break;
			case AD_MAGM:
				explmask[i][j] |= resists_magm(mtmp);
				break;
			case AD_FIRE:
				explmask[i][j] |= resists_fire(mtmp);
				break;
			case AD_COLD:
				explmask[i][j] |= resists_cold(mtmp);
				break;
			case AD_DISN:
				explmask[i][j] |= (olet == WAND_CLASS) ?
					(nonliving(mtmp->data) || is_demon(mtmp->data)) :
					resists_disint(mtmp);
				break;
			case AD_ELEC:
				explmask[i][j] |= resists_elec(mtmp);
				break;
			case AD_DRST:
				explmask[i][j] |= resists_poison(mtmp);
				break;
			case AD_ACID:
				explmask[i][j] |= resists_acid(mtmp);
				break;
			default:
				impossible("explosion type %d?", adtyp);
				break;
		    }
		}
		if (mtmp && cansee(i+x-1,j+y-1) && !canspotmon(level, mtmp))
		    map_invisible(i+x-1, j+y-1);
		else if (!mtmp && level->locations[i+x-1][j+y-1].mem_invis) {
		    unmap_object(i+x-1, j+y-1);
		    newsym(i+x-1, j+y-1);
		}
		if (cansee(i+x-1, j+y-1)) visible = TRUE;
		if (explmask[i][j] == 1) any_shield = TRUE;
	}

	if (visible) {
		/* Start the explosion */
		for (i=0; i<3; i++) for (j=0; j<3; j++) {
			if (explmask[i][j] == 2) continue;
			tmp_at(starting ? DISP_BEAM : DISP_CHANGE,
				dbuf_explosion(expltype, explosion[i][j]));
			tmp_at(i+x-1, j+y-1);
			starting = 0;
		}
		flush_screen();	/* will flush screen and output */

		if (any_shield && flags.sparkle > 0) { /* simulate shield effect */
		    for (k = 0; k < SHIELD_COUNT; k += flags.sparkle) {
			for (i=0; i<3; i++) for (j=0; j<3; j++) {
			    if (explmask[i][j] == 1)
				/*
				 * Bypass tmp_at() and send the shield glyphs
				 * directly to the buffered screen.  tmp_at()
				 * will clean up the location for us later.
				 */
				dbuf_set_effect(i+x-1, j+y-1,
					dbuf_effect(E_MISC, shield_static[k]));
			}
			flush_screen();	/* will flush screen and output */
			win_delay_output();
		    }

		    /* Cover last shield glyph with blast symbol. */
		    for (i=0; i<3; i++) for (j=0; j<3; j++) {
			if (explmask[i][j] == 1)
			    dbuf_set_effect(i+x-1,j+y-1,
					    dbuf_explosion(expltype, explosion[i][j]));
		    }

		} else {		/* delay a little bit. */
		    win_delay_output();
		    win_delay_output();
		}

		tmp_at(DISP_END, 0); /* clear the explosion */
	} else {
	    if (olet == MON_EXPLODE) {
		str = "explosion";
		generic = TRUE;
	    }
	    if (flags.soundok) You_hear("a blast.");
	}

    if (dam)
	for (i=0; i<3; i++) for (j=0; j<3; j++) {
		if (explmask[i][j] == 2) continue;
		if (i+x-1 == u.ux && j+y-1 == u.uy)
			uhurt = (explmask[i][j] == 1) ? 1 : 2;
		idamres = idamnonres = 0;
		if (type >= 0)
		    zap_over_floor((xchar)(i+x-1), (xchar)(j+y-1),
		    		type, &shopdamage);

		mtmp = m_at(level, i+x-1, j+y-1);
		if (!mtmp && i+x-1 == u.ux && j+y-1 == u.uy)
			mtmp = u.usteed;
		if (!mtmp) continue;
		if (u.uswallow && mtmp == u.ustuck) {
			if (is_animal(u.ustuck->data))
				pline("%s gets %s!",
				      Monnam(u.ustuck),
				      (adtyp == AD_FIRE) ? "heartburn" :
				      (adtyp == AD_COLD) ? "chilly" :
				      (adtyp == AD_DISN) ? ((olet == WAND_CLASS) ?
				       "irradiated by pure energy" : "perforated") :
				      (adtyp == AD_ELEC) ? "shocked" :
				      (adtyp == AD_DRST) ? "poisoned" :
				      (adtyp == AD_ACID) ? "an upset stomach" :
				       "fried");
			else
				pline("%s gets slightly %s!",
				      Monnam(u.ustuck),
				      (adtyp == AD_FIRE) ? "toasted" :
				      (adtyp == AD_COLD) ? "chilly" :
				      (adtyp == AD_DISN) ? ((olet == WAND_CLASS) ?
				       "overwhelmed by pure energy" : "perforated") :
				      (adtyp == AD_ELEC) ? "shocked" :
				      (adtyp == AD_DRST) ? "intoxicated" :
				      (adtyp == AD_ACID) ? "burned" :
				       "fried");
		} else if (cansee(i+x-1, j+y-1)) {
		    if (mtmp->m_ap_type) seemimic(mtmp);
		    pline("%s is caught in the %s!", Monnam(mtmp), str);
		}

		destroy_mitem(mtmp, SCROLL_CLASS, adtyp, &idamres);
		destroy_mitem(mtmp, SPBOOK_CLASS, adtyp, &idamres);
		destroy_mitem(mtmp, POTION_CLASS, adtyp, &idamnonres);
		destroy_mitem(mtmp, WAND_CLASS, adtyp, &idamnonres);

		if (explmask[i][j] == 1) {
			golemeffects(mtmp, (int) adtyp, dam + idamres);
			mtmp->mhp -= idamnonres;
		} else {
		/* call resist with 0 and do damage manually so 1) we can
		 * get out the message before doing the damage, and 2) we can
		 * call mondied, not killed, if it's not your blast
		 */
			int mdam = dam;

			if (resist(mtmp, olet, 0, FALSE)) {
			    if (cansee(i+x-1,j+y-1))
				pline("%s resists the %s!", Monnam(mtmp), str);
			    mdam = dam/2;
			}
			if (mtmp == u.ustuck)
				mdam *= 2;
			if (resists_cold(mtmp) && adtyp == AD_FIRE)
				mdam *= 2;
			else if (resists_fire(mtmp) && adtyp == AD_COLD)
				mdam *= 2;
			mtmp->mhp -= mdam;
			mtmp->mhp -= (idamres + idamnonres);
		}
		if (mtmp->mhp <= 0) {
			/* KMH -- Don't blame the player for pets killing gas spores */
			if (!flags.mon_moving) killed(mtmp);
			else monkilled(mtmp, "", (int)adtyp);
		} else if (!flags.mon_moving) setmangry(mtmp);
	}

	/* Do your injury last */
	if (uhurt) {
		int kf;
		char kb[BUFSZ];

		if ((type >= 0 || adtyp == AD_PHYS) &&	/* gas spores */
				flags.verbose && olet != SCROLL_CLASS)
			pline("You are caught in the %s!", str);

		if (adtyp == AD_FIRE && olet == WEAPON_CLASS) {
		    /* projectile of detonation */
		    kf = KILLED_BY_AN;
		    strcpy(kb, "exploding projectile");
		} else if (olet == MON_EXPLODE) {
		    /* killer handled by caller */
		    if (!generic)
			strcpy(kb, str);
		    kf = KILLED_BY_AN;
		} else if (type >= 0 && olet != SCROLL_CLASS) {
		    kf = NO_KILLER_PREFIX;
		    sprintf(kb, "caught %sself in %s own %s",
			    uhim(), uhis(), str);
		} else if (!strncmpi(str,"tower of flame", 8) ||
			   !strncmpi(str,"fireball", 8)) {
		    kf = KILLED_BY_AN;
		    strcpy(kb, str);
		} else {
		    kf = KILLED_BY;
		    strcpy(kb, str);
		}

		exercise(A_STR, FALSE);

		if (adtyp == AD_FIRE) {
		    fire_damageu(damu, NULL, kb, kf, 25, TRUE, FALSE);
		} else if (adtyp == AD_COLD) {
		    cold_damageu(damu, NULL, kb, kf, 25);
		} else if (adtyp == AD_ELEC) {
		    elec_damageu(damu, NULL, kb, kf, 25, TRUE);
		} else if (adtyp == AD_MAGM) {
		    mana_damageu(damu, NULL, kb, kf, FALSE);
		} else {
		    if (Invulnerable) {
			damu = 0;
			pline("You are unharmed!");
		    } else if ((Half_physical_damage && adtyp == AD_PHYS) ||
			       (PPoison_resistance && adtyp == AD_DRST)) {
			damu = (damu + 1) / 2;
		    }

		    ugolemeffects(adtyp, damu);
		    if (uhurt == 2) {
			if (Upolyd)
			    u.mh -= damu;
			else
			    u.uhp -= damu;
			iflags.botl = 1;
		    }
		    if (Upolyd && u.mh <= 0) {
			rehumanize();
		    } else if (u.uhp <= 0) {
			killer_format = kf;
			killer = kb;
			/* Known BUG: BURNING suppresses corpse in bones data,
			   but done does not handle killer reason correctly */
			done((adtyp == AD_FIRE) ? BURNING : DIED);
		    }
		}
	}

	if (shopdamage) {
		pay_for_damage(adtyp == AD_FIRE ? "burn away" :
			       adtyp == AD_COLD ? "shatter" :
			       adtyp == AD_DISN ? "disintegrate" : "destroy",
			       FALSE);
	}

	/* explosions are noisy */
	i = dam * dam;
	if (i < 50) i = 50;	/* in case random damage is very small */
	wake_nearto(x, y, i);
}


struct scatter_chain {
	struct scatter_chain *next;	/* pointer to next scatter item	*/
	struct obj *obj;		/* pointer to the object	*/
	struct obj *ostack;		/* stack object was split from	*/
	xchar ox;			/* location of			*/
	xchar oy;			/*	item			*/
	schar dx;			/* direction of			*/
	schar dy;			/*	travel			*/
	int range;			/* range of object		*/
	boolean stopped;		/* flag for in-motion/stopped	*/
};

/*
 * scflags:
 *	VIS_EFFECTS	Add visual effects to display
 *	MAY_HITMON	Objects may hit monsters
 *	MAY_HITYOU	Objects may hit hero
 *	MAY_HIT		Objects may hit you or monsters
 *	MAY_DESTROY	Objects may be destroyed at random
 *	MAY_FRACTURE	Stone objects can be fractured (statues, boulders)
 */

/* returns number of scattered objects */
long scatter(int sx, int sy,	/* location of objects to scatter */
             int blastforce,	/* force behind the scattering	*/
             unsigned int scflags,
             struct obj *obj)	/* only scatter this obj        */
{
	struct obj *otmp;
	struct obj *ostack;
	struct level *lev = obj ? obj->olev : level;
	int tmp;
	int farthest = 0;
	uchar typ;
	long qtmp;
	boolean used_up;
	boolean individual_object = obj ? TRUE : FALSE;
	struct monst *mtmp;
	struct scatter_chain *stmp, *stmp2 = 0;
	struct scatter_chain *schain = NULL;
	long total = 0L;
	boolean visible = (lev == level && cansee(sx, sy));

	while ((otmp = individual_object ? obj : lev->objects[sx][sy]) != 0) {
	    if (otmp->quan > 1L) {
		qtmp = otmp->quan - 1;
		if (qtmp > LARGEST_INT) qtmp = LARGEST_INT;
		qtmp = (long)rnd((int)qtmp);
		ostack = otmp;
		otmp = splitobj(otmp, qtmp);
	    } else {
		ostack = NULL;
		obj = NULL; /* all used */
	    }
	    obj_extract_self(otmp);
	    used_up = FALSE;

	    /* 9 in 10 chance of fracturing boulders or statues */
	    if ((scflags & MAY_FRACTURE)
			&& ((otmp->otyp == BOULDER) || (otmp->otyp == STATUE))
			&& rn2(10)) {
		if (otmp->otyp == BOULDER) {
		    if (visible)
			pline("%s apart.", Tobjnam(otmp, "break"));
		    fracture_rock(otmp);
		    place_object(otmp, lev, sx, sy);
		    if ((otmp = sobj_at(BOULDER, lev, sx, sy)) != 0) {
			/* another boulder here, restack it to the top */
			obj_extract_self(otmp);
			place_object(otmp, lev, sx, sy);
		    }
		} else {
		    struct trap *trap;

		    if ((trap = t_at(lev, sx, sy)) && trap->ttyp == STATUE_TRAP)
			    deltrap(lev, trap);
		    if (visible)
			pline("%s.", Tobjnam(otmp, "crumble"));
		    break_statue(otmp);
		    place_object(otmp, lev, sx, sy);	/* put fragments on floor */
		}
		used_up = TRUE;

	    /* 1 in 10 chance of destruction of obj; glass, egg destruction */
	    } else if ((scflags & MAY_DESTROY) && (!rn2(10)
			|| (objects[otmp->otyp].oc_material == GLASS
			|| otmp->otyp == EGG))) {
		if (breaks(otmp, (xchar)sx, (xchar)sy)) used_up = TRUE;
	    }

	    if (!used_up) {
		stmp = malloc(sizeof(struct scatter_chain));
		stmp->next = NULL;
		stmp->obj = otmp;
		stmp->ostack = ostack;
		stmp->ox = sx;
		stmp->oy = sy;
		tmp = rn2(8);		/* get the direction */
		stmp->dx = xdir[tmp];
		stmp->dy = ydir[tmp];
		tmp = blastforce - (otmp->owt/40);
		if (tmp < 1) tmp = 1;
		stmp->range = rnd(tmp); /* anywhere up to that determ. by wt */
		if (farthest < stmp->range) farthest = stmp->range;
		stmp->stopped = FALSE;
		if (!schain)
		    schain = stmp;
		else
		    stmp2->next = stmp;
		stmp2 = stmp;
	    }
	}

	while (farthest-- > 0) {
		for (stmp = schain; stmp; stmp = stmp->next) {
		   if ((stmp->range-- > 0) && (!stmp->stopped)) {
			bhitpos.x = stmp->ox + stmp->dx;
			bhitpos.y = stmp->oy + stmp->dy;
			typ = lev->locations[bhitpos.x][bhitpos.y].typ;
			if (!isok(bhitpos.x, bhitpos.y)) {
				bhitpos.x -= stmp->dx;
				bhitpos.y -= stmp->dy;
				stmp->stopped = TRUE;
			} else if (!ZAP_POS(typ) ||
					closed_door(lev, bhitpos.x, bhitpos.y)) {
				bhitpos.x -= stmp->dx;
				bhitpos.y -= stmp->dy;
				stmp->stopped = TRUE;
			} else if ((mtmp = m_at(lev, bhitpos.x, bhitpos.y)) != 0) {
				if (scflags & MAY_HITMON) {
				    stmp->range--;
				    if (ohitmon(NULL, mtmp, stmp->obj,
						stmp->ostack, 1, FALSE)) {
					stmp->obj = NULL;
					stmp->stopped = TRUE;
				    }
				}
			} else if (bhitpos.x==u.ux && bhitpos.y==u.uy) {
				if (scflags & MAY_HITYOU) {
				    int hitvalu, hitu;

				    if (multi) nomul(0, NULL);
				    hitvalu = 8 + stmp->obj->spe;
				    if (bigmonst(youmonst.data)) hitvalu++;
				    hitu = thitu(hitvalu,
						 dmgval(NULL, stmp->obj, TRUE,
							&youmonst),
						 stmp->obj, stmp->ostack, NULL,
						 NULL);
				    if (hitu) {
					stmp->range -= 3;
					stop_occupation();
				    }
				}
			} else {
				if (scflags & VIS_EFFECTS) {
				    /* tmp_at(bhitpos.x, bhitpos.y); */
				    /* delay_output(); */
				}
			}
			stmp->ox = bhitpos.x;
			stmp->oy = bhitpos.y;
		   }
		}
	}
	for (stmp = schain; stmp; stmp = stmp2) {
		int x,y;

		stmp2 = stmp->next;
		x = stmp->ox; y = stmp->oy;
		if (stmp->obj) {
			if ( x!=sx || y!=sy )
			    total += stmp->obj->quan;
			place_object(stmp->obj, lev, x, y);
			stackobj(stmp->obj);
		}
		free(stmp);
		newsym(x,y);
	}
	newsym(sx,sy);

	return total;
}


/*
 * Splatter burning oil from x,y to the surrounding area.
 *
 * This routine should really take a how and direction parameters.
 * The how is how it was caused, e.g. kicked verses thrown.  The
 * direction is which way to spread the flaming oil.  Different
 * "how"s would give different dispersal patterns.  For example,
 * kicking a burning flask will splatter differently from a thrown
 * flask hitting the ground.
 *
 * For now, just perform a "regular" explosion.
 */
void splatter_burning_oil(int x, int y)
{
/* ZT_SPELL(ZT_FIRE) = ZT_SPELL(AD_FIRE-1) = 10+(2-1) = 11 */
#define ZT_SPELL_O_FIRE 11 /* value kludge, see zap.c */
    explode(x, y, ZT_SPELL_O_FIRE, dice(4,4), BURNING_OIL, EXPL_FIERY);
}

/*explode.c*/
