/*
 * BB: The portable demo
 *
 * (C) 1997 by AA-group (e-mail: aa@horac.ta.jcu.cz)
 *
 * 3rd August 1997
 * version: 1.2 [final3]
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public Licences as by published
 * by the Free Software Foundation; either version 2; or (at your option)
 * any later version
 *
 * This program is distributed in the hope that it will entertaining,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILTY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Publis License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef IMAGE_H
#define IMAGE_H
struct image {
  unsigned char *data;
  int size;
  int width,height;
  unsigned char *decompressed;
};
/* Upstream carried four portrait sets, one per AA-group member. KK carries
 * one: the four crystallise stages of the KDOS author's head, baked from the
 * FACE table by tools/genface.c. */
extern struct image kd1,kd2,kd3,kd4;
#endif
