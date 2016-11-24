/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *  $Id$
 */

#include "g_local.h"
#include "fb_globals.h"

void NextLevel ();
void IdlebotForceStart ();
void StartMatch ();
void remove_specs_wizards ();
void lastscore_add ();
void OnePlayerMidairStats();
void OnePlayerInstagibStats();
void StartLogs();
void StopLogs();
void ClearDemoMarkers();

void race_match_start(void);
qbool race_match_mode(void);
char* race_scoring_system_name(void);
qbool race_can_cancel_demo(void);

extern int g_matchstarttime;

// Return count of players which have state cs_connected or cs_spawned.
// It is weird because used string comparision so I treat it as slow and idiotic but it return more players than CountPlayers().
int WeirdCountPlayers(void)
{
	gedict_t *p;
	int num;
	char state[16];

	for( num = 0, p = world + 1; p <= world + MAX_CLIENTS; p++ )
	{
		infokey(p, "*spectator", state, sizeof(state));

		if ( state[0] )
			continue; // ignore spectators

		infokey(p, "*state", state, sizeof(state));

		if ( streq(state, "connected") || streq(state, "spawned") )
			num++;
	}

	return num;
}

float CountPlayers()
{
	gedict_t	*p;
	float		num = 0;

	for ( p = world; (p = find_plr( p )); )
		num++;

	return num;
}

float CountBots (void)
{
	gedict_t	*p;
	float		num = 0;

	for (p = world; (p = find_plr (p)); )
		if (p->isBot)
			num++;

	return num;
}

float CountRPlayers()
{
	gedict_t	*p;
	float		num = 0;

	for ( p = world; (p = find_plr( p )); )
		if ( p->ready )
			num++;

	return num;
}

float CountTeams()
{
	gedict_t	*p, *p2;
	float		num = 0;
	char		*s = "";

	for ( p = world; (p = find_plr( p )); )
		p->k_flag = 0;

	for ( p = world; (p = find_plr( p )); ) {
		if( p->k_flag || strnull( s = getteam( p ) ) )
			continue;

		p->k_flag = 1;
		num++;

		for( p2 = p; (p2 = find_plr( p2 )); )
			if( streq( s, getteam( p2 ) ) )
				p2->k_flag = 1;
	}
	return num;
}

// return number of teams where at least one member is ready?
float CountRTeams()
{
	gedict_t	*p, *p2;
	float		num = 0;
	char		*s = "";

	for ( p = world; (p = find_plr( p )); )
		p->k_flag = 0;

	for ( p = world; (p = find_plr( p )); ) {
		if( p->k_flag || !p->ready || strnull( s = getteam( p ) ) )
			continue;

		p->k_flag = 1;
		num++;

		for( p2 = p; (p2 = find_plr( p2 )); )
			if( streq( s, getteam( p2 ) ) )
				p2->k_flag = 1;
	}
	return num;
}

// check count of members in each team i'm guess
// and return 0 if at least one team has less members than 'memcnt'
// else return 1 (even we have more mebers than memcnt, dunno is this bug <- FIXME)

float CheckMembers ( float memcnt )
{
	gedict_t	*p, *p2;
	float		f1;
	char		*s = "";

	for ( p = world; (p = find_plr( p )); )
		p->k_flag = 0;

	for( p = world; (p = find_plr( p )); ) {
		if ( p->k_flag )
			continue;

		p->k_flag = 1;
		f1 = 1;

		if( !strnull ( s = getteam( p ) ) )
			for( p2 = p; (p2 = find_plr( p2 )); )
				if( streq( s, getteam( p2 ) ) ) {
					p2->k_flag = 1;
					f1++;
				}

		if ( f1 < memcnt )
			return 0;
	}
	return 1;
}

#define MAX_TM_STATS (MAX_CLIENTS)

char tmStats_names[MAX_TM_STATS][MAX_TEAM_NAME]; // u can't put this in struct in QVM

typedef struct teamStats_s {
	char *name; // team name
	int gfrags; // frags from ghosts
	int frags, deaths, tkills;
	float dmg_t, dmg_g, dmg_team, dmg_eweapon;
// { ctf
	float res, str, rgn, hst;
	int caps, pickups, returns, f_defends, c_defends;
// }
	wpType_t wpn[wpMAX];
	itType_t itm[itMAX];
	int transferred_packs;
} teamStats_t;

teamStats_t tmStats[MAX_TM_STATS];

int tmStats_cnt = 0;

void CollectTpStats()
{
	gedict_t	*p, *p2;
	int from1, from2;
	int i, *frags;
	char *tmp = "";

	for( from1 = 0, p = world; (p = find_plrghst ( p, &from1 )); )
		p->ready = 0; // clear mark

//	get one player and search all his mates, mark served players via ->ready field
//  ghosts is served too

	for( from1 = 0, p = world; (p = find_plrghst ( p, &from1 )); ) {
		if( p->ready || strnull( tmp = getteam( p ) ) )
			continue; // served or wrong team

		if ( tmStats_cnt < 0 || tmStats_cnt >= MAX_TM_STATS )
			return; // all slots busy

		for( from2 = 0, p2 = world; (p2 = find_plrghst ( p2, &from2 )); ) {
			if( p2->ready || strneq( tmp, getteam( p2 ) ))
				continue; // served or on different team

			if ( strnull(tmStats[tmStats_cnt].name) ) // we not yet done that, do that once
				strlcpy(tmStats[tmStats_cnt].name, tmp, MAX_TEAM_NAME);

			frags = (p2->ct == ctPlayer ? &tmStats[tmStats_cnt].frags : &tmStats[tmStats_cnt].gfrags);
			frags[0]					  += p2->s.v.frags;
			tmStats[tmStats_cnt].deaths   += p2->deaths;
			tmStats[tmStats_cnt].tkills   += p2->friendly;

			tmStats[tmStats_cnt].dmg_t    += p2->ps.dmg_t;
			tmStats[tmStats_cnt].dmg_g    += p2->ps.dmg_g;
			tmStats[tmStats_cnt].dmg_team += p2->ps.dmg_team;
			tmStats[tmStats_cnt].dmg_eweapon += p2->ps.dmg_eweapon;

			for ( i = itNONE; i < itMAX; i++ ) { // summ each field of items
				tmStats[tmStats_cnt].itm[i].tooks += p2->ps.itm[i].tooks;
				tmStats[tmStats_cnt].itm[i].time  += p2->ps.itm[i].time;
			}

			for ( i = wpNONE; i < wpMAX; i++ ) { // summ each field of weapons
				tmStats[tmStats_cnt].wpn[i].hits    += p2->ps.wpn[i].hits;
				tmStats[tmStats_cnt].wpn[i].attacks += p2->ps.wpn[i].attacks;

				tmStats[tmStats_cnt].wpn[i].kills   += p2->ps.wpn[i].kills;
				tmStats[tmStats_cnt].wpn[i].deaths  += p2->ps.wpn[i].deaths;
				tmStats[tmStats_cnt].wpn[i].tkills  += p2->ps.wpn[i].tkills;
				tmStats[tmStats_cnt].wpn[i].ekills  += p2->ps.wpn[i].ekills;
				tmStats[tmStats_cnt].wpn[i].drops   += p2->ps.wpn[i].drops;
				tmStats[tmStats_cnt].wpn[i].tooks   += p2->ps.wpn[i].tooks;
				tmStats[tmStats_cnt].wpn[i].ttooks  += p2->ps.wpn[i].ttooks;
			}

			tmStats[tmStats_cnt].transferred_packs += p2->ps.transferred_packs;

// { ctf related
			tmStats[tmStats_cnt].res += p2->ps.res_time;
			tmStats[tmStats_cnt].str += p2->ps.str_time;
			tmStats[tmStats_cnt].hst += p2->ps.hst_time;
			tmStats[tmStats_cnt].rgn += p2->ps.rgn_time;

			tmStats[tmStats_cnt].caps      += p2->ps.caps;
			tmStats[tmStats_cnt].pickups   += p2->ps.pickups;
			tmStats[tmStats_cnt].returns   += p2->ps.returns;
			tmStats[tmStats_cnt].f_defends += p2->ps.f_defends;
			tmStats[tmStats_cnt].c_defends += p2->ps.c_defends;
// }

			p2->ready = 1; // set mark
		}

		if ( strnull(tmStats[tmStats_cnt].name) )
			continue; // wtf, empty team?

		if ( isCTF() && g_globalvars.time - match_start_time > 0 )
		{
			tmStats[tmStats_cnt].res = ( tmStats[tmStats_cnt].res / ( g_globalvars.time - match_start_time )) * 100;
			tmStats[tmStats_cnt].str = ( tmStats[tmStats_cnt].str / ( g_globalvars.time - match_start_time )) * 100;
			tmStats[tmStats_cnt].hst = ( tmStats[tmStats_cnt].hst / ( g_globalvars.time - match_start_time )) * 100;
			tmStats[tmStats_cnt].rgn = ( tmStats[tmStats_cnt].rgn / ( g_globalvars.time - match_start_time )) * 100;
		}

		tmStats_cnt++;
	}
}

void ShowTeamsBanner ( )
{
	int i;

	G_bprint(2, "�����������������������������������\n");


//	for( i = 666 + 1; i <= k_teamid ; i++ )
//		G_bprint(2, "%s�%s�", (i != (666+1) ? " vs " : ""), ezinfokey(world, va("%d", i)));
	for( i = 0; i < min(tmStats_cnt, MAX_TM_STATS); i++ )
		G_bprint(2, "%s�%s�", (i ? " vs " : ""), tmStats[i].name);

	G_bprint(2, " %s:\n", redtext("match statistics"));

	G_bprint(2, "�����������������������������������\n");
}

void SummaryTPStats()
{
	int i;
	float h_sg, h_ssg, h_gl, h_rl, h_lg;

	ShowTeamsBanner ();

	G_bprint(2, "\n%s, %s, %s, %s\n", redtext("weapons"), redtext("powerups"),
									  redtext("armors&mhs"), redtext("damage"));
	G_bprint(2, "�����������������������������������\n");

	for( i = 0; i < min(tmStats_cnt, MAX_TM_STATS); i++ ) {

		h_sg  = 100.0 * tmStats[i].wpn[wpSG].hits  / max(1, tmStats[i].wpn[wpSG].attacks);
		h_ssg = 100.0 * tmStats[i].wpn[wpSSG].hits / max(1, tmStats[i].wpn[wpSSG].attacks);
		h_gl  = tmStats[i].wpn[wpGL].hits;
		h_rl  = tmStats[i].wpn[wpRL].hits;
		h_lg  = 100.0 * tmStats[i].wpn[wpLG].hits  / max(1, tmStats[i].wpn[wpLG].attacks);

		// weapons
		if ( !cvar("k_instagib") ) {
		G_bprint(2, "�%s�: %s:%s%s%s%s%s\n", tmStats[i].name, redtext("Wp"),
					(h_lg  ? va(" %s%.0f%%", redtext("lg"),   h_lg) : ""),
					(h_rl  ? va(" %s%.0f",   redtext("rl"),   h_rl) : ""),
					(h_gl  ? va(" %s%.0f",   redtext("gl"),   h_gl) : ""),
					(h_sg  ? va(" %s%.0f%%", redtext("sg"),   h_sg) : ""),
					(h_ssg ? va(" %s%.0f%%", redtext("ssg"), h_ssg) : ""));

		// powerups
		G_bprint(2, "%s: %s:%d %s:%d %s:%d\n", redtext("Powerups"),
				redtext("Q"), tmStats[i].itm[itQUAD].tooks, redtext("P"), tmStats[i].itm[itPENT].tooks,
				redtext("R"), tmStats[i].itm[itRING].tooks);

		// armors + megahealths
		G_bprint(2, "%s: %s:%d %s:%d %s:%d %s:%d\n", redtext("Armr&mhs"),
				redtext("ga"), tmStats[i].itm[itGA].tooks, redtext("ya"), tmStats[i].itm[itYA].tooks,
				redtext("ra"), tmStats[i].itm[itRA].tooks, redtext("mh"), tmStats[i].itm[itHEALTH_100].tooks);

		} else  {
			G_bprint(2, "�%s�: %s:%s\n", tmStats[i].name, redtext("Wp"),
					(h_sg  ? va(" %s%.0f%%", redtext("cg"),   h_sg) : ""));
		}

		if ( isCTF() )
		{
			if ( cvar("k_ctf_runes") )
			{
				G_bprint(2, "%s: %s:%.0f%% %s:%.0f%% %s:%.0f%% %s:%.0f%%\n", redtext("RuneTime"),
					redtext("res"), tmStats[i].res, redtext("str"), tmStats[i].str,
					redtext("hst"), tmStats[i].hst, redtext("rgn"), tmStats[i].rgn);
			}
			G_bprint(2, "%s: %s:%d %s:%d %s:%d\n", redtext("     CTF"),
				redtext("pickups"), tmStats[i].pickups, redtext("caps"), tmStats[i].caps, redtext("returns"), tmStats[i].returns );
			G_bprint(2, "%s: %s:%d %s:%d\n", redtext(" Defends"),
				redtext("flag"), tmStats[i].f_defends, redtext("carrier"), tmStats[i].c_defends );
		}

		// rl
		G_bprint(2, "%s: %s:%d %s:%d %s:%d %s:%d\n", redtext("      RL"),
				redtext("Took"), tmStats[i].wpn[wpRL].tooks, redtext("Killed"), tmStats[i].wpn[wpRL].ekills,
				redtext("Dropped"), tmStats[i].wpn[wpRL].drops, redtext("Xfer"), tmStats[i].transferred_packs);

		// damage
		if ( deathmatch == 1 )
		{
			G_bprint(2, "%s: %s:%.0f %s:%.0f %s:%.0f %s:%.0f\n", redtext("  Damage"),
						redtext("Tkn"), tmStats[i].dmg_t, redtext("Gvn"), tmStats[i].dmg_g, redtext("EWep"), tmStats[i].dmg_eweapon, redtext("Tm"), tmStats[i].dmg_team);
		}
		else
		{
			G_bprint(2, "%s: %s:%.0f %s:%.0f %s:%.0f\n", redtext("  Damage"),
						redtext("Tkn"), tmStats[i].dmg_t, redtext("Gvn"), tmStats[i].dmg_g, redtext("Tm"), tmStats[i].dmg_team);
		}

		// times
		G_bprint(2, "%s: %s:%d\n", redtext("    Time"),
					redtext("Quad"), (int)tmStats[i].itm[itQUAD].time);
	}

	G_bprint(2, "�����������������������������������\n");
}


void TeamsStats ( )
{
	int	i, sumfrags = 0, wasPrint = 0;

	if ( isCA() )
	{
		CA_TeamsStats();
		return;
	}

	// Summing up the frags to calculate team percentages
	for( i = 0; i < min(tmStats_cnt, MAX_TM_STATS); i++ )
		sumfrags += max(0, tmStats[i].frags + tmStats[i].gfrags);
	// End of summing

	G_bprint(2, "\n%s: %s\n"
				"�����������������������������������\n", redtext("Team scores"),
													     redtext("frags � percentage"));

	for( i = 0; i < min(tmStats_cnt, MAX_TM_STATS); i++ ) {
		G_bprint(2, "�%s�: %d", tmStats[i].name, tmStats[i].frags );

		if( tmStats[i].gfrags )
			G_bprint( 2, " + (%d) = %d", tmStats[i].gfrags, tmStats[i].frags + tmStats[i].gfrags );

		// effi
		G_bprint(2, " � %.1f%%\n", (sumfrags > 0 ? ((float)(tmStats[i].frags + tmStats[i].gfrags))/sumfrags * 100 : 0.0));

		wasPrint = 1;
	}

	if ( wasPrint )
		G_bprint(2, "�����������������������������������\n");
}

float maxfrags, maxdeaths, maxfriend, maxeffi, maxcaps, maxdefends, maxsgeffi;
int maxspree, maxspree_q, maxdmgg, maxrlkills;

void OnePlayerStats(gedict_t *p, int tp)
{
	float	dmg_g, dmg_t, dmg_team, dmg_self, dmg_eweapon, dmg_g_rl;
	int   ra, ya, ga;
	int   mh, d_rl, k_rl, t_rl;
	int   quad, pent, ring;
	float ph_rl, vh_rl, h_rl, a_rl, ph_gl, vh_gl, a_gl, h_lg, a_lg, h_sg, a_sg, h_ssg, a_ssg;
	float e_sg, e_ssg, e_lg;
	int res, str, hst, rgn;

	dmg_g = p->ps.dmg_g;
	dmg_g_rl = p->ps.dmg_g_rl;
	dmg_t = p->ps.dmg_t;
	dmg_team = p->ps.dmg_team;
	dmg_self = p->ps.dmg_self;
	dmg_eweapon = p->ps.dmg_eweapon;
	ra    = p->ps.itm[itRA].tooks;
	ya    = p->ps.itm[itYA].tooks;
	ga    = p->ps.itm[itGA].tooks;
	mh    = p->ps.itm[itHEALTH_100].tooks;
	quad  = p->ps.itm[itQUAD].tooks;
	pent  = p->ps.itm[itPENT].tooks;
	ring  = p->ps.itm[itRING].tooks;

	h_rl  = p->ps.wpn[wpRL].hits;
	vh_rl = p->ps.wpn[wpRL].vhits;
	a_rl  = p->ps.wpn[wpRL].attacks;
	vh_gl = p->ps.wpn[wpGL].vhits;
	a_gl  = p->ps.wpn[wpGL].attacks;
	h_lg  = p->ps.wpn[wpLG].hits;
	a_lg  = p->ps.wpn[wpLG].attacks;
	h_sg  = p->ps.wpn[wpSG].hits;
	a_sg  = p->ps.wpn[wpSG].attacks;
	h_ssg = p->ps.wpn[wpSSG].hits;
	a_ssg = p->ps.wpn[wpSSG].attacks;

	e_sg  = 100.0 * h_sg  / max(1, a_sg);
	e_ssg = 100.0 * h_ssg / max(1, a_ssg);
	ph_gl = 100.0 * vh_gl / max(1, a_gl);
	ph_rl = 100.0 * vh_rl / max(1, a_rl);
	e_lg  = 100.0 * h_lg  / max(1, a_lg);

	d_rl = p->ps.wpn[wpRL].drops;
	k_rl = p->ps.wpn[wpRL].ekills;
	t_rl = p->ps.wpn[wpRL].tooks;

	if ( isCTF() && g_globalvars.time - match_start_time > 0 )
	{
		res = ( p->ps.res_time / ( g_globalvars.time - match_start_time )) * 100;
		str = ( p->ps.str_time / ( g_globalvars.time - match_start_time )) * 100;
		hst = ( p->ps.hst_time / ( g_globalvars.time - match_start_time )) * 100;
		rgn = ( p->ps.rgn_time / ( g_globalvars.time - match_start_time )) * 100;
	}
	else
	 res = str = hst = rgn = 0;

	if ( tp )
		G_bprint(2,"\235\236\236\236\236\236\236\236\236\237\n" );

	G_bprint(2, "\x87 %s%s:\n"
		"  %d (%d) %s%.1f%%\n", ( isghost( p ) ? "\x83" : "" ), getname(p),
		( isCTF() ? (int)(p->s.v.frags - p->ps.ctf_points) : (int)p->s.v.frags),
		( isCTF() ? (int)(p->s.v.frags - p->ps.ctf_points - p->deaths) : (int)(p->s.v.frags - p->deaths)),
		( tp ? va("%d ", (int)p->friendly ) : "" ),
		p->efficiency);

// qqshka - force show this always
//	if ( !tp || cvar( "tp_players_stats" ) ) {
		// weapons
	G_bprint(2, "%s:%s%s%s%s%s\n", redtext("Wp"),
		(a_lg  ? va(" %s%.1f%% (%d/%d)", redtext("lg"), e_lg, (int)h_lg, (int)a_lg) : ""),
		(ph_rl ? va(" %s%.1f%%", redtext("rl"), ph_rl) : ""),
		(ph_gl ? va(" %s%.1f%%", redtext("gl"), ph_gl) : ""),
		(e_sg  ? va(" %s%.1f%%", redtext("sg"), e_sg) : ""),
		(e_ssg ? va(" %s%.1f%%", redtext("ssg"), e_ssg) : ""));

		// rockets detail
		G_bprint(2, "%s: %s:%.1f %s:%.0f\n", redtext("RL skill"),
			redtext("ad"), vh_rl ? ( dmg_g_rl / vh_rl ) : 0. , redtext("dh"), h_rl);

		// velocity
		if ( isDuel() )
		{
			G_bprint(2, "%s: %s:%.1f %s:%.1f\n", redtext("   Speed"),
				redtext("max"), p->ps.velocity_max,
				redtext("average"), p->ps.vel_frames > 0 ? p->ps.velocity_sum / p->ps.vel_frames : 0.);
		}

		// armors + megahealths
		G_bprint(2, "%s: %s:%d %s:%d %s:%d %s:%d\n", redtext("Armr&mhs"),
			redtext("ga"), ga, redtext("ya"), ya, redtext("ra"), ra, redtext("mh"), mh);

		// powerups
		if ( isTeam() || isCTF() )
			G_bprint(2, "%s: %s:%d %s:%d %s:%d\n", redtext("Powerups"),
				redtext("Q"), quad, redtext("P"), pent, redtext("R"), ring);

		if ( isCTF() )
		{
			if ( cvar("k_ctf_runes") )
			{
				G_bprint(2, "%s: %s:%d%% %s:%d%% %s:%d%% %s:%d%%\n", redtext("RuneTime"),
					redtext("res"), res, redtext("str"), str, redtext("hst"), hst, redtext("rgn"), rgn );
			}
			G_bprint(2, "%s: %s:%d %s:%d %s:%d\n", redtext("     CTF"),
				redtext("pickups"), p->ps.pickups, redtext("caps"), p->ps.caps, redtext("returns"), p->ps.returns );
			G_bprint(2, "%s: %s:%d %s:%d\n", redtext(" Defends"),
				redtext("flag"), p->ps.f_defends, redtext("carrier"), p->ps.c_defends );
		}

		// rl
		if ( isTeam() )
			G_bprint(2, "%s: %s:%d %s:%d %s:%d%s\n", redtext("      RL"),
				redtext("Took"), t_rl, redtext("Killed"), k_rl, redtext("Dropped"), d_rl,
				(p->ps.transferred_packs ? va(" %s:%d", redtext("Xfer"), p->ps.transferred_packs) : ""));

		// damage
		if ( isTeam() && deathmatch == 1 )
		{
			G_bprint(2, "%s: %s:%.0f %s:%.0f %s:%.0f %s:%.0f %s:%.0f\n", redtext("  Damage"),
				redtext("Tkn"), dmg_t, redtext("Gvn"), dmg_g, redtext("EWep"), dmg_eweapon, redtext("Tm"), dmg_team, redtext("Self"), dmg_self);
		}
		else
		{
			G_bprint(2, "%s: %s:%.0f %s:%.0f %s:%.0f %s:%.0f\n", redtext("  Damage"),
				redtext("Tkn"), dmg_t, redtext("Gvn"), dmg_g, redtext("Tm"), dmg_team, redtext("Self"), dmg_self);
		}

		// times
		if ( g_globalvars.time - match_start_time > 0 )
		{
			if ( isDuel() )
			{
				G_bprint(2, "%s: %s:%d (%d%%)\n", redtext("    Time"),
					redtext("Control"), (int)p->ps.control_time, (int)((p->ps.control_time / ( g_globalvars.time - match_start_time )) * 100));
			}
			else
			{
				G_bprint(2, "%s: %s:%d\n", redtext("    Time"),
					redtext("Quad"), (int)p->ps.itm[itQUAD].time);
			}
		}

		if ( isDuel() )
		{
			//  endgame h & a
			G_bprint(2, " %s: %s:%d %s:", redtext("EndGame"), redtext("H"), (int)p->s.v.health, redtext("A"));
			if ( (int)p->s.v.armorvalue )
				G_bprint(2, "%s%d\n", armor_type(p->s.v.items), (int)p->s.v.armorvalue);
			else
				G_bprint(2, "0\n");

			// overtime h & a
			if ( k_overtime ) {
				G_bprint(2, " %s: %s:%d %s:", redtext("OverTime"), redtext("H"), (int)p->ps.ot_h, redtext("A"));
				if ( (int)p->ps.ot_a )
					G_bprint(2, "%s%d\n", armor_type(p->ps.ot_items), (int)p->ps.ot_a);
				else
					G_bprint(2, "0\n");
			}
		}
		else
		{
			G_bprint(2, "%s: %s:%d %s:%d\n", redtext(" Streaks"),
				redtext("Frags"), p->ps.spree_max, redtext("QuadRun"), p->ps.spree_max_q);
		}

		// spawnfrags
		if ( !isCTF() )
			G_bprint(2, "  %s: %d\n", redtext("SpawnFrags"), p->ps.spawn_frags);

//	}
	if ( !tp )
		G_bprint(2,"\235\236\236\236\236\236\236\236\236\237\n" );

	maxfrags   = max((isCTF() ? p->s.v.frags - p->ps.ctf_points : p->s.v.frags), maxfrags);
	maxdeaths  = max(p->deaths, maxdeaths);
	maxfriend  = max(p->friendly, maxfriend);
	maxeffi    = max(p->efficiency, maxeffi);
	maxcaps    = max(p->ps.caps, maxcaps);
	maxdefends = max(p->ps.f_defends, maxdefends);
	maxspree   = max(p->ps.spree_max, maxspree);
	maxspree_q = max(p->ps.spree_max_q, maxspree_q);
	maxdmgg    = max(p->ps.dmg_g, maxdmgg);
	maxrlkills = max(p->ps.wpn[wpRL].ekills, maxrlkills);
	maxsgeffi  = max(e_sg, maxsgeffi);
}

// Players statistics printout here
void PlayersStats ()
{
	gedict_t	*p, *p2;
	char		*tmp, *tmp2;
	int			tp, first, from1, from2;

	from1 = 0;
	p = find_plrghst ( world, &from1 );
	while( p ) {
		p->ready = 0; // clear mark
		p = find_plrghst ( p, &from1 );
	}

	// Probably low enough for a start value :)
	maxfrags = -999999;

	maxeffi = maxfriend = maxdeaths = maxcaps = maxdefends = maxsgeffi = 0;
	maxspree = maxspree_q = maxdmgg = maxrlkills = 0;

	tp = isTeam() || isCTF();

	G_bprint(2, "\n%s:\n�����������������������������������\n", redtext("Player statistics"));
	if (!cvar("k_midair"))
		G_bprint(2, "%s (%s) %s� %s\n", redtext( "Frags"), redtext( "rank"), ( tp ? redtext("friendkills "): "" ), redtext( "efficiency" ));

	from1 = 0;
	p = find_plrghst ( world, &from1 );
	while( p ) {
		if( !p->ready ) {

			first = 1;

			from2 = 0;
			p2 = find_plrghst ( world, &from2 );
			while ( p2 ) {
				if( !p2->ready ) {
					// sort by team
					tmp = getteam(p);
					tmp2 = getteam(p2);

					if( streq ( tmp, tmp2 ) ) {
						p2->ready = 1; // set mark

						if ( first ) {
							first = 0;
							if ( tp )
								G_bprint(2, "Team �%s�:\n", tmp );
						}

						if ( isCTF() )
						{
							if ( p2->s.v.frags - p2->ps.ctf_points < 1 )
								p2->efficiency = 0;
							else
								p2->efficiency = (p2->s.v.frags - p2->ps.ctf_points) / (p2->s.v.frags - p2->ps.ctf_points + p2->deaths) * 100;
						}
						else if ( isRA() )
						{
							p2->efficiency = ( ( p2->ps.loses + p2->ps.wins ) ? ( p2->ps.wins * 100.0f ) / ( p2->ps.loses + p2->ps.wins ) : 0 );
						}
						else
						{
							if( p2->s.v.frags < 1 )
								p2->efficiency = 0;
							else
								p2->efficiency = p2->s.v.frags / (p2->s.v.frags + p2->deaths) * 100;
						}

						if ( cvar("k_midair") )
							OnePlayerMidairStats(p2, tp);
						else if ( cvar("k_instagib") )
							OnePlayerInstagibStats(p2, tp);
						else
							OnePlayerStats(p2, tp);
					}
				}

				p2 = find_plrghst ( p2, &from2 );

				if ( !p2 )
					G_bprint(2, "\n"); // split players from different teams via \n
			}
		}

		p = find_plrghst ( p, &from1 );
	}

	if (isHoonyMode())
		HM_stats();
}

// Print the high score table
void TopStats ( )
{
	gedict_t	*p;
	float		f1, h_sg, a_sg;
	int			from;

	G_bprint(2, "�%s� %s:\n"
				"�����������������������������������\n"
				"      Frags: ", g_globalvars.mapname, redtext("top scorers"));

	from = f1 = 0;
	p = find_plrghst ( world, &from );
	while( p ) {
		if( (!isCTF() && p->s.v.frags == maxfrags) || (isCTF() &&  p->s.v.frags - p->ps.ctf_points == maxfrags)) {
			G_bprint(2, "%s%s%s �%d�\n", (f1 ? "             " : ""),
								( isghost( p ) ? "\x83" : "" ), getname( p ), (int)maxfrags);
			f1 = 1;
		}

		p = find_plrghst ( p, &from );
	}


	G_bprint(2, "     Deaths: ");

	from = f1 = 0;
	p = find_plrghst ( world, &from );
	while( p ) {
		if( p->deaths == maxdeaths ) {
			G_bprint(2, "%s%s%s �%d�\n", (f1 ? "             " : ""),
								( isghost( p ) ? "\x83" : "" ),	getname( p ), (int)maxdeaths);
			f1 = 1;
		}

		p = find_plrghst ( p, &from );
	}

	if( maxfriend ) {
		G_bprint(2, "Friendkills: ");

		from = f1 = 0;
		p = find_plrghst ( world, &from );
		while( p ) {
			if( p->friendly == maxfriend ) {
				G_bprint(2, "%s%s%s �%d�\n", (f1 ? "             " : ""),
								( isghost( p ) ? "\x83" : "" ),	getname( p ), (int)maxfriend);
				f1 = 1;
			}

			p = find_plrghst ( p, &from );
		}
	}

	G_bprint(2, " Efficiency: ");

	from = f1 = 0;
	p = find_plrghst ( world, &from );
	while( p ) {
		if( p->efficiency == maxeffi ) {
			G_bprint(2, "%s%s%s �%.1f%%�\n", (f1 ? "             " : ""),
								( isghost( p ) ? "\x83" : "" ),	getname( p ), maxeffi);
			f1 = 1;
		}

		p = find_plrghst ( p, &from );
	}

	if ( maxspree )
	{
		G_bprint( 2, " FragStreak: ");
		from = f1 = 0;
		p = find_plrghst( world, &from );
		while( p ) {
			if ( p->ps.spree_max == maxspree ) {
				G_bprint(2, "%s%s%s \220%d\221\n", (f1 ? "             " : ""),
					( isghost( p ) ? "\x83" : "" ), getname( p ), maxspree );
				f1 = 1;
			}
			p = find_plrghst( p, &from );
		}
	}

	if ( maxspree_q )
	{
		G_bprint( 2, "    QuadRun: ");
		from = f1 = 0;
		p = find_plrghst( world, &from );
		while( p ) {
			if ( p->ps.spree_max_q == maxspree_q ) {
					G_bprint(2, "%s%s%s \220%d\221\n", (f1 ? "             " : ""),
						( isghost( p ) ? "\x83" : "" ), getname( p ), maxspree_q );
					f1 = 1;
			}
			p = find_plrghst( p, &from );
		}
	}

    if ( maxrlkills && deathmatch == 1 )
    {
		G_bprint( 2, "  RL Killer: ");
		from = f1 = 0;
		p = find_plrghst( world, &from );
		while( p ) {
			if ( p->ps.wpn[wpRL].ekills == maxrlkills ) {
					G_bprint(2, "%s%s%s \220%d\221\n", (f1 ? "             " : ""),
						( isghost( p ) ? "\x83" : "" ), getname( p ), maxrlkills );
					f1 = 1;
			}
			p = find_plrghst( p, &from );
		}
    }

    if ( maxsgeffi && deathmatch == 1 )
    {
		G_bprint( 2, "Boomsticker: ");
		from = f1 = 0;
		p = find_plrghst( world, &from );
		while( p ) {
        	h_sg  = p->ps.wpn[wpSG].hits;
        	a_sg  = p->ps.wpn[wpSG].attacks;
        	h_sg  = 100.0 * h_sg  / max(1, a_sg);
			if ( h_sg == maxsgeffi ) {
					G_bprint(2, "%s%s%s \220%.1f%%\221\n", (f1 ? "             " : ""),
						( isghost( p ) ? "\x83" : "" ), getname( p ), maxsgeffi );
					f1 = 1;
			}
			p = find_plrghst( p, &from );
		}
    }
 
	if ( maxdmgg )
	{
		G_bprint( 2, "Annihilator: ");
		from = f1 = 0;
		p = find_plrghst( world, &from );
		while( p ) {
			if ( p->ps.dmg_g == maxdmgg ) {
					G_bprint(2, "%s%s%s \220%d\221\n", (f1 ? "             " : ""),
						( isghost( p ) ? "\x83" : "" ), getname( p ), maxdmgg );
					f1 = 1;
			}
			p = find_plrghst( p, &from );
		}
	}
 
	if ( isCTF() )
	{
		if ( maxcaps > 0 )
		{
			G_bprint(2, "   Captures: ");
			from = f1 = 0;
			p = find_plrghst ( world, &from );
			while( p ) {
				if ( p->ps.caps == maxcaps ) {
					G_bprint(2, "%s%s%s �%d�\n", (f1 ? "             " : ""),
								( isghost( p ) ? "\x83" : "" ), getname( p ), (int)maxcaps );
					f1 = 1;
				}
				p = find_plrghst ( p, &from );
			}
		}

		if ( maxdefends > 0 )
		{
			G_bprint(2, "FlagDefends: ");
			from = f1 = 0;
			p = find_plrghst ( world, &from );
			while( p ) {
				if ( p->ps.f_defends == maxdefends ) {
					G_bprint(2, "%s%s%s �%d�\n", (f1 ? "             " : ""),
						( isghost( p ) ? "\x83" : "" ), getname( p ), (int)maxdefends );
					f1 = 1;
				}
				p = find_plrghst ( p, &from );
			}
		}
	}

	G_bprint(2, "\n�����������������������������������\n");
}

extern demo_marker_t demo_markers[];
extern int demo_marker_index;

void ListDemoMarkers()
{
	int i = 0;

	if ( !demo_marker_index )
		return;

    G_bprint(2, "%s:\n�����������������������������������\n", redtext("Demo markers"));

	for (i = 0; i < demo_marker_index; ++i)
	{
		int total = (int)(demo_markers[i].time - match_start_time);
		G_bprint( 2, "%s: %d:%02d \220%s\221\n", redtext("Time"), (total / 60), (total % 60), demo_markers[i].markername);
	}

	G_bprint(2, "�����������������������������������\n");
}

void TopMidairStats ( )
{
	gedict_t  *p;
	float f1, vh_rl, a_rl, ph_rl, maxtopheight = 0, maxtopavgheight = 0, maxrlefficiency = 0;
	int  from, maxscore = -99999, maxkills = 0, maxmidairs = 0, maxstomps = 0, maxstreak = 0, maxspawnfrags = 0, maxbonus = 0;

	from = f1 = 0;
	p = find_plrghst ( world, &from );
	while( p ) {
		maxscore = max(p->s.v.frags, maxscore);
		maxkills = max(p->ps.mid_total + p->ps.mid_stomps, maxkills);
		maxmidairs = max(p->ps.mid_total, maxmidairs);
		maxstomps = max(p->ps.mid_stomps, maxstomps);
		maxstreak = max(p->ps.spree_max, maxstreak);
		maxspawnfrags = max(p->ps.spawn_frags, maxspawnfrags);
		maxbonus = max(p->ps.mid_bonus, maxbonus);
		maxtopheight = max(p->ps.mid_maxheight, maxtopheight);
		maxtopavgheight = max(p->ps.mid_avgheight, maxtopavgheight);
		vh_rl = p->ps.wpn[wpRL].vhits;
		a_rl  = p->ps.wpn[wpRL].attacks;
		ph_rl = 100.0 * vh_rl / max(1, a_rl);
		maxrlefficiency = max(ph_rl, maxrlefficiency);

		p = find_plrghst ( p, &from );
	}

  G_bprint(2, "%s:\n\235\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\237\n", redtext("Top performers"));

  from = f1 = 0;
  p = find_plrghst ( world, &from );
  while( p ) {
    if( p->s.v.frags == maxscore ) {
			G_bprint(2, "   %-13s: %s%s (%d)\n", (f1 ? "" : redtext("score")), ( isghost( p ) ? "\x83" : "" ), getname( p ), maxscore);
      f1 = 1;
    }
    p = find_plrghst ( p, &from );
  }

	if (maxkills) {
		from = f1 = 0;
		p = find_plrghst ( world, &from );
		while( p ) {
			if( (p->ps.mid_total + p->ps.mid_stomps) == maxkills ) {
				G_bprint(2, "   %-13s: %s%s (%d)\n", (f1 ? "" : redtext("kills")), ( isghost( p ) ? "\x83" : "" ), getname( p ), maxkills);
				f1 = 1;
			}
			p = find_plrghst ( p, &from );
		}
	}

	if (maxmidairs) {
		from = f1 = 0;
		p = find_plrghst ( world, &from );
		while( p ) {
			if( (p->ps.mid_total) == maxmidairs ) {
				G_bprint(2, "   %-13s: %s%s (%d)\n", (f1 ? "" : redtext("midairs")), ( isghost( p ) ? "\x83" : "" ), getname( p ), maxmidairs);
				f1 = 1;
			}
			p = find_plrghst ( p, &from );
		}
	}

	if (maxstomps) {
		from = f1 = 0;
		p = find_plrghst ( world, &from );
		while( p ) {
			if( (p->ps.mid_stomps) == maxstomps ) {
				G_bprint(2, "   %-13s: %s%s (%d)\n", (f1 ? "" : redtext("head stomps")), ( isghost( p ) ? "\x83" : "" ), getname( p ), maxstomps);
				f1 = 1;
			}
			p = find_plrghst ( p, &from );
		}
	}

	if (maxstreak) {
		from = f1 = 0;
		p = find_plrghst ( world, &from );
		while( p ) {
			if( (p->ps.spree_max) == maxstreak ) {
				G_bprint(2, "   %-13s: %s%s (%d)\n", (f1 ? "" : redtext("streak")), ( isghost( p ) ? "\x83" : "" ), getname( p ), maxmidairs);
				f1 = 1;
			}
			p = find_plrghst ( p, &from );
		}
	}

	if (maxspawnfrags) {
		from = f1 = 0;
		p = find_plrghst ( world, &from );
		while( p ) {
			if( (p->ps.spawn_frags) == maxspawnfrags ) {
				G_bprint(2, "   %-13s: %s%s (%d)\n", (f1 ? "" : redtext("spawn frags")), ( isghost( p ) ? "\x83" : "" ), getname( p ), maxspawnfrags);
				f1 = 1;
			}
			p = find_plrghst ( p, &from );
		}
	}

	if (maxbonus) {
		from = f1 = 0;
		p = find_plrghst ( world, &from );
		while( p ) {
			if( (p->ps.mid_bonus) == maxbonus ) {
				G_bprint(2, "   %-13s: %s%s (%d)\n", (f1 ? "" : redtext("bonus fiend")), ( isghost( p ) ? "\x83" : "" ), getname( p ), maxbonus);
				f1 = 1;
			}
			p = find_plrghst ( p, &from );
		}
	}

	if (maxtopheight) {
		from = f1 = 0;
		p = find_plrghst ( world, &from );
		while( p ) {
			if( (p->ps.mid_maxheight) == maxtopheight ) {
				G_bprint(2, "   %-13s: %s%s (%.1f)\n", (f1 ? "" : redtext("highest kill")), ( isghost( p ) ? "\x83" : "" ), getname( p ), maxtopheight);
				f1 = 1;
			}
			p = find_plrghst ( p, &from );
		}
	}

	if (maxtopavgheight) {
		from = f1 = 0;
		p = find_plrghst ( world, &from );
		while( p ) {
			if( (p->ps.mid_avgheight) == maxtopavgheight ) {
				G_bprint(2, "   %-13s: %s%s (%.1f)\n", (f1 ? "" : redtext("avg height")), ( isghost( p ) ? "\x83" : "" ), getname( p ), maxtopavgheight);
				f1 = 1;
			}
			p = find_plrghst ( p, &from );
		}
	}

	from = f1 = 0;
	p = find_plrghst ( world, &from );
	while( p ) {
		vh_rl = p->ps.wpn[wpRL].vhits;
		a_rl  = p->ps.wpn[wpRL].attacks;
		ph_rl = 100.0 * vh_rl / max(1, a_rl);
		if( (ph_rl) == maxrlefficiency ) {
			G_bprint(2, "   %-13s: %s%s (%.1f%%)\n", (f1 ? "" : redtext("rl efficiency")), ( isghost( p ) ? "\x83" : "" ), getname( p ), maxrlefficiency);
			f1 = 1;
		}
		p = find_plrghst ( p, &from );
	}

  G_bprint(2, "\235\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\237\n");
}

void OnePlayerInstagibStats( gedict_t *p, int tp )
{
	float h_ax, a_ax, h_sg, a_sg, h_ssg, a_ssg;
	char *stats_text;

	h_sg  = p->ps.wpn[wpSG].hits;
	a_sg  = p->ps.wpn[wpSG].attacks;
	h_ssg = p->ps.wpn[wpSSG].hits;
	a_ssg = p->ps.wpn[wpSSG].attacks;
	h_ax  = p->ps.wpn[wpAXE].hits;
	a_ax  = p->ps.wpn[wpAXE].attacks;

	h_ax  = 100.0 * h_ax  / max(1, a_ax);
	h_sg  = 100.0 * h_sg  / max(1, a_sg);
	h_ssg = 100.0 * h_ssg / max(1, a_ssg);

	stats_text = va("\n\x87 %s: %s%s \x87\n", "PLAYER", ( isghost( p ) ? "\x83" : "" ), getname(p));

	stats_text = va("%s \220%s\221\n", stats_text, "SCORES");
	stats_text = va("%s  %s: %.1f\n", stats_text, redtext("Efficiency"), p->efficiency);
	stats_text = va("%s  %s: %d\n", stats_text, redtext("Points"), (int)p->s.v.frags);
	stats_text = va("%s  %s: %d\n", stats_text, redtext("Frags"), p->ps.i_cggibs + p->ps.i_axegibs + p->ps.i_stompgibs);
	if (tp)
		stats_text = va("%s  %s: %d\n", stats_text, redtext("Teamkills"), (int)p->friendly);
	stats_text = va("%s  %s: %d\n", stats_text, redtext("Deaths"), (int)p->deaths);

	stats_text = va("%s  %s: %d\n", stats_text, redtext("Streaks"), p->ps.spree_max);
	stats_text = va("%s  %s: %d\n", stats_text, redtext("Spawns"), p->ps.spawn_frags);

	stats_text = va("%s \220%s\221\n", stats_text, "SPEED");
	stats_text = va("%s  %s: %.1f\n", stats_text, redtext("Maximum"), p->ps.velocity_max);
	stats_text = va("%s  %s: %.1f\n", stats_text, redtext("Average"),
		p->ps.vel_frames > 0 ? p->ps.velocity_sum / p->ps.vel_frames : 0.);

	stats_text = va("%s \220%s\221\n", stats_text, "WEAPONS");
	if ( cvar("k_instagib") )
	{
		stats_text = va("%s  %s: %s", stats_text, redtext("Coilgun"), (a_sg ? va("%.1f%% (%d)", h_sg, p->ps.i_cggibs) : ""));
		stats_text = va("%s%s", stats_text, (a_sg ? "" : "n/u"));
	}
	stats_text = va("%s\n", stats_text);
	stats_text = va("%s  %s: %s", stats_text, redtext("Axe"), (a_ax ? va("%.1f%% (%d)", h_ax, p->ps.i_axegibs) : ""));
	stats_text = va("%s%s", stats_text, (a_ax ? "" : "n/u"));
	stats_text = va("%s\n", stats_text);

	stats_text = va("%s \220%s\221\n", stats_text, "GIBS");
	stats_text = va("%s  %s: %d\n", stats_text, redtext("Coilgun"), p->ps.i_cggibs);
	stats_text = va("%s  %s: %d\n", stats_text, redtext("Axe"), p->ps.i_axegibs);
	stats_text = va("%s  %s: %d\n", stats_text, redtext("Stomps"), p->ps.i_stompgibs);

	stats_text = va("%s \220%s\221\n", stats_text, "MULTIGIBS");
	stats_text = va("%s  %s: %d\n", stats_text, redtext("Total Multigibs"), p->ps.i_multigibs);
	stats_text = va("%s  %s: %d\n", stats_text, redtext("Maximum Victims"), p->ps.i_maxmultigibs);

	stats_text = va("%s \220%s\221\n", stats_text, "AIRGIBS");
	stats_text = va("%s  %s: %d\n", stats_text, redtext("Total"), p->ps.i_airgibs);
	stats_text = va("%s  %s: %d\n", stats_text, redtext("Total Height"), p->ps.i_height);
	stats_text = va("%s  %s: %d\n", stats_text, redtext("Maximum Height"), p->ps.i_maxheight);
	stats_text = va("%s  %s: %.1f\n", stats_text, redtext("Average Height"), p->ps.i_airgibs ? p->ps.i_height / p->ps.i_airgibs : 0.);
	G_bprint(2, "%s", stats_text);

	if ( !tp )
		G_bprint(2,"�����������������������������������\n");

	maxfrags   = max((isCTF() ? p->s.v.frags - p->ps.ctf_points : p->s.v.frags), maxfrags);
	maxdeaths  = max(p->deaths, maxdeaths);
	maxfriend  = max(p->friendly, maxfriend);
	maxeffi    = max(p->efficiency, maxeffi);
	maxcaps    = max(p->ps.caps, maxcaps);
	maxdefends = max(p->ps.f_defends, maxdefends);
	maxspree   = max(p->ps.spree_max, maxspree);
	maxspree_q = max(p->ps.spree_max_q, maxspree_q);
	maxdmgg    = max(p->ps.dmg_g, maxdmgg);
	maxrlkills = max(p->ps.wpn[wpRL].ekills, maxrlkills);
	maxsgeffi  = max(h_sg, maxsgeffi);

}

void OnePlayerMidairStats( gedict_t *p, int tp )
{
	float vh_rl, a_rl, ph_rl;

	vh_rl = p->ps.wpn[wpRL].vhits;
	a_rl  = p->ps.wpn[wpRL].attacks;
	ph_rl = 100.0 * vh_rl / max(1, a_rl);

	G_bprint(2, "\x87 %s%s: %d\n", ( isghost( p ) ? "\x83" : "" ), getname(p), (int)p->s.v.frags);
	G_bprint(2, "   %-13s: %d\n", redtext("total midairs"), p->ps.mid_total);
	G_bprint(2, "    %12s: %d\n", "bronze", p->ps.mid_bronze);
	G_bprint(2, "    %12s: %d\n", "silver", p->ps.mid_silver);
	G_bprint(2, "    %12s: %d\n", "gold", p->ps.mid_gold);
	G_bprint(2, "    %12s: %d\n", "platinum", p->ps.mid_platinum);
	G_bprint(2, "   %-13s: %d\n", redtext("stomps"), p->ps.mid_stomps);
	G_bprint(2, "   %-13s: %d\n", redtext("streak"), p->ps.spree_max);
	G_bprint(2, "   %-13s: %d\n", redtext("spawnfrags"), p->ps.spawn_frags);
	G_bprint(2, "   %-13s: %d\n", redtext("bonuses"), p->ps.mid_bonus);
	G_bprint(2, "   %-13s: %.1f\n", redtext("max height"), p->ps.mid_maxheight);
	G_bprint(2, "   %-13s: %.1f\n", redtext("avg height"), (p->ps.mid_maxheight ? p->ps.mid_avgheight : 0));
	G_bprint(2, "   %-13s: %s\n", redtext("rl efficiency"), (ph_rl ? va("%.1f%%", ph_rl) : "  0.0%"));

	G_bprint(2,"\235\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\236\237\n" );
}

char *GetMode() {

	if ( isRA() )
		return "RA";
	else if ( isDuel() )
		return "duel";
	else if ( isTeam() )
		return "team";
	else if ( isCTF() )
		return "CTF";
	else if ( isFFA() )
		return "FFA";
	else
		return "unknown";
}

char *WpName( weaponName_t wp )
{
	switch ( wp ) {
		case wpAXE: return "axe";
		case wpSG:  return "sg";
		case wpSSG: return "ssg";
		case wpNG:  return "ng";
		case wpSNG: return "sng";
		case wpGL:  return "gl";
		case wpRL:  return "rl";
		case wpLG:  return "lg";

		// shut up gcc
		case wpNONE:
		case wpMAX: return "unknown";
	}

	return "unknown";
}

char *ItName( itemName_t it )
{
	switch ( it ) {
		case itHEALTH_15:  return "health_15";
		case itHEALTH_25:  return "health_25";
		case itHEALTH_100: return "health_100";
		case itGA:		   return "ga";
		case itYA:		   return "ya";
		case itRA:		   return "ra";
		case itQUAD:	   return "q";
		case itPENT:	   return "p";
		case itRING:	   return "r";

		// shut up gcc
		case itNONE:
		case itMAX: return "unknown";
	}

	return "unknown";
}


qbool itPowerup( itemName_t it )
{
	return (it == itQUAD || it == itPENT || it == itRING);
}

void s2di( fileHandle_t file_handle, const char *fmt, ... )
{
	va_list argptr;
	char    text[1024];

	va_start( argptr, fmt );
	Q_vsnprintf( text, sizeof(text), fmt, argptr );
	va_end( argptr );

	text[sizeof(text)-1] = 0;

	trap_FS_WriteFile( text, strlen(text), file_handle );
}

void s2di_weap_header(fileHandle_t handle, int format)
{
	s2di(handle, "\t\t\t<weapons>\n");
}

void s2di_weap_footer(fileHandle_t handle, int format)
{
	s2di(handle, "\t\t\t</weapons>\n");
}

void s2di_weap_stats(fileHandle_t handle, int format, int weapon, wpType_t* stats)
{
	s2di(handle, "\t\t\t\t<weapon name=\"%s\" hits=\"%d\" attacks=\"%d\""
			" kills=\"%d\" deaths=\"%d\" tkills=\"%d\" ekills=\"%d\""
			" drops=\"%d\" tooks=\"%d\" ttooks=\"%d\"/>\n",
			WpName(weapon), stats->hits, stats->attacks, stats->kills, stats->deaths, stats->tkills, stats->ekills,
			stats->drops, stats->tooks, stats->ttooks);
}

void s2di_teams_header(fileHandle_t handle, int format)
{
	char tmp[1024] = {0}, buf[1024] = {0};
	int i = 0;

	for ( tmp[0] = i = 0; i < min(tmStats_cnt, MAX_TM_STATS); i++ ) {
		snprintf(buf, sizeof(buf), " team%d=\"%s\"", i + 1, tmStats[i].name);
		strlcat(tmp, buf, sizeof(tmp));
	}

	if ( i )
		s2di(handle, "\t<teams%s>\n", striphigh(tmp));
}

void s2di_teams_footer(fileHandle_t handle, int format)
{
	s2di(handle, "\t</teams>\n");
}

void s2di_team_header(fileHandle_t handle, int format, int num, teamStats_t* stats)
{
	s2di(handle, "\t\t<team name=\"%s\" frags=\"%d\" deaths=\"%d\" tkills=\"%d\" dmg_tkn=\"%d\" dmg_gvn=\"%d\" dmg_tm=\"%d\">\n",
		striphigh(stats->name), stats->frags + stats->gfrags, stats->deaths, stats->tkills,
		(int)stats->dmg_t, (int)stats->dmg_g, (int)stats->dmg_team);
}

void s2di_team_footer(fileHandle_t handle, int format)
{
	s2di(handle, "\t\t</team>\n");
}

void s2di_items_header(fileHandle_t handle, int format)
{
	s2di(handle, "\t\t\t<items>\n");
}

void s2di_item_stats(fileHandle_t handle, int format, int j, itType_t* stats) {
	char buf[1024] = { 0 };

	if ( itPowerup( j ) )
		snprintf(buf, sizeof(buf), " time=\"%d\"", (int)stats->time);
	else
		buf[0] = 0;
	s2di(handle, "\t\t\t\t<item name=\"%s\" tooks=\"%d\"%s/>\n", ItName(j), stats->tooks, buf);
}

void s2di_items_footer(fileHandle_t handle, int format)
{
	s2di(handle, "\t\t\t</items>\n");
}

void s2di_players_header(fileHandle_t handle, int format)
{
	s2di(handle, "\t<players>\n");
}

void s2di_players_footer(fileHandle_t handle, int format)
{
	s2di(handle, "\t</players>\n");
}

void s2di_player_header(fileHandle_t handle, int format, gedict_t* player, char* team)
{
	s2di(handle, "\t\t<player name=\"%s\" team=\"%s\" frags=\"%d\" deaths=\"%d\" tkills=\"%d\""
				" dmg_tkn=\"%d\" dmg_gvn=\"%d\" dmg_tm=\"%d\" spawnfrags=\"%d\" xfer_packs=\"%d\""
				" spree=\"%d\" qspree=\"%d\" control_time=\"%f\">\n",
			striphigh(getname(player)), striphigh(team), (int)player->s.v.frags, (int)player->deaths, (int)player->friendly,
			(int)player->ps.dmg_t, (int)player->ps.dmg_g, (int)player->ps.dmg_team, player->ps.spawn_frags, player->ps.transferred_packs,
			player->ps.spree_max, player->ps.spree_max_q);
}

void s2di_player_footer(fileHandle_t handle, int format)
{
	s2di(handle, "\t\t</player>\n");
}

/*
char* xmlstring(char* original)
{
	static char string[MAX_STRINGS][1024];
	static int  index = 0;
	int length = strlen(original);
	int newlength = 0;
	int i = 0;
	
	index %= MAX_STRINGS;

	memset(string[index], 0, sizeof(string[0]));

	for (i = 0; i < length; ++i)
	{
		unsigned char ch = (unsigned char) original[i];

		if (ch == '<')
		{
			if (newlength < sizeof(string[0]) - 4)
			{
				string[index][newlength++] = '&';
				string[index][newlength++] = 'l';
				string[index][newlength++] = 't';
				string[index][newlength++] = ';';
			}
		}
		else if (ch == '>')
		{
			if (newlength < sizeof(string[0]) - 4)
			{
				string[index][newlength++] = '&';
				string[index][newlength++] = 'g';
				string[index][newlength++] = 't';
				string[index][newlength++] = ';';
			}
		}
		else if (ch == '"')
		{
			if (newlength < sizeof(string[0]) - 5)
			{
				string[index][newlength++] = '&';
				string[index][newlength++] = '#';
				string[index][newlength++] = '3';
				string[index][newlength++] = '4';
				string[index][newlength++] = ';';
			}
		}
		else if (ch == '&')
		{
			if (newlength < sizeof(string[0]) - 5)
			{
				string[index][newlength++] = '&';
				string[index][newlength++] = 'a';
				string[index][newlength++] = 'm';
				string[index][newlength++] = 'p';
				string[index][newlength++] = ';';
			}
		}
		else if (ch == '\'')
		{
			if (newlength < sizeof(string[0]) - 5)
			{
				string[index][newlength++] = '&';
				string[index][newlength++] = '#';
				string[index][newlength++] = '3';
				string[index][newlength++] = '9';
				string[index][newlength++] = ';';
			}
		}
		else 
		{
		}
	}
	for ( ; *i; i++ )
		if ( *i < 32 || (*i > 126 && *i < 160) || *i > 254)
			*i = 95;

	return string[index++];
}*/

void s2di_match_header(fileHandle_t handle, int format, char* ip, int port)
{
	char date[64] = { 0 };
	char matchtag[64] = { 0 };
	const char* mode = cvar ("k_instagib") ? "instagib" : (isRACE() ? "race" : GetMode ());

	infokey(world, "matchtag", matchtag, sizeof(matchtag));

	if ( !QVMstrftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S %Z", 0) )
		date[0] = 0; // bad date

	s2di(handle, "%s", "<?xml version=\"1.0\"?>\n");
	s2di(handle, "<match version=\"3\" date=\"%s\" map=\"%s\" hostname=\"%s\" ip=\"%s\" port=\"%d\" mode=\"%s\" tl=\"%d\" fl=\"%d\" dmm=\"%d\" tp=\"%d\">\n",
		date, g_globalvars.mapname, striphigh(cvar_string("hostname")), ip, port, mode, timelimit, fraglimit, deathmatch, teamplay);
	if (! strnull(cvar_string("serverdemo")))
		s2di(handle, "\t<demo>%s</demo>\n", cvar_string("serverdemo"));
}

void s2di_match_footer(fileHandle_t handle, int format)
{
	s2di(handle, "</match>\n");
}

void s2di_player_ctf_stats(fileHandle_t handle, int format, player_stats_t* stats)
{
	s2di(handle, "\t\t\t<ctf points=\"%d\" caps=\"%d\" flag-defends=\"%d\" cap-defends=\"%d\" "
	            "cap-frags=\"%d\" pickups=\"%d\" returns=\"%d\" "
	            "rune-res-time=\"%f\" rune-str-time=\"%f\" rune-hst-time=\"%f\" rune-rgn-time=\"%f\" />\n", 
		stats->ctf_points, stats->caps, stats->f_defends, stats->c_defends, 
		stats->c_frags, stats->pickups, stats->returns, 
		stats->res_time, stats->str_time, stats->hst_time, stats->rgn_time);
}

void s2di_player_instagib_stats(fileHandle_t handle, int format, player_stats_t* stats)
{
	s2di(handle, "\t\t\t<instagib height=\"%d\" maxheight=\"%d\" cggibs=\"%d\""
			" axegibs=\"%d\" stompgibs=\"%d\" multigibs=\"%d\" airgibs=\"%d\" "
			" maxmultigibs=\"%d\" rings=\"%d\" />\n", 
		stats->i_height, stats->i_maxheight, stats->i_cggibs, 
		stats->i_axegibs, stats->i_stompgibs, stats->i_multigibs, stats->i_airgibs, 
		stats->i_maxmultigibs, stats->i_rings);
}

void s2di_player_midair_stats(fileHandle_t handle, int format, player_stats_t* stats)
{
	s2di(handle, "\t\t\t<midair stomps=\"%d\" bronze=\"%d\" silver=\"%d\" gold=\"%d\" platinum=\"%d\" " 
			" total=\"%d\" bonus=\"%d\" totalheight=\"%f\" maxheight=\"%f\" avgheight=\"%f\" />\n", 
			stats->mid_stomps, stats->mid_bronze, stats->mid_silver, stats->mid_gold, stats->mid_platinum,
			stats->mid_total, stats->mid_bonus, stats->mid_totalheight, stats->mid_maxheight, stats->mid_avgheight);
}

void s2di_player_ra_stats(fileHandle_t handle, int format, player_stats_t* stats)
{
	s2di(handle, "\t\t\t<rocket-arena wins=\"%d\" losses=\"%d\" />\n", stats->wins, stats->loses);
}

void s2di_race_stats(fileHandle_t handle, int format)
{
	extern gedict_t* race_find_racer(gedict_t* p);
	gedict_t* p;

	s2di(handle, "\t<race route=\"%d\" weaponmode=\"%d\" startmode=\"%d\">", race.active_route, race.weapon, race.falsestart);
	if (! strnull(race.pacemaker_nick)) {
		s2di(handle, "\t\t<pacemaker time=\"%f\">%s</pacemaker>\n", race.pacemaker_time * 1.0f, race.pacemaker_nick);
	}
	for (p = world; (p = race_find_racer(p)); /**/) {
		int player_number = NUM_FOR_EDICT(p) - 1;
		raceRecord_t* record = NULL;
		if (player_number < 0 || player_number >= sizeof(race.currentrace) / sizeof(race.currentrace[0])) {
			continue;
		}
		record = &race.currentrace[player_number];

		s2di(handle, "\t<racer avgspeed=\"%f\" distance=\"%f\" time=\"%f\" "
			"racer=\"%s\" weaponmode=\"%d\" startmode=\"%d\" maxspeed=\"%f\">\n", 
			record->avgspeed / record->avgcount, record->distance, record->time, 
			striphigh(p->s.v.netname), record->maxspeed
		);
		s2di(handle, "\t</race>\n");
	}
}

// Only format supported at present
#define STATSFORMAT_XML 1

qbool CreateStatsFile(char* filename, char* ip, int port, qbool xml)
{
	gedict_t	*p, *p2;
	fileHandle_t di_handle;
	int from1, from2;
	char *team = "";
	int format = STATSFORMAT_XML;

	char date[64] = {0};
	char tmp[1024] = {0}, buf[1024] = {0};
	int i = 0, j = 0;

	if ( trap_FS_OpenFile( filename, &di_handle, FS_WRITE_BIN ) < 0 )
		return false;

	s2di_match_header(di_handle, format, ip, port);

// { TEAMS

	// Not a bug... only outputs header if teams available
	s2di_teams_header(di_handle, format);

	for ( i = 0; i < min(tmStats_cnt, MAX_TM_STATS); i++ ) {
		s2di_team_header(di_handle, format, i+1, &tmStats[i]);

		s2di_weap_header(di_handle, format);
		for ( j = 1; j < wpMAX; j++ ) {
			s2di_weap_stats(di_handle, format, j, &tmStats[i].wpn[j]);
		}
		s2di_weap_footer(di_handle, format);

		s2di_items_header(di_handle, format);
		for ( j = 1; j < itMAX; j++) {
			s2di_item_stats(di_handle, format, j, &tmStats[i].itm[j]);
		}
		s2di_items_footer(di_handle, format);

		s2di_team_footer(di_handle, format);
	}

	if ( i )
		s2di_teams_footer(di_handle, format);

// } TEAMS

// { PLAYERS

	for ( from1 = 0, p = world; (p = find_plrghst ( p, &from1 )); )
		p->ready = 0; // clear mark

	s2di_players_header(di_handle, format);

//	get one player and search all his mates, mark served players via ->ready field
//  ghosts is served too

	for ( from1 = 0, p = world; (p = find_plrghst ( p, &from1 )); ) {
		team = getteam( p );
		if ( p->ready /* || strnull( team ) */ )
			continue; // served or wrong team

		for ( from2 = 0, p2 = world; (p2 = find_plrghst ( p2, &from2 )); ) {
			if ( p2->ready || strneq( team, getteam( p2 ) ))
				continue; // served or on different team

			s2di_player_header(di_handle, format, p2, team);

			s2di_weap_header(di_handle, format);
			for ( j = 1; j < wpMAX; j++ ) {
				s2di_weap_stats(di_handle, format, j, &p2->ps.wpn[j]);
			}
			s2di_weap_footer(di_handle, format);

			s2di_items_header(di_handle, format);
			for ( j = 1; j < itMAX; j++) {
				s2di_item_stats(di_handle, format, j, &p2->ps.itm[j]);
			}
			s2di_items_footer(di_handle, format);

			if ( cvar("k_midair") )
				s2di_player_midair_stats(di_handle, format, &p2->ps);
			if ( cvar("k_instagib") )
				s2di_player_instagib_stats(di_handle, format, &p2->ps);
			if ( isCTF() )
				s2di_player_ctf_stats(di_handle, format, &p2->ps);
			if ( isRA() )
				s2di_player_ra_stats(di_handle, format, &p2->ps);
			if ( p2->isBot )
				s2di_player_bot_info (di_handle, format, &p2->fb);

			s2di_player_footer(di_handle, format);
			p2->ready = 1; // set mark
		}
	}

	s2di_players_footer(di_handle, format);
// } PLAYERS

	if (isRACE())
		s2di_race_stats(di_handle, format);

	s2di_match_footer(di_handle, format);

	trap_FS_CloseFile( di_handle );
	return true;
}

void StatsToFile()
{
	char name[256] = {0}, *ip = "", *port = "";
	int i = 0;

	if ( strnull( ip = cvar_string( "sv_local_addr" ) ) || strnull( port = strchr(ip, ':') ) || !(i = atoi(port + 1)) )
		return;

	port[0] = 0;
	port++;

	if ( strnull( cvar_string( "serverdemo" ) ) || cvar("sv_demotxt") != 2 )
		return; // does't record demo or does't want stats to be put in file

	// This file over-written every time
	snprintf(name, sizeof(name), "demoinfo_%s_%d.txt", ip, i);

	if (CreateStatsFile(name, ip, i, true));
	{
		localcmd("\n" // why new line?
				 "sv_demoinfoadd ** %s\n", name);
		trap_executecmd();
	}
}

void EM_on_MatchEndBreak( int isBreak )
{
	gedict_t *p;

	for( p = world; (p = find_client( p )); )
		if ( isBreak )
			on_match_break( p );
		else
			on_match_end( p );
}

void EM_CorrectStats()
{
	gedict_t	*p;

	for( p = world; (p = find_plr( p )); )
	{
		// take away powerups so scoreboard looks normal
		p->s.v.items = (int)p->s.v.items & ~(IT_INVISIBILITY | IT_INVULNERABILITY | IT_SUIT | IT_QUAD);
		p->s.v.effects = (int)p->s.v.effects & ~(EF_DIMLIGHT | EF_BRIGHTLIGHT | EF_BLUE | EF_RED | EF_GREEN);
		p->invisible_finished = 0;
		p->invincible_finished = 0;
		p->super_damage_finished = 0;
		p->radsuit_finished = 0;

		p->ps.spree_max = max(p->ps.spree_current, p->ps.spree_max);
		p->ps.spree_max_q = max(p->ps.spree_current_q, p->ps.spree_max_q);

		adjust_pickup_time( &p->q_pickup_time, &p->ps.itm[itQUAD].time );
		adjust_pickup_time( &p->p_pickup_time, &p->ps.itm[itPENT].time );
		adjust_pickup_time( &p->r_pickup_time, &p->ps.itm[itRING].time );

		if ( p->control_start_time )
		{
			p->ps.control_time += g_globalvars.time - p->control_start_time;
			p->control_start_time = 0;
		}

		if ( isCTF() ) { // if a player ends the game with a rune adjust their rune time

			if ( p->ctf_flag & CTF_RUNE_RES )
				p->ps.res_time += g_globalvars.time - p->rune_pickup_time;
			else if ( p->ctf_flag & CTF_RUNE_STR )
				p->ps.str_time += g_globalvars.time - p->rune_pickup_time;
			else if ( p->ctf_flag & CTF_RUNE_HST )
				p->ps.hst_time += g_globalvars.time - p->rune_pickup_time;
			else if ( p->ctf_flag & CTF_RUNE_RGN )
				p->ps.rgn_time += g_globalvars.time - p->rune_pickup_time;
		}
	}
}

// WARNING: if we are skip log, we are also delete demo

void EndMatch ( float skip_log )
{
	gedict_t	*p;

	int old_match_in_progress = match_in_progress;
	char *tmp;
	float f1;

	if( match_over || !match_in_progress )
		return;

	match_over = 1;

	remove_projectiles();

	// s: zero the flag
	k_sudden_death = 0;

	if( !strnull( tmp = cvar_string( "_k_host" ) ) )
		trap_cvar_set( "hostname", tmp ); // restore host name at match end

	trap_lightstyle(0, "m");

	// spec silence
	{
		int fpd = iKey ( world, "fpd" );

		fpd = fpd & ~64;
		localcmd("serverinfo fpd %d\n", fpd);

		cvar_fset("sv_spectalk", 1);
	}

	if ( isHoonyMode() )
		G_bprint( 2, "The point is over\n");
	else if ( deathmatch )
		G_bprint( 2, "The match is over\n");

	EM_CorrectStats();

	if ( k_bloodfest )
	{
		extern void bloodfest_stats(void);

		// remove any powerup left
		for( p = world; (p = nextent(p)); )
		{
			if ( streq( p->s.v.classname, "item_artifact_invulnerability")
					|| streq( p->s.v.classname, "item_artifact_invisibility")
					|| streq( p->s.v.classname, "item_artifact_super_damage")
			   )
			{
				ent_remove( p );
				continue;
			}
		}

		bloodfest_stats();
	}

	if ( /* skip_log || */ !deathmatch )
	{
		;
	}
	else {
		if( isTeam() || isCTF() )
			CollectTpStats();

		if (isRACE()) {
			extern void race_match_stats(void);

			race_match_stats();
		}
		else {
			PlayersStats(); // all info about any player

			if (!cvar("k_midair")) {
				if (isTeam() || isCTF())
					SummaryTPStats(); // print summary stats like armos powerups weapons etc..

				if (!isDuel()) // top stats only in non duel modes
					TopStats(); // print top frags tkills deaths...
			}
			else {
				TopMidairStats();
			}

			if (isTeam() || isCTF())
				TeamsStats(); // print basic info like frags for each team

			if ((p = find(world, FOFCLSN, "ghost"))) // show legend :)
				G_bprint(2, "\n\x83 - %s player\n\n", redtext("disconnected"));

			ListDemoMarkers();
		}

		lastscore_add(); // save game result somewhere, so we can show it later

		StatsToFile();
	}

	for( p = world; (p = find ( p, FOFCLSN, "ghost" )); )
		ent_remove( p );

	StopTimer( skip_log ); // WARNING: if we are skip log, we are also delete demo

	for( f1 = 666; k_teamid >= f1 ; f1++ )
		localcmd("localinfo %d \"\"\n", (int)f1); //removing key

	for( f1 = 1; k_userid >= f1; f1++ )
		localcmd("localinfo %d \"\"\n", (int)f1); //removing key

	if ( old_match_in_progress == 2 ) {
		for ( p = world; (p = find_plr( p )); )
			p->ready = 0; // force players be not ready after match is end.
	}

	EM_on_MatchEndBreak( skip_log );

	if (isHoonyMode()) {
		if ( HM_current_point_type() != HM_PT_FINAL ) {
			match_over = 0;

			// All bots ready first
			for (p = world; (p = find_plr (p)); ) {
				if (p->isBot) {
					p->ready = true;
				}
			}

			for (p = world; (p = find_plr (p)); ) {
				stuffcmd (p, "ready\n");
			}
		}
		else {
			for ( p = world; (p = find_plr( p )); )
				stuffcmd(p, "hmstats\n");
			StopLogs();
			NextLevel();
		}
	}
	else {
		StopLogs();
		NextLevel();
	}

	// allow ready/break in bloodfest without map reloading.
	if ( k_bloodfest )
		match_over = 0;

  g_matchstarttime = 0;
}

void SaveOvertimeStats ()
{
	gedict_t	*p;

	if ( k_overtime ){
		for( p = world; (p = find_plr( p )); )
		{
		 	// save overtime stats
			p->ps.ot_a	    = (int)p->s.v.armorvalue;
			p->ps.ot_items	=      p->s.v.items; // float
			p->ps.ot_h	    = (int)p->s.v.health;
		}
	}
}

void CheckOvertime()
{
	gedict_t	*timer, *ed1 = get_ed_scores1(), *ed2 = get_ed_scores2();
	int teams   = CountTeams(), players = CountPlayers();
	int sc = get_scores1() - get_scores2();
	int k_mb_overtime = cvar( "k_overtime" );
	int k_exttime = bound(1, cvar( "k_exttime" ), 999); // at least some reasonable values

	// If 0 no overtime, 1 overtime, 2 sudden death
	// And if its neither then well we exit
	if( !k_mb_overtime || (k_mb_overtime != 1 && k_mb_overtime != 2 && k_mb_overtime != 3) )
	{
		EndMatch( 0 );
		return;
	}

    // Overtime.
	// Ok we have now decided that the game is ending, so decide overtime wise here what to do.

	if ( (isDuel() || isFFA()) && ed1 && ed2 )
		sc = ed1->s.v.frags - ed2->s.v.frags;

//	if( k_matchLess ) {
//		k_mb_overtime = 0; // no overtime in matchLess mode
//	}
//	else

	if( (isTeam() || isCTF()) && teams != 2 ) {
		k_mb_overtime = 0; // no overtime in case of less then 2 or more then 2 teams
	}
	else if(    ( (isDuel() || isFFA()) && ed1 && ed2 ) // duel or ffa
			 || ( (isTeam() || isCTF()) && teams == 2 && players > 2 ) // Handle a 2v2 or above team game
	)
	{
		if (    ( k_mb_overtime == 3 && abs( sc ) > 1 ) // tie-break overtime allowed with one frag difference (c) ktpro
			 || ( k_mb_overtime != 3 && abs( sc ) > 0 ) // time based or sudden death overtime allowed with zero frag difference
		   )
		k_mb_overtime = 0;
	}
	else
		k_mb_overtime = 0;

	if( !k_mb_overtime )
	{
		EndMatch( 0 );
		return;
	}

	k_overtime = k_mb_overtime;
	SaveOvertimeStats ();

	G_bprint(2, "time over, the game is a draw\n");

	if( k_overtime == 1 ) {
		// Ok its increase time
		self->cnt  =  k_exttime;
		self->cnt2 = 60;
		localcmd("serverinfo status \"%d min left\"\n", (int)self->cnt);

		G_bprint(2, "\x90%s\x91 minute%s overtime follows\n", dig3(k_exttime), count_s(k_exttime));
		self->s.v.nextthink = g_globalvars.time + 1;
	}
	else if ( k_overtime == 2 ) {
		k_sudden_death = SD_NORMAL;
	}
	else {
		k_sudden_death = SD_TIEBREAK;
	}

	if ( k_sudden_death ) {
		G_bprint(2, "%s %s\n", SD_type_str(), redtext("overtime begins"));

		// added timer removal at sudden death beginning
		for( timer = world; (timer = find(timer, FOFCLSN, "timer")); )
			ent_remove( timer );
	}
}

// Called every second during a match. cnt = minutes, cnt2 = seconds left.
// Tells the time every now and then.
void TimerThink ()
{
//	G_bprint(2, "left %2d:%2d\n", (int)self->cnt, (int)self->cnt2);

	if( !k_matchLess && !CountPlayers() ) {
		EndMatch( 1 );
		return;
	}

	if( k_sudden_death )
		return;

	if( self->k_teamnum < g_globalvars.time && !k_checkx )
		k_checkx = 1; // global which set to true when some time spend after match start

	( self->cnt2 )--;

	if( !self->cnt2 ) {
		self->cnt2 = 60;
		self->cnt  -= 1;

		localcmd("serverinfo status \"%d min left\"\n", (int)self->cnt);

		if( !self->cnt ) {
			CheckOvertime();
			return;
		}

		G_bprint(2, "\x90%s\x91 minute%s remaining\n", dig3(self->cnt), count_s(self->cnt));

		self->s.v.nextthink = g_globalvars.time + 1;

		if( k_showscores ) {
			int sc = get_scores1() - get_scores2();

			if ( sc ) {
				G_bprint(2, "%s \x90%s\x91 leads by %s frag%s\n",
						redtext("Team"), cvar_string ( ( sc > 0 ? "_k_team1" : "_k_team2" ) ),
						dig3(abs( (int)sc )), count_s( abs( (int)sc ) ) );
			}
			else
				G_bprint(2, "The game is currently a tie\n");
		}
		return;
	}

	if( self->cnt == 1 && ( self->cnt2 == 30 || self->cnt2 == 15 || self->cnt2 <= 10 ) )
		G_bprint(2, "\x90%s\x91 second%s\n", dig3( self->cnt2 ), count_s( self->cnt2 ) );

	self->s.v.nextthink = g_globalvars.time + 1;
}

void soft_ent_remove (gedict_t* ent)
{
	if (bots_enabled ()) {
		ent->s.v.model = "";
		ent->s.v.solid = SOLID_TRIGGER;
		ent->s.v.nextthink = 0;
		ent->s.v.think = (func_t) SUB_Null;
		ent->s.v.touch = (func_t) marker_touch;
		ent->fb.desire = goal_NULL;
		ent->fb.goal_respawn_time = 0;
	}
	else {
		ent_remove (ent);
	}
}

// remove/add some items from map regardind with dmm and game mode
void SM_PrepareMap()
{
	gedict_t *p;

	if ( isCTF() )
		SpawnRunes( cvar("k_ctf_runes") );

	// this must be removed in any cases
	remove_projectiles();

	for( p = world; (p = nextent(p)); )
	{
		// going for the if content record..
		if (   isRA()
			|| isRACE()
			|| ( deathmatch == 4 && cvar("k_instagib") )
			|| cvar("k_noitems")
			|| k_bloodfest
		   )
		{
			if (
				   streq( p->s.v.classname, "weapon_nailgun" )
				|| streq( p->s.v.classname, "weapon_supernailgun" )
				|| streq( p->s.v.classname, "weapon_supershotgun" )
				|| streq( p->s.v.classname, "weapon_rocketlauncher" )
				|| streq( p->s.v.classname, "weapon_grenadelauncher" )
				|| streq( p->s.v.classname, "weapon_lightning" )
				|| streq( p->s.v.classname, "item_shells" )
				|| streq( p->s.v.classname, "item_spikes" )
				|| streq( p->s.v.classname, "item_rockets" )
				|| streq( p->s.v.classname, "item_cells" )
				|| streq( p->s.v.classname, "item_health" )
				|| streq( p->s.v.classname, "item_armor1")
				|| streq( p->s.v.classname, "item_armor2")
				|| streq( p->s.v.classname, "item_armorInv")
				|| streq( p->s.v.classname, "item_artifact_invulnerability")
				|| streq( p->s.v.classname, "item_artifact_envirosuit")
				|| streq( p->s.v.classname, "item_artifact_invisibility")
				|| streq( p->s.v.classname, "item_artifact_super_damage")
			   )
			{
				soft_ent_remove( p );
				continue;
			}
		}

		if ( deathmatch == 2 )
		{
			if (   streq( p->s.v.classname, "item_armor1" )
			    || streq( p->s.v.classname, "item_armor2" )
			    || streq( p->s.v.classname, "item_armorInv")
			   )
			{
				soft_ent_remove( p );
				continue;
			}
		}

		if ( deathmatch >= 4 )
		{
			if (   streq( p->s.v.classname, "weapon_nailgun" )
				|| streq( p->s.v.classname, "weapon_supernailgun" )
				|| streq( p->s.v.classname, "weapon_supershotgun" )
				|| streq( p->s.v.classname, "weapon_rocketlauncher" )
				|| streq( p->s.v.classname, "weapon_grenadelauncher" )
				|| streq( p->s.v.classname, "weapon_lightning" )
			   )
			{ // no weapons for any of this deathmatches (4 or 5)
				soft_ent_remove( p );
				continue;
			}

			if ( deathmatch == 4 )
			{
				if (   streq( p->s.v.classname, "item_shells" )
					|| streq( p->s.v.classname, "item_spikes" )
					|| streq( p->s.v.classname, "item_rockets" )
					|| streq( p->s.v.classname, "item_cells" )
					|| (streq( p->s.v.classname, "item_health" ) && (( int ) p->s.v.spawnflags & H_MEGA))
			       )
				{ // no weapon ammo and megahealth for dmm4
					soft_ent_remove( p );
					continue;
				}
			}
		}

		if ( k_killquad && streq( p->s.v.classname, "item_artifact_super_damage") )
		{	// no normal quad in killquad mode.
			soft_ent_remove( p );
			continue;
		}
	}

	ClearBodyQue(); // hide corpses
}

// put clients in server and reset some params
void SM_PrepareClients()
{
	int hdc, i;
	char *pl_team;
	gedict_t *p;

	k_teamid = 666;
	localcmd("localinfo 666 \"\"\n");
	trap_executecmd (); // <- this really needed

	for( p = world;	(p = find_plr( p )); ) {
		if( !k_matchLess ) { // skip setup k_teamnum in matchLess mode
			pl_team = getteam( p );
			p->k_teamnum = 0;

			if( !strnull( pl_team ) ) {
				i = 665;

				while( k_teamid > i && !p->k_teamnum ) {
					i++;

					if( streq( pl_team, ezinfokey(world, va("%d", i)) ) )
						p->k_teamnum = i;
				}

				if( !p->k_teamnum ) { // team not found in localinfo, so put it in
					i++;
					p->k_teamnum = k_teamid = i;
					localcmd( "localinfo %d \"%s\"\n", i, pl_team );
					trap_executecmd (); // <- this really needed
				}
			}
			else
				p->k_teamnum = 666;
		}

		if (!isHoonyMode())
			p->friendly = p->deaths = p->s.v.frags = 0;

		hdc = p->ps.handicap; // save player handicap

		memset( (void*) &( p->ps ), 0, sizeof(p->ps) ); // clear player stats

		WS_Reset( p ); // force reset "new weapon stats"

		p->ps.handicap = hdc; // restore player handicap

		if ( isRA() )
		{
			if ( isWinner( p ) || isLoser( p ) )
				setfullwep( p );

			continue;
		}

		// ignore k_respawn() in case of coop unless bloodfest
		if ( !deathmatch && !k_bloodfest )
			continue;

		// ignore  k_respawn() in case of CA
		if ( isCA() )
			continue;

		// ignore  k_respawn() in case of race mode
		if ( isRACE() )
			continue;

		k_respawn( p, false );
	}
}

void SM_PrepareShowscores()
{
	gedict_t *p;
	char *team1 = "", *team2 = "";

	if ( k_matchLess ) // skip this in matchLess mode
		return;

	if ( (!isTeam() && !isCTF()) || CountRTeams() != 2 ) // we need 2 teams
		return;

	if ( (p = find_plr( world )) )
		team1 = getteam( p );

	if ( strnull( team1 ) )
		return;

	while( (p = find_plr( p )) ) {
		team2 = getteam( p );

		if( strneq( team1, team2 ) )
			break;
	}

	if ( strnull( team2 ) || streq(team1, team2) )
		return;

	k_showscores = 1;

	cvar_set("_k_team1", team1);
	cvar_set("_k_team2", team2);
}

void SM_PrepareHostname()
{
	char *team1 = cvar_string("_k_team1"), *team2 = cvar_string("_k_team2");

	cvar_set( "_k_host", cvar_string("hostname") );  // save host name at match start

	if ( k_showscores && !strnull( team1 ) && !strnull( team2 ) )
		cvar_set("hostname", va("%s (%.4s vs. %.4s)\x87", cvar_string("hostname"), team1, team2));
	else
		cvar_set("hostname", va("%s\x87", cvar_string("hostname")));
}

void SM_PrepareTeamsStats()
{
	int i;

	tmStats_cnt = 0;
	memset(tmStats, 0, sizeof(tmStats));
	memset(tmStats_names, 0, sizeof(tmStats_names));

	for ( i = 0; i < MAX_TM_STATS; i++ )
		tmStats[i].name = tmStats_names[i];
}

void SM_on_MatchStart()
{
	gedict_t *p;

	for( p = world; (p = find_client( p )); )
		on_match_start( p );
}

// Reset player frags and start the timer.
void HideSpawnPoints();

void StartMatch ()
{
	char date[64];

	// reset bloodfest vars.
	bloodfest_reset();

	k_nochange   = 0;
	k_showscores = 0;
	k_standby    = 0;
	k_checkx     = 0;

	k_userid   = 1;
	localcmd("localinfo 1 \"\"\n");
	trap_executecmd (); // <- this really needed

	first_rl_taken = false; // no one took rl yet

	SM_PrepareMap(); // remove/add some items from map regardind with dmm and game mode

	HideSpawnPoints();

	match_start_time  = g_globalvars.time;
  g_matchstarttime = (int) (g_globalvars.time*1000);
	match_in_progress = 2;

	lastTeamLocationTime = -TEAM_LOCATION_UPDATE_TIME; // update on next frame

	remove_specs_wizards (); // remove wizards

	if (isHoonyMode())
		HM_rig_the_spawns(1, 0);
	SM_PrepareClients(); // put clients in server and reset some params
	if (isHoonyMode())
		HM_rig_the_spawns(0, 0);

	if ( !QVMstrftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S %Z", 0) )
		date[0] = 0;

	if ( deathmatch && (!isHoonyMode() || HM_current_point() == 0))
	{
		if ( date[0] )
			G_bprint(2, "matchdate: %s\n", date);

		if ( !k_matchLess || cvar( "k_matchless_countdown" ) )
			G_bprint(2, "%s\n", redtext("The match has begun!"));
	}

// spec silence
	{
		int fpd = iKey( world, "fpd" );
		int k_spectalk = ( coop ? 1 : bound(0, cvar( "k_spectalk" ), 1) );
		cvar_fset( "sv_spectalk", k_spectalk );

		fpd = ( k_spectalk ) ? (fpd & ~64) : (fpd | 64);

		localcmd( "serverinfo fpd %d\n", fpd );
	}

	self->k_teamnum = g_globalvars.time + 3;  //dirty i know, but why waste space?
											  // FIXME: waste space, but be clean
	self->cnt = bound(0, timelimit, 9999);
	self->cnt2 = 60;
	localcmd("serverinfo status \"%d min left\"\n", (int)timelimit);

	self->s.v.think = ( func_t ) TimerThink;
	self->s.v.nextthink = g_globalvars.time + 1;

	SM_PrepareShowscores();

	SM_PrepareHostname();

	SM_PrepareTeamsStats();

	SM_PrepareCA();

	SM_on_MatchStart();

	ClearDemoMarkers();

	StartLogs();

	BotsMatchStart ();

	if ( !self->cnt )
		ent_remove( self ); // timelimit == 0, so match will end no due to timelimit but due to fraglimit or something

	if ( isRACE() )
		race_match_start();

	cvar_fset("qtv_sayenabled", 0);
}

// just check if someone using handicap
static qbool handicap_in_use(void)
{
	gedict_t *p;
	int from;

	for ( from = 0, p = world; (p = find_plrghst ( p, &from )); )
		if ( GetHandicap( p ) != 100 )
			return true;

	return false;
}

void PrintCountdown( int seconds )
{
// Countdown: seconds
//
//
// EQL semifinal
//
// Deathmatch  x
// Mode		  D u e l | T e a m | F F A | C O O P | BLOODFST | C T F | RA | CA
// Spawnmodel KTX | bla bla bla // optional
// Antilag    On|Off
// NoItems    On // optional
// Midair     On // optional
// Instagib   On // optional
// Yawnmode   On // optional
// Airstep    On // optional
// TmOverlay  On // optional
// Teamplay    x
// Timelimit  xx
// Fraglimit xxx
// Overtime   xx		Overtime printout, supports sudden death display
// Powerups   On|Off|QPRS
// Dmgfrags   On // optional
// Noweapon

// Handicap in use // optional

	char text[1024] = {0};
	char *mode = "";
	char *ot   = "";
	char *nowp = "";
	char *matchtag = ezinfokey(world, "matchtag");


	strlcat(text, va("%s: %2s\n\n\n", redtext("Countdown"), dig3(seconds)), sizeof(text));

	if (matchtag[0])
		strlcat(text, va("%s\n\n\n", matchtag), sizeof(text));

	if ( !isRA() && !coop && !isRACE() ) // useless in RA
		strlcat(text, va("%s %2s\n", "Deathmatch", dig3(deathmatch)), sizeof(text));

	if ( k_bloodfest )
		mode = redtext("BLOODFST");
	else if ( coop )
		mode = redtext("C O O P");
	else if ( isRA() )
		mode = redtext("RA");
	else if ( isCA() )
		mode = redtext("CA");
	else if( isHoonyMode() )
		mode = redtext("Hoony");
	else if( isRACE() )
		mode = redtext("R A C E");
	else if( isDuel() )
		mode = redtext("D u e l");
	else if ( isTeam() )
		mode = redtext("T e a m");
	else if ( isFFA() )
		mode = redtext("F F A");
	else if ( isCTF() )
		mode = redtext("C T F");
	else
		mode = redtext("Unknown");

	strlcat(text, va("%s %8s\n", "Mode", mode), sizeof(text));

	if ( isCA() )
		strlcat(text, va("%s %8s\n", "Wins", dig3(CA_wins_required())), sizeof(text));
	if (isRACE())
	{
		if (race.round_number >= race.rounds) {
			strlcat(text, va("Round %7s\n", redtext("final")), sizeof(text));
		}
		strlcat(text, va("%s %9s\n", "Pts", race_scoring_system_name()), sizeof(text));
	}

//	if ( cvar( "k_spw" ) != 3 )
	if ( ! isRACE() )
		strlcat(text, va("%s %4s\n", "Respawns", respawn_model_name_short( cvar( "k_spw" ) )), sizeof(text));

	if (cvar("sv_antilag"))
		strlcat(text, va("%s %5s\n", "Antilag", dig3((int)cvar("sv_antilag"))), sizeof(text));

	if ( cvar("k_noitems") && !isRACE() )
		strlcat(text, va("%s %5s\n", "NoItems", redtext("on")), sizeof(text));

	if ( cvar("k_midair") )
		strlcat(text, va("%s %6s\n", "Midair", redtext("on")), sizeof(text));

	if ( cvar("k_instagib") )
		strlcat(text, va("%s %4s\n", "Instagib", redtext("on")), sizeof(text));

	if ( k_yawnmode )
		strlcat(text, va("%s %4s\n", "Yawnmode", redtext("on")), sizeof(text));

	if ( cvar("pm_airstep") )
		strlcat(text, va("%s %5s\n", "Airstep", redtext("on")), sizeof(text));

	vw_enabled = vw_available && cvar("k_allow_vwep") && cvar("k_vwep");
	if ( vw_enabled && ! isRACE() )
		strlcat(text, va("%s %8s\n", "VWep", redtext("on")), sizeof(text));

	if ( cvar("k_teamoverlay") && tp_num() && !isDuel() && ! isRACE() )
		strlcat(text, va("%s %3s\n", "TmOverlay", redtext("on")), sizeof(text));

	if ( !isRA() ) // useless in RA
	if ( isTeam() || isCTF() )
		strlcat(text, va("%s %4s\n", "Teamplay", dig3(teamplay)), sizeof(text));
	if ( timelimit )
		strlcat(text, va("%s %3s\n", "Timelimit", dig3(timelimit)), sizeof(text));
	if ( fraglimit )
		strlcat(text, va("%s %3s\n", "Fraglimit", dig3(fraglimit)), sizeof(text));

	switch ( (int)cvar( "k_overtime" ) ) {
		case 0:  ot = redtext("off"); break;
		case 1:  ot = dig3( cvar( "k_exttime" ) ); break;
		case 2:  ot = redtext("sd"); break;
		case 3:  ot = va("%s %s", dig3(tiecount()),redtext("tb")); break;
		default: ot	= redtext("Unkn"); break;
	}

	if ( timelimit && cvar( "k_overtime" ) )
		strlcat(text, va("%s %4s\n", "Overtime", ot), sizeof(text));

	if ( !isRA() && Get_Powerups() && strneq("off", Get_PowerupsStr()) )
		strlcat(text, va("%s %4s\n", "Powerups", redtext(Get_PowerupsStr())), sizeof(text));

	if ( cvar("k_dmgfrags") )
		strlcat(text, va("%s %4s\n", "Dmgfrags", redtext("on")), sizeof(text));

	if ( deathmatch == 4 && !cvar("k_midair") && !cvar("k_instagib")
		&& !strnull( nowp = str_noweapon((int)cvar("k_disallow_weapons") & DA_WPNS) )
	   )
		strlcat(text, va("\n%s %4s\n", "Noweapon",
					redtext(nowp[0] == 32 ? (nowp+1) : nowp)), sizeof(text));

	if ( handicap_in_use() )
		strlcat(text, "\n"
					  "Handicap in use\n", sizeof(text));

	if (isHoonyMode())
	{
		if ((HM_current_point() % 2 == 0))
			strlcat(text, va("\n%-13s\n", redtext("New spawns")), sizeof(text));
		else
			strlcat(text, va("\n%-13s\n", redtext("Switch spawns")), sizeof(text));
	}

	G_cp2all(text);
}

qbool isCanStart ( gedict_t *s, qbool forceMembersWarn )
{
	int k_lockmin     = ( isCA() || isRACE() ) ? 2 : cvar( "k_lockmin" );
	int k_lockmax     = ( isCA() || isRACE() ) ? 2 : cvar( "k_lockmax" );
	int k_membercount = cvar( "k_membercount" );
	int i = CountRTeams();
	int sub, nready;
	char *txt = "";
	gedict_t *p;

	// no limits in RA
	if ( isRA() )
		return true;

	// some limits in duel...
	if ( isDuel() )
	{
		sub = CountPlayers() - 2;

		if ( sub > 0 ) // we need two players in duel...
		{
			txt = va("Get rid of %d player%s!\n", sub, count_s( sub ));

			if ( s )
            	G_sprint(s, 2, "%s", txt);
			else
            	G_bprint(2, "%s", txt);

			return false;
		}
	}

 	// no team/members rules limitation in non team game
	if ( !isTeam() && !isCTF() )
		return true;

	// below Team or CTF game modes and limits...

	for( p = world; (p = find_plr( p )); )
	{
		if ( strnull( getteam(p) ) )
		{
			txt = "Get rid of players with empty team\n";

			if ( s )
        		G_sprint(s, 2, "%s", txt);
			else
        		G_bprint(2, "%s", txt);

			return false;
		}
	}

    if( i < k_lockmin )
    {
		sub = k_lockmin - i;
		txt = va("%d more team%s required!\n", sub, count_s( sub ));

		if ( s )
        	G_sprint(s, 2, "%s", txt);
		else
        	G_bprint(2, "%s", txt);

		return false;
	}

	if( i > k_lockmax )
	{
		sub = i - k_lockmax;
		txt = va("Get rid of %d team%s!\n", sub, count_s( sub ));

		if ( s )
        	G_sprint(s, 2, "%s", txt);
		else
        	G_bprint(2, "%s", txt);

        	return false;
	}

	nready = 0;
	for( p = world; (p = find_plr( p )); )
		if( p->ready )
			nready++;

	if ( !CheckMembers( k_membercount ) ) {
		if( !forceMembersWarn ) // warn anyway if we want
		if( nready != k_attendees && !s )
			return false; // inform not in all cases, less annoying

		txt = va("%s %d %s\n"
				 "%s\n",
			 redtext("Server wants at least"), k_membercount, redtext("players in each team"),
			 redtext("Waiting..."));

		if ( s )
        	G_sprint(s, 2, "%s", txt);
		else
        	G_bprint(2, "%s", txt);

		return false;
	}

	if ( isCTF() )
	{
		// can't really play ctf if map doesn't have flags
		gedict_t *rflag = find( world, FOFCLSN, "item_flag_team1" );
		gedict_t *bflag = find( world, FOFCLSN, "item_flag_team2" );

		if ( !rflag || !bflag )
		{
			txt = "This map does not support CTF mode\n";

			if ( s )
        		G_sprint(s, 2, "%s", txt);
			else
        		G_bprint(2, "%s", txt);

			return false;
		}
	}

	return true;
}

void standby_think()
{
	gedict_t *p;

	if ( match_in_progress == 1 && !isRA() ) {

		k_standby = 1;

		for( p = world;	(p = find_plr( p )); ) {
			if( !strnull ( p->s.v.netname ) ) {
				//set to ghost, 0.2 second before matchstart
				p->s.v.takedamage = 0;
				p->s.v.solid      = 0;
				p->s.v.movetype   = 0;
				p->s.v.modelindex = 0;
				p->s.v.model      = "";
			}
		}
	}

	ent_remove ( self );
}

// Called every second during the countdown.
void TimerStartThink ()
{
	gedict_t *p;

	k_attendees = CountPlayers();

	if( !isCanStart( NULL, true ) ) {
		G_bprint(2, "Aborting...\n");

		StopTimer( 1 );

		return;
	}

	self->cnt2 -= 1;

	if( self->cnt2 == 1 ) {
		p = spawn();
		p->s.v.owner = EDICT_TO_PROG( world );
		p->s.v.classname = "standby_th";
		p->s.v.nextthink = g_globalvars.time + 0.8;
		p->s.v.think = ( func_t ) standby_think;
	}
    else if( self->cnt2 <= 0 ) {
		G_cp2all("");

		StartMatch();

		return;
	}

	PrintCountdown( self->cnt2 );

	if( self->cnt2 < 6 )
	{
		char *gr = redtext("Get ready");

		for( p = world; (p = find_client( p )); )
		{
			if ( p->ct == ctPlayer && !p->ready )
			{
				G_sprint(p, 2, "%s!\n", gr);
			}

			stuffcmd (p, "play buttons/switch04.wav\n");
		}
	}

	self->s.v.nextthink = g_globalvars.time + 1;
}


void ShowMatchSettings()
{
	int i;
	char *txt = "";

//	G_bprint(2, "Spawnmodel: %s\n", redtext( respawn_model_name( cvar( "k_spw" ) ) ));

// changed to print only if other than default

	if( (i = get_fair_pack()) ) {
		// Output the Fairpack setting here
		switch ( i ) {
			case  0: txt = "off"; break;
			case  1: txt = "best weapon"; break;
			case  2: txt = "last weapon fired"; break;
			default: txt = "!Unknown!"; break;
		}

		G_bprint(2, "Fairpacks setting: %s\n", redtext(txt));
	}

// print qizmo ( FPD ) settings
	i = iKey( world, "fpd" );
	if( i & 170 ) {
		char buf[256] = {0};

		if( i & 2 )
			strlcat(buf, " timer", sizeof(buf));
		if( i & 8 )
			strlcat(buf, " lag", sizeof(buf));
		if( i & 32 )
			strlcat(buf, " enemy", sizeof(buf));
		if( i & 128 )
			strlcat(buf, " point", sizeof(buf));

		G_bprint(2, "QiZmo:%s disabled\n", redtext(buf));
	}
}

// duel_dag_vs_zu-zu[dm3]
// team_no!_vs_fom[dm3]
// ctf_no!_vs_fom[dm3]
// ffa_10[dm3] // where 10 is count of players
// ra_10[dm3] // where 10 is count of players
// unknown_10[dm3] // where 10 is count of players

char *CompilateDemoName ()
{
	static char demoname[60];
	char date[128], *fmt;

	int i;
	gedict_t *p;
	char *name, *vs;

	demoname[0] = 0;
	if ( isRA() )
	{
		strlcat( demoname, va("ra_%d", (int)CountPlayers()), sizeof( demoname ) );
	}
	else if ( isRACE() && ! race_match_mode() )
	{
		int players = 0;

		strlcat( demoname, "race", sizeof( demoname ) );
		for( vs = "_", p = world; (p = find_plr( p )); )
		{
			if ( strnull( name = getname( p ) ) || !( p->racer ) )
				continue;

			if (players < 2) {
				strlcat(demoname, vs, sizeof(demoname));
				strlcat(demoname, name, sizeof(demoname));
			}
			else if (players == 2) {
				strlcat(demoname, vs, sizeof(demoname));
				strlcat(demoname, "others", sizeof(demoname));
			}
			++players;
		}
	}
	else if ( isDuel() )
	{
		strlcat( demoname, "duel", sizeof( demoname ) );
		if ( isRACE() )
			strlcat( demoname, "_race", sizeof( demoname ) );
		if ( cvar("k_midair") )
			strlcat( demoname, "_midair", sizeof( demoname ) );
		if ( cvar("k_instagib") )
			strlcat( demoname, "_instagib", sizeof( demoname ) );

		for( vs = "_", p = world; (p = find_plr( p )); )
		{
			if ( strnull( name = getname( p ) ) )
				continue;
			if ( isRACE() && !( p->race_participant ) )
				continue;

			strlcat( demoname, vs, sizeof( demoname ) );
			strlcat( demoname, name, sizeof( demoname ) );
			vs = "_vs_";
		}
	}
	else if ( isTeam() || isCTF() )
	{
		char teams[MAX_CLIENTS][MAX_TEAM_NAME];
		int cnt = getteams(teams);
		int clt = cvar("maxclients"); //CountPlayers();

		// guess is this XonX
		if ( clt > 1 && cnt > 1 && !(clt % cnt) )
			clt /= cnt; // yes
		else
			clt = 0; // no

		strlcat( demoname, (isTeam() ? (clt ? va("%don%d", clt, clt) : "team"): "ctf"), sizeof( demoname ) );
		if ( isRACE() )
			strlcat( demoname, "_race", sizeof( demoname ) );

		for( vs = "_", i = 0; i < MAX_CLIENTS; i++ )
		{
			if ( strnull( teams[i] ) )
				break;

			strlcat( demoname, vs, sizeof( demoname ) );
			strlcat( demoname, teams[i], sizeof( demoname ) );
			vs = "_vs_";
		}
	}
	else if ( isFFA() ) {
		if ( isRACE() )
			strlcat( demoname, "race_", sizeof( demoname ) );
		strlcat( demoname, va("ffa_%d", (int)CountPlayers()), sizeof( demoname ) );
	}
	else {
		if ( isRACE() )
			strlcat( demoname, "race_", sizeof( demoname ) );
		strlcat( demoname, va("unknown_%d", (int)CountPlayers()), sizeof( demoname ) );
	}

	if ( isRACE() )
		strlcat( demoname, va("[%s_r%02d]", g_globalvars.mapname, race.active_route), sizeof( demoname ) );
	else
		strlcat( demoname, va("[%s]", g_globalvars.mapname), sizeof( demoname ) );

	fmt = cvar_string( "k_demoname_date" );

	if ( !strnull( fmt ) && QVMstrftime(date, sizeof(date), fmt, 0) )
		strlcat( demoname, date, sizeof( demoname ) );

	return demoname;
}

void StartDemoRecord ()
{
	char *demoname;

	// extralog should be set by easyrecord and if we skip recording we will have it set to WRONG value.
	// So this set it at least to something reasonable ffs.
	cvar_set( "extralogname", "" );

	if ( cvar( "demo_tmp_record" ) )
	{ // FIXME: TODO: make this more like ktpro
		qbool record = false;

		if ( isRACE() )
			record = true;
		else if ( !deathmatch )
			record = false;
		else if ( isFFA() && cvar( "demo_skip_ktffa_record" ) )
			record = false;
		else if ( isHoonyMode() && HM_current_point() > 0 )
			record = false; // don't try to record (segfault) when already recording
		else
			record = true;

		if ( record )
		{
			if( !strnull( cvar_string( "serverdemo" ) ) )
				localcmd("cancel\n");  // demo is recording, cancel before new one

			demoname = CompilateDemoName();
			localcmd( "easyrecord \"%s\"\n", demoname );
		}
	}
}

// Spawns the timer and starts the countdown.
void StartTimer ()
{
	gedict_t *timer;

	if ( match_in_progress || intermission_running || match_over )
		return;

	if ( k_matchLess && !CountPlayers() )
		return; // can't start countdown in matchless mode due to no players,

	k_force = 0;

	for( timer = world; (timer = find(timer, FOFCLSN, "idlebot")); )
		ent_remove( timer );

	for( timer = world; (timer = find(timer, FOFCLSN, "timer")); )
		ent_remove( timer );

	for( timer = world; (timer = find(timer, FOFCLSN, "standby_th")); )
		ent_remove( timer );

	if ( !k_matchLess )
	{
		ShowMatchSettings ();
	}

	if ( !k_matchLess || k_bloodfest )
	{
		for( timer = world; (timer = find_client( timer )); )
			stuffcmd(timer, "play items/protect2.wav\n");
	}

	timer = spawn();
	timer->s.v.owner = EDICT_TO_PROG( world );
	timer->s.v.classname = "timer";
	timer->cnt = 0;

    timer->cnt2 = max(3, (int)cvar( "k_count" ));  // at the least we want a 3 second countdown

	if ( isHoonyMode() && HM_current_point() > 0)
		timer->cnt2 = 3; // first point gets usual 10 seconds, next points gets less

	if ( k_bloodfest )
	{
		// at the least 5 second countdown in bloodfest mode.
		timer->cnt2 = max(5, (int)cvar( "k_count" ));
	}
	else if ( !deathmatch )
	{
		// no countdown in coop or similar modes.
		timer->cnt2 = 0;
	}
	else if ( k_matchLess )
	{
		if ( !cvar("k_matchless_countdown") )
			timer->cnt2 = 0; // no countdown if variable is not specified.
	}

	( timer->cnt2 )++;

    timer->s.v.nextthink = g_globalvars.time + 0.001;
	timer->s.v.think = ( func_t ) TimerStartThink;

	match_in_progress = 1;

	localcmd( "serverinfo status Countdown\n" );

	StartDemoRecord (); // if allowed
}

// Whenever a countdown or match stops, remove the timer and reset everything.
// also stop/cancel demo recording
void StopTimer ( int removeDemo )
{
	gedict_t *timer, *p;
	int k_demo_mintime = bound(0, cvar("k_demo_mintime"), 3600);

	if ( k_demo_mintime <= 0 )
		k_demo_mintime = 120; // 120 seconds is default

	if ( match_in_progress == 1 )
		G_cp2all(""); // clear center print

	k_force = 0;
	match_in_progress = 0;

	if ( k_standby )
	{
		// Stops the bug where players are set to ghosts 0.2 second to go and countdown aborts.
		// standby flag needs clearing (sturm)
		k_standby = 0;

		for( p = world; (p = find_plr( p )); )
		{
			p->s.v.takedamage = 2;
			p->s.v.solid      = 3;
			p->s.v.movetype   = 3;
			setmodel (p, "progs/player.mdl");
		}
	}

	for( timer = world; (timer = find(timer, FOFCLSN, "timer")); )
		ent_remove( timer );

	for( timer = world; (timer = find(timer, FOFCLSN, "standby_th")); )
		ent_remove( timer );

	if (   removeDemo
		&& ( !match_start_time || (g_globalvars.time - match_start_time ) < k_demo_mintime )
		&& ( !isRACE() || race_can_cancel_demo() )
		&& !strnull( cvar_string( "serverdemo" ) )
	   )
		localcmd("cancel\n");  // demo is recording and must be removed, do it

	match_start_time = 0;

	if (isHoonyMode() && HM_current_point_type() != HM_PT_FINAL )
	{
		; // do not set to Standby during points, (unless its the final point of course)
	}
	else
	{
		localcmd("serverinfo status Standby\n");
	}
}

void IdlebotForceStart ()
{
    gedict_t *p;
    int i;

    G_bprint ( 2, "server is tired of waiting\n"
				  "match WILL commence!\n" );

    i = 0;
    for( p = world; (p = find_plr( p )); )
    {
		if( p->ready ) {
    		i++;
		}
		else
		{
    		G_bprint(2, "%s was kicked by IDLE BOT\n", p->s.v.netname);
    		G_sprint(p, 2, "Bye bye! Pay attention next time.\n");

    		stuffcmd(p, "disconnect\n"); // FIXME: stupid way
		}
    }

    k_attendees = i;

    if( k_attendees > 1 ) {
        StartTimer();
	}
    else
    {
        G_bprint(2, "Can't start! More players needed.\n");
		EndMatch( 1 );
    }
}

void IdlebotThink ()
{
	gedict_t *p;
	int i;

	if ( cvar( "k_idletime" ) <= 0 ) {
		ent_remove( self );
		return;
	}

	self->attack_finished -= 1;

	i = CountPlayers();

	if( 0.5f * i > CountRPlayers() || i < 2 ) {
		G_bprint(2, "console: bah! chickening out?\n"
					"server disables the %s\n", redtext("idle bot"));

		ent_remove( self ) ;

		return;
	}

	k_attendees = CountPlayers();

	if ( !isCanStart(NULL, true) ) {
        G_bprint(2, "%s removed\n", redtext("idle bot"));

        ent_remove ( self );

        return;
	}

	if( self->attack_finished < 1 ) {

		IdlebotForceStart();

		ent_remove( self );

		return;

	} else {
		i = self->attack_finished;

		if( i < 5 || !(i % 5) ) {
			for( p = world; (p = find_plr( p )); )
				if( !p->ready )
					G_sprint(p, 2, "console: %d second%s to go ready\n", i, ( i == 1 ? "" : "s" ));
		}
	}

	self->s.v.nextthink = g_globalvars.time + 1;
}

void IdlebotCheck ()
{
	gedict_t *p;
	int i;

	if ( cvar( "k_idletime" ) <= 0 ) {
		if ( (p = find ( world, FOFCLSN, "idlebot" )) )
			ent_remove( p );
		return;
	}

	i = CountPlayers() - CountBots();

	if( 0.5f * i > CountRPlayers() || i < 2 ) {
		p = find ( world, FOFCLSN, "idlebot" );

		if( p ) {
			G_bprint(2, "console: bah! chickening out?\n"
						"server disables the %s\n", redtext("idle bot"));

			ent_remove( p );
		}

		return;
	}

	if( match_in_progress || intermission_running || k_force )
		return;

	// no idele bot in practice mode
	if ( k_practice ) // #practice mode#
		return;

	if( (p = find ( world, FOFCLSN, "idlebot" )) ) // already have idlebot
		return;

	//50% or more of the players are ready! go-go-go

	k_attendees = CountPlayers();

	if ( !isCanStart( NULL, true ) ) {
        G_sprint(self, 2, "Can't issue %s!\n", redtext("idle bot"));
		return;
	}

	p = spawn();
	p->s.v.classname = "idlebot";
	p->s.v.think = (func_t) IdlebotThink;
	p->s.v.nextthink = g_globalvars.time + 0.001;

	p->attack_finished = max( 3, cvar( "k_idletime" ) );

	G_bprint(2, "\n"
				"server activates the %s\n", redtext("idle bot"));
}

void CheckAutoXonX(qbool use_time);
void r_changestatus( float t );

// Called by a player to inform that (s)he is ready for a match.
void PlayerReady ()
{
	gedict_t *p;
	float nready;

	if ( isRACE() && ! race_match_mode() )
	{
		r_changestatus( 1 ); // race_ready
		return;
	}

	if ( self->ct == ctSpec && ! isRACE() ) {

		if ( !cvar("k_auto_xonx") || k_matchLess ) {
			G_sprint(self, 2, "Command not allowed\n");
			return;
		}

		if( self->ready ) {
			G_sprint(self, 2, "Type break to unready yourself\n");
			return;
		}

		self->ready = 1;

		for( p = world; (p = (match_in_progress ? find_spc( p ) : find_client( p ))); )
			G_sprint(p, 2, "%s %s to play\n", self->s.v.netname, redtext("desire"));

		CheckAutoXonX(g_globalvars.time < 10 ? true : false); // force switch mode asap if possible after some time spent

		return;
	}

	if ( intermission_running || match_in_progress == 2 || match_over )
		return;

	if ( k_practice && !isRACE() ) { // #practice mode#
		G_sprint(self, 2, "%s\n", redtext("Server in practice mode"));
		return;
	}

	if ( self->ready ) {
		G_sprint(self, 2, "Type break to unready yourself\n");
		return;
	}

    if ( isCTF() )
	{
		if ( !streq(getteam(self), "red") && !streq(getteam(self), "blue") )
		{
			G_sprint( self, 2, "You must be on team red or blue for CTF\n" );
			return;
		}
	}

    if ( k_force && ( isTeam() || isCTF() )) {
		nready = 0;
		for( p = world; (p = find_plr( p )); ) {
			if( p->ready ) {
				if( streq( getteam(self), getteam(p) ) && !strnull( getteam(self) ) ){
					nready = 1;
					break;
				}
			}
		}

		if( !nready ) {
			G_sprint(self, 2, "Join an existing team!\n");
			return;
		}
	}

	// do not allow empty team in team mode, because it cause problems
	if ( ( isTeam() || isCTF() ) && strnull( getteam(self) ) ) {
		G_sprint(self, 2, "Set your %s before ready!\n", redtext("team"));
		return;
	}

	if ( GetHandicap(self) != 100 )
		G_sprint(self, 2, "\x87%s you are using handicap!\n", redtext( "WARNING:" ));

	self->ready = 1;
	self->v.brk = 0;
	self->k_teamnum = 0;

	// force red or blue color if ctf
	if ( isCTF() )
	{
		if ( streq( getteam(self), "blue" ) )
			stuffcmd_flags( self, STUFFCMD_IGNOREINDEMO, "color 13\n" );
		else if ( streq( getteam(self), "red" ) )
			stuffcmd_flags( self, STUFFCMD_IGNOREINDEMO, "color 4\n" );
	}

	if (!isHoonyMode() || HM_current_point() == 0)
		G_bprint(2, "%s %s%s\n", self->s.v.netname, redtext("is ready"),
						( ( isTeam() || isCTF() ) ? va(" \x90%s\x91", getteam( self ) ) : "" ) );

	nready = CountRPlayers();
	k_attendees = CountPlayers();

	if ( match_in_progress )
		return; // possible in bloodfest.

	if ( !isCanStart ( NULL, false ) )
		return; // rules does't allow us to start match, idlebot ignored because of same reason

	if ( k_force )
		return; // admin forces match - timer will started somewhere else

	// we ignore "all players ready" and "at least two players ready" checks in bloodfest mode.
	if ( !k_bloodfest )
	{
		if( nready != k_attendees )
		{
			// not all players ready, check idlebot and return
			IdlebotCheck();
			return;
		}

		// ok all players ready.
		// only one or less players ready, match is pointless.
		if ( nready < 2 )
			return;
	}

	if ( isHoonyMode() && k_attendees && nready == k_attendees )
	{
		HM_all_ready();
	}
	else
	{
		if ( k_attendees && nready == k_attendees )
			G_bprint(2, "All players ready\n");
		G_bprint(2,	"Timer started\n");
	}

	StartTimer();
}

void PlayerBreak ()
{
	int votes;
	gedict_t *p;

	if ( isRACE() && ! race_match_mode() )
	{
		r_changestatus( 2 ); // race_break
		return;
	}

	if ( self->ct == ctSpec && ! isRACE() )
	{
		if ( !cvar("k_auto_xonx") || k_matchLess )
		{
			G_sprint(self, 2, "Command not allowed\n");
			return;
		}

		if( !self->ready )
			return;

		self->ready = 0;

		for( p = world; (p = (match_in_progress ? find_spc( p ) : find_client( p ))); )
			G_sprint(p, 2, "%s %s to play\n", self->s.v.netname, redtext("lost desire"));

		return;
	}

	if( !self->ready || intermission_running || match_over )
		return;

	if ( k_matchLess && !k_bloodfest )
	{
		// do not allow break/next_map commands in some cases.
		if ( cvar("k_no_vote_map") )
		{
			G_sprint(self, 2, "Voting next map is %s allowed\n", redtext("not"));
			return;
		}
	}

	if( !match_in_progress )
	{
		self->ready = 0;

		G_bprint(2, "%s %s\n", self->s.v.netname, redtext("is not ready"));

		return;
	}

	if( !k_matchLess || k_bloodfest )
	{
		// try stop countdown.
		if( match_in_progress == 1 )
		{
			p = find ( world, FOFCLSN, "timer");

			if( p && p->cnt2 > 1 )
			{
				self->ready = 0;

				if ( !k_matchLess || ( k_bloodfest && CountRPlayers() < 1 ) )
				{
					G_bprint(2, "%s %s\n", self->s.v.netname, redtext("stops the countdown"));
					StopTimer( 1 );
				}
				else
				{
					G_bprint(2, "%s %s\n", self->s.v.netname, redtext("is not ready"));
				}
			}

			return;
		}
	}

	if( self->v.brk )
	{
		self->v.brk = 0;

		G_bprint(2, "%s %s %s vote%s\n", self->s.v.netname,
				redtext("withdraws"), redtext(g_his(self)),
				((votes = get_votes_req( OV_BREAK, true )) ? va(" (%d)", votes) : ""));

		return;
	}

	self->v.brk = 1;

	G_bprint(2, "%s %s%s\n", self->s.v.netname, redtext(k_matchLess ? "votes for next map" : "votes for stopping the match"),
				((votes = get_votes_req( OV_BREAK, true )) ? va(" (%d)", votes) : ""));

	// show warning to player - that he can't stop countdown alone in matchless mode.
	if ( k_matchLess && match_in_progress == 1 && CountPlayers() == 1 )
	{
		G_sprint(self, 2, "You can't stop countdown alone\n");
	}

	vote_check_break ();
}

