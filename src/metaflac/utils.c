/* metaflac - Command-line FLAC metadata editor
 * Copyright (C) 2001,2002  Josh Coalson
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "utils.h"
#include "FLAC/assert.h"
#include "share/utf8.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void die(const char *message)
{
	FLAC__ASSERT(0 != message);
	fprintf(stderr, "ERROR: %s\n", message);
	exit(1);
}

char *local_strdup(const char *source)
{
	char *ret;
	FLAC__ASSERT(0 != source);
	if(0 == (ret = strdup(source)))
		die("out of memory during strdup()");
	return ret;
}

void local_strcat(char **dest, const char *source)
{
	unsigned ndest, nsource;

	FLAC__ASSERT(0 != dest);
	FLAC__ASSERT(0 != source);

	ndest = *dest? strlen(*dest) : 0;
	nsource = strlen(source);

	if(nsource == 0)
		return;

	*dest = realloc(*dest, ndest + nsource + 1);
	if(0 == *dest)
		die("out of memory growing string");
	strcpy((*dest)+ndest, source);
}

void hexdump(const char *filename, const FLAC__byte *buf, unsigned bytes, const char *indent)
{
	unsigned i, left = bytes;
	const FLAC__byte *b = buf;

	for(i = 0; i < bytes; i += 16) {
		printf("%s%s%s%08X: "
			"%02X %02X %02X %02X %02X %02X %02X %02X "
			"%02X %02X %02X %02X %02X %02X %02X %02X "
			"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
			filename? filename:"", filename? ":":"",
			indent, i,
			left >  0? (unsigned char)b[ 0] : 0,
			left >  1? (unsigned char)b[ 1] : 0,
			left >  2? (unsigned char)b[ 2] : 0,
			left >  3? (unsigned char)b[ 3] : 0,
			left >  4? (unsigned char)b[ 4] : 0,
			left >  5? (unsigned char)b[ 5] : 0,
			left >  6? (unsigned char)b[ 6] : 0,
			left >  7? (unsigned char)b[ 7] : 0,
			left >  8? (unsigned char)b[ 8] : 0,
			left >  9? (unsigned char)b[ 9] : 0,
			left > 10? (unsigned char)b[10] : 0,
			left > 11? (unsigned char)b[11] : 0,
			left > 12? (unsigned char)b[12] : 0,
			left > 13? (unsigned char)b[13] : 0,
			left > 14? (unsigned char)b[14] : 0,
			left > 15? (unsigned char)b[15] : 0,
			(left >  0) ? (isprint(b[ 0]) ? b[ 0] : '.') : ' ',
			(left >  1) ? (isprint(b[ 1]) ? b[ 1] : '.') : ' ',
			(left >  2) ? (isprint(b[ 2]) ? b[ 2] : '.') : ' ',
			(left >  3) ? (isprint(b[ 3]) ? b[ 3] : '.') : ' ',
			(left >  4) ? (isprint(b[ 4]) ? b[ 4] : '.') : ' ',
			(left >  5) ? (isprint(b[ 5]) ? b[ 5] : '.') : ' ',
			(left >  6) ? (isprint(b[ 6]) ? b[ 6] : '.') : ' ',
			(left >  7) ? (isprint(b[ 7]) ? b[ 7] : '.') : ' ',
			(left >  8) ? (isprint(b[ 8]) ? b[ 8] : '.') : ' ',
			(left >  9) ? (isprint(b[ 9]) ? b[ 9] : '.') : ' ',
			(left > 10) ? (isprint(b[10]) ? b[10] : '.') : ' ',
			(left > 11) ? (isprint(b[11]) ? b[11] : '.') : ' ',
			(left > 12) ? (isprint(b[12]) ? b[12] : '.') : ' ',
			(left > 13) ? (isprint(b[13]) ? b[13] : '.') : ' ',
			(left > 14) ? (isprint(b[14]) ? b[14] : '.') : ' ',
			(left > 15) ? (isprint(b[15]) ? b[15] : '.') : ' '
		);
		left -= 16;
		b += 16;
   }
}

FLAC__bool parse_vorbis_comment_field(const char *field_ref, char **field, char **name, char **value, unsigned *length, const char **violation)
{
	static const char * const violations[] = {
		"field name contains invalid character",
		"field contains no '=' character"
	};

	char *p, *q, *s;

	if(0 != field)
		*field = local_strdup(field_ref);

	s = local_strdup(field_ref);

	if(0 == (p = strchr(s, '='))) {
		free(s);
		*violation = violations[1];
		return false;
	}
	*p++ = '\0';

	for(q = s; *q; q++) {
		if(*q < 0x20 || *q > 0x7d || *q == 0x3d) {
			free(s);
			*violation = violations[0];
			return false;
		}
	}

	*name = local_strdup(s);
	*value = local_strdup(p);
	*length = strlen(p);

	free(s);
	return true;
}

void write_vc_field(const char *filename, const FLAC__StreamMetadata_VorbisComment_Entry *entry, FLAC__bool raw, FILE *f)
{
	if(0 != entry->entry) {
		if(filename)
			fprintf(f, "%s:", filename);

		if(!raw) {
			/*
			 * utf8_decode() works on NULL-terminated strings, so
			 * we append a null to the entry.  @@@ Note, this means
			 * that comments that contain an embedded null will be
			 * truncated by utf_decode().
			 */
			char *terminated, *converted;

			if(0 == (terminated = malloc(entry->length + 1)))
				die("out of memory allocating space for vorbis comment");
			memcpy(terminated, entry->entry, entry->length);
			terminated[entry->length] = '\0';
			if(utf8_decode(terminated, &converted) >= 0) {
				(void) fwrite(converted, 1, strlen(converted), f);
				free(terminated);
				free(converted);
			}
			else {
				free(terminated);
				(void) fwrite(entry->entry, 1, entry->length, f);
			}
		}
		else {
			(void) fwrite(entry->entry, 1, entry->length, f);
		}
	}

	fprintf(f, "\n");
}

void write_vc_fields(const char *filename, const char *field_name, const FLAC__StreamMetadata_VorbisComment_Entry entry[], unsigned num_entries, FLAC__bool raw, FILE *f)
{
	unsigned i;
	const unsigned field_name_length = (0 != field_name)? strlen(field_name) : 0;

	for(i = 0; i < num_entries; i++) {
		if(0 == field_name || FLAC__metadata_object_vorbiscomment_entry_matches(entry + i, field_name, field_name_length))
			write_vc_field(filename, entry + i, raw, f);
	}
}