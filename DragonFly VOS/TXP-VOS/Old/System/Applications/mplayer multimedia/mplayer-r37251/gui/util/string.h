/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_GUI_STRING_H
#define MPLAYER_GUI_STRING_H

#include <stddef.h>

/**
 * @brief Wraps #cutString():
 *        Extract a part of a string delimited by a separator character
 *        at most the size of @a out.
 */
#define cutStr(in, out, sep, num) cutString(in, out, sep, num, sizeof(out))

int cutInt(char *in, char sep, int num);
void cutString(char *in, char *out, char sep, int num, size_t maxout);
char *decomment(char *in);
char *gstrchr(const char *str, int c);
int gstrcmp(const char *a, const char *b);
char *gstrdup(const char *str);
int gstrncmp(const char *a, const char *b, size_t n);
void setddup(char **old, const char *dir, const char *name);
void setdup(char **old, const char *str);
char *strlower(char *in);
char *strswap(char *in, char from, char to);
char *strupper(char *in);
char *trim(char *in);

#endif /* MPLAYER_GUI_STRING_H */
