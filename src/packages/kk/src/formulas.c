/* This file was taken from XaoS-the fast portable realtime interactive 
   fractal zoomer. but it is simplified for BB. You may get complette
   sources at XaoS homepage (http://www.paru.cas.cz/~hubicka/XaoS
 */
/* 
 *     XaoS, a fast portable realtime fractal zoomer 
 *                  Copyright (C) 1996,1997 by
 *
 *      Jan Hubicka          (hubicka@paru.cas.cz)
 *      Thomas Marsh         (tmarsh@austin.ibm.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _MAC
#include "aconfig.h"
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <math.h>
#include <string.h>
#include "config.h"
#include "complex.h"
#include "formulas.h"
#include "zoom.h"

int coloringmode;
int incoloringmode;

char *incolorname[] =
{
    "maxiter",
    "zmag",
};

char *outcolorname[] =
{
    "iter",
    "iter+real",
    "iter+imag",
    "iter+real/imag",
    "iter+real+imag+real/imag",
    "binary decomposition",
    "biomorphs",
    "biomorphs decompisition"
};

#define OUTPUT() if(iter>=MAXITER)\
		return(INT_MAX); else \
		return(iter)

static int mand_calc(register number_t cre, register number_t cim, register number_t pre, register number_t pim)
{
    register number_t rp = 0, ip = 0;
    register unsigned int iter = MAXITER /*& (~(int) 3) */ ;
    register number_t zre, zim;
    register number_t r = 0, s = 0;
#ifdef _UNDEFINED_
    int whensavenew, whenincsave;
#endif
    zre = cre;
    zim = cim;
    if (!incoloringmode && pre == cre && pim == cim) {
	r = cre * cre + cim * cim;
	s = sqrt(r - cre / 2 + 1.0 / 16.0);
    }
    if (!incoloringmode && pre == cre && pim == cim && ((cre + 1) * (cre + 1) + cim * cim < 1.0 / 16.0 || ((16 * r * s) < (5 * s - 4 * cre + 1))))
	iter = 0;
    else {
	r = zre;
	s = zim;
#ifdef _UNDEFINED_
	whensavenew = 4;	/*You should adapt theese values */
	whenincsave = 10;
#endif

	while ((iter) && (rp + ip < 4)) {
	    ip = (zim * zim);
	    zim = (zim * zre) * 2 + pim;
	    rp = (zre * zre);
	    zre = rp - ip + pre;
#ifdef _UNDEFINED_
	    if ((iter % whensavenew) == 0) {
		r = zre;
		s = zim;
		whenincsave--;
		if (!whenincsave) {
		    whensavenew <<= 1;
		    whenincsave = 10;
		}
	    }
	    else {
		if ((myabs(r - zre) < lim) && (myabs(s - zim) < lim)) {
		    return (iter);
		}
	    }
#endif
	    iter--;

	}
    }
    iter = MAXITER - iter;
    OUTPUT();
}

static int mand3_calc(register number_t cre, register number_t cim, register number_t pre, register number_t pim)
{
    register number_t rp = 0, ip = 0;
    register int iter = MAXITER;
    register number_t zre, zim;

    zre = cre;
    zim = cim;
    rp = zre * zre;
    ip = zim * zim;
    while ((iter) && (rp + ip < 4)) {
	rp = zre * (rp - 3 * ip);
	zim = zim * (3 * zre * zre - ip) + pim;
	zre = rp + pre;
	rp = zre * zre;
	ip = zim * zim;
	iter--;
    }
    iter = MAXITER - iter;
    OUTPUT();
}

symetry sym6[] =
{
    {0, 1.73205080758},
    {0, -1.73205080758}
};

symetry sym8[] =
{
    {0, 1},
    {0, -1}
};

symetry sym16[] =
{
    {0, 1},
    {0, -1},
    {0, 0.414214},
    {0, -0.414214},
    {0, 2.414214},
    {0, -2.414214}
};

struct formula formulas[] =
{
    {
	FORMULAMAGIC,
	mand_calc,
	{"Mandelbrot", "Julia"},
	{0.5, -2.0, 1.25, -1.25},
	1, 0.0, 0.0,
	{
	    {INT_MAX, 0, 0, NULL},
	    {INT_MAX, 0, 0, NULL},
	    {INT_MAX, INT_MAX, 0, NULL},
	    {INT_MAX, INT_MAX, 0, NULL},
	    {INT_MAX, INT_MAX, 0, NULL},
	    {INT_MAX, INT_MAX, 0, NULL},
	    {INT_MAX, 0, 0, NULL}
	},
	{
	    {INT_MAX, 0, 0, NULL},
	    {INT_MAX, 0, 0, NULL}
	},
	0, 0,
    },
    {
	FORMULAMAGIC,
	mand3_calc,
	{"Mandelbrot^3", "Julia^3"},
	{1.25, -1.25, 1.25, -1.25},
	1, 0.0, 0.0,
	{
	    {0, 0, 0, NULL},
	    {INT_MAX, 0, 0, NULL},
	    {0, INT_MAX, 0, NULL},
	    {INT_MAX, INT_MAX, 0, NULL},
	    {INT_MAX, INT_MAX, 0, NULL},
	    {0, INT_MAX, 0, NULL},
	    {0, 0, 0, NULL}
	},
	{
	    {0, 0, 0, NULL},
	    {0, 0, 0, NULL}
	},
	1, 0,
    },
#if 0
    {
	FORMULAMAGIC,
	/*magnet_calc */ NULL,
	{"Magnet", "Magnet"},
	{3, 0, 2.2, -2.2},
	1, 0.0, 0.0,
	{
	    {INT_MAX, 0, 0, NULL},
	    {INT_MAX, 0, 0, NULL},
	    {INT_MAX, INT_MAX, 0, NULL},
	    {INT_MAX, INT_MAX, 0, NULL},
	    {INT_MAX, INT_MAX, 0, NULL},
	    {INT_MAX, INT_MAX, 0, NULL},
	    {INT_MAX, 0, 0, NULL}
	},
	{
	    {INT_MAX, 0, 0, NULL},
	    {INT_MAX, 0, 0, NULL}
	},
	6, 1,
    },
#endif
};

struct formula *currentformula = formulas;
CONST int nformulas = sizeof(formulas) / sizeof(struct formula);
