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

#include "options.h"
#include "utils.h"
#include "FLAC/assert.h"
#include "share/utf8.h"
#include <stdlib.h>
#include <string.h>

static FLAC__bool remove_vc_all(const char *filename, FLAC__StreamMetadata *block, FLAC__bool *needs_write);
static FLAC__bool remove_vc_field(const char *filename, FLAC__StreamMetadata *block, const char *field_name, FLAC__bool *needs_write);
static FLAC__bool remove_vc_firstfield(const char *filename, FLAC__StreamMetadata *block, const char *field_name, FLAC__bool *needs_write);
static FLAC__bool set_vc_field(const char *filename, FLAC__StreamMetadata *block, const Argument_VcField *field, FLAC__bool *needs_write, FLAC__bool raw);
static FLAC__bool import_vc_from(const char *filename, FLAC__StreamMetadata *block, const Argument_Filename *vc_filename, FLAC__bool *needs_write, FLAC__bool raw);
static FLAC__bool export_vc_to(const char *filename, FLAC__StreamMetadata *block, const Argument_Filename *vc_filename, FLAC__bool raw);

FLAC__bool do_shorthand_operation__vorbis_comment(const char *filename, FLAC__bool prefix_with_filename, FLAC__Metadata_Chain *chain, const Operation *operation, FLAC__bool *needs_write, FLAC__bool raw)
{
	FLAC__bool ok = true, found_vc_block = false;
	FLAC__StreamMetadata *block = 0;
	FLAC__Metadata_Iterator *iterator = FLAC__metadata_iterator_new();

	if(0 == iterator)
		die("out of memory allocating iterator");

	FLAC__metadata_iterator_init(iterator, chain);

	do {
		block = FLAC__metadata_iterator_get_block(iterator);
		if(block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
			found_vc_block = true;
	} while(!found_vc_block && FLAC__metadata_iterator_next(iterator));

	if(!found_vc_block) {
		/* create a new block if necessary */
		if(operation->type == OP__SET_VC_FIELD || operation->type == OP__IMPORT_VC_FROM) {
			block = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
			if(0 == block)
				die("out of memory allocating VORBIS_COMMENT block");
			while(FLAC__metadata_iterator_next(iterator))
				;
			if(!FLAC__metadata_iterator_insert_block_after(iterator, block)) {
				fprintf(stderr, "%s: ERROR: adding new VORBIS_COMMENT block to metadata, status =\"%s\"\n", filename, FLAC__Metadata_ChainStatusString[FLAC__metadata_chain_status(chain)]);
				return false;
			}
			/* iterator is left pointing to new block */
			FLAC__ASSERT(FLAC__metadata_iterator_get_block(iterator) == block);
		}
		else {
			FLAC__metadata_iterator_delete(iterator);
			return ok;
		}
	}

	FLAC__ASSERT(0 != block);
	FLAC__ASSERT(block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT);

	switch(operation->type) {
		case OP__SHOW_VC_VENDOR:
			write_vc_field(prefix_with_filename? filename : 0, &block->data.vorbis_comment.vendor_string, raw, stdout);
			break;
		case OP__SHOW_VC_FIELD:
			write_vc_fields(prefix_with_filename? filename : 0, operation->argument.vc_field_name.value, block->data.vorbis_comment.comments, block->data.vorbis_comment.num_comments, raw, stdout);
			break;
		case OP__REMOVE_VC_ALL:
			ok = remove_vc_all(filename, block, needs_write);
			break;
		case OP__REMOVE_VC_FIELD:
			ok = remove_vc_field(filename, block, operation->argument.vc_field_name.value, needs_write);
			break;
		case OP__REMOVE_VC_FIRSTFIELD:
			ok = remove_vc_firstfield(filename, block, operation->argument.vc_field_name.value, needs_write);
			break;
		case OP__SET_VC_FIELD:
			ok = set_vc_field(filename, block, &operation->argument.vc_field, needs_write, raw);
			break;
		case OP__IMPORT_VC_FROM:
			ok = import_vc_from(filename, block, &operation->argument.filename, needs_write, raw);
			break;
		case OP__EXPORT_VC_TO:
			ok = export_vc_to(filename, block, &operation->argument.filename, raw);
			break;
		default:
			ok = false;
			FLAC__ASSERT(0);
			break;
	};

	FLAC__metadata_iterator_delete(iterator);
	return ok;
}

/*
 * local routines
 */

FLAC__bool remove_vc_all(const char *filename, FLAC__StreamMetadata *block, FLAC__bool *needs_write)
{
	FLAC__ASSERT(0 != block);
	FLAC__ASSERT(block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT);
	FLAC__ASSERT(0 != needs_write);

	if(0 != block->data.vorbis_comment.comments) {
		FLAC__ASSERT(block->data.vorbis_comment.num_comments > 0);
		if(!FLAC__metadata_object_vorbiscomment_resize_comments(block, 0)) {
			fprintf(stderr, "%s: ERROR: memory allocation failure\n", filename);
			return false;
		}
		*needs_write = true;
	}
	else {
		FLAC__ASSERT(block->data.vorbis_comment.num_comments == 0);
	}

	return true;
}

FLAC__bool remove_vc_field(const char *filename, FLAC__StreamMetadata *block, const char *field_name, FLAC__bool *needs_write)
{
	int n;

	FLAC__ASSERT(0 != needs_write);

	n = FLAC__metadata_object_vorbiscomment_remove_entries_matching(block, field_name);

	if(n < 0) {
		fprintf(stderr, "%s: ERROR: memory allocation failure\n", filename);
		return false;
	}
	else if(n > 0)
		*needs_write = true;

	return true;
}

FLAC__bool remove_vc_firstfield(const char *filename, FLAC__StreamMetadata *block, const char *field_name, FLAC__bool *needs_write)
{
	int n;

	FLAC__ASSERT(0 != needs_write);

	n = FLAC__metadata_object_vorbiscomment_remove_entry_matching(block, field_name);

	if(n < 0) {
		fprintf(stderr, "%s: ERROR: memory allocation failure\n", filename);
		return false;
	}
	else if(n > 0)
		*needs_write = true;

	return true;
}

FLAC__bool set_vc_field(const char *filename, FLAC__StreamMetadata *block, const Argument_VcField *field, FLAC__bool *needs_write, FLAC__bool raw)
{
	FLAC__StreamMetadata_VorbisComment_Entry entry;
	char *converted;
	FLAC__bool needs_free = false;

	FLAC__ASSERT(0 != block);
	FLAC__ASSERT(block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT);
	FLAC__ASSERT(0 != field);
	FLAC__ASSERT(0 != needs_write);

	if(raw) {
		entry.entry = field->field;
	}
	else if(utf8_encode(field->field, &converted) >= 0) {
		entry.entry = converted;
		needs_free = true;
	}
	else {
		fprintf(stderr, "%s: ERROR: couldn't convert comment to UTF-8\n", filename);
		return false;
	}

	entry.length = strlen(entry.entry);

	if(!FLAC__metadata_object_vorbiscomment_insert_comment(block, block->data.vorbis_comment.num_comments, entry, /*copy=*/true)) {
		if(needs_free)
			free(converted);
		fprintf(stderr, "%s: ERROR: memory allocation failure\n", filename);
		return false;
	}
	else {
		*needs_write = true;
		if(needs_free)
			free(converted);
		return true;
	}
}

FLAC__bool import_vc_from(const char *filename, FLAC__StreamMetadata *block, const Argument_Filename *vc_filename, FLAC__bool *needs_write, FLAC__bool raw)
{
	FILE *f;
	char line[65536];
	FLAC__bool ret;

	if(0 == vc_filename->value || strlen(vc_filename->value) == 0) {
		fprintf(stderr, "%s: ERROR: empty import file name\n", filename);
		return false;
	}
	if(0 == strcmp(vc_filename->value, "-"))
		f = stdin;
	else
		f = fopen(vc_filename->value, "r");

	if(0 == f) {
		fprintf(stderr, "%s: ERROR: can't open import file %s\n", filename, vc_filename->value);
		return false;
	}

	ret = true;
	while(ret && !feof(f)) {
		fgets(line, sizeof(line), f);
		if(!feof(f)) {
			char *p = strchr(line, '\n');
			if(0 == p) {
				fprintf(stderr, "%s: ERROR: line too long, aborting\n", vc_filename->value);
				ret = false;
			}
			else {
				const char *violation;
				Argument_VcField field;
				*p = '\0';
				memset(&field, 0, sizeof(Argument_VcField));
				if(!parse_vorbis_comment_field(line, &field.field, &field.field_name, &field.field_value, &field.field_value_length, &violation)) {
					FLAC__ASSERT(0 != violation);
					fprintf(stderr, "%s: ERROR: malformed vorbis comment field \"%s\",\n       %s\n", vc_filename->value, line, violation);
					ret = false;
				}
				else {
					ret = set_vc_field(filename, block, &field, needs_write, raw);
				}
				if(0 != field.field)
					free(field.field);
				if(0 != field.field_name)
					free(field.field_name);
				if(0 != field.field_value)
					free(field.field_value);
			}
		}
	};

	if(f != stdin)
		fclose(f);
	return ret;
}

FLAC__bool export_vc_to(const char *filename, FLAC__StreamMetadata *block, const Argument_Filename *vc_filename, FLAC__bool raw)
{
	FILE *f;
	FLAC__bool ret;

	if(0 == vc_filename->value || strlen(vc_filename->value) == 0) {
		fprintf(stderr, "%s: ERROR: empty export file name\n", filename);
		return false;
	}
	if(0 == strcmp(vc_filename->value, "-"))
		f = stdout;
	else
		f = fopen(vc_filename->value, "w");

	if(0 == f) {
		fprintf(stderr, "%s: ERROR: can't open export file %s\n", filename, vc_filename->value);
		return false;
	}

	ret = true;

	write_vc_fields(0, 0, block->data.vorbis_comment.comments, block->data.vorbis_comment.num_comments, raw, f);

	if(f != stdout)
		fclose(f);
	return ret;
}