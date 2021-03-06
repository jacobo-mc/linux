/*
 * descriptions_store.c: store man page descriptions in database
 *
 * Copyright (C) 2002, 2003, 2006, 2007, 2008, 2011 Colin Watson.
 *
 * This file is part of man-db.
 *
 * man-db is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * man-db is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with man-db; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "gettext.h"
#define _(String) gettext (String)

#include "error.h"
#include "gl_list.h"
#include "stat-time.h"
#include "xalloc.h"

#include "manconfig.h"

#include "debug.h"
#include "glcontainers.h"

#include "db_storage.h"

#include "filenames.h"
#include "ult_src.h"
#include "descriptions.h"

static void gripe_bad_store (const char *name, const char *ext)
{
	if (quiet < 2)
		error (0, 0, _("warning: failed to store entry for %s(%s)"),
		       name, ext);
}

/* Is PATH a prefix of DIR, such that DIR is in the manual hierarchy PATH?
 * This requires that the part of DIR following PATH start with "/man".
 */
static int is_prefix (const char *path, const char *dir)
{
	return (STRNEQ (dir, path, strlen (path)) &&
		STRNEQ (dir + strlen (path), "/man", 4));
}

/* Take a list of descriptions returned by parse_descriptions() and store
 * it into the database.
 */
void store_descriptions (MYDBM_FILE dbf, gl_list_t descs, struct mandata *info,
			 const char *path, const char *base, gl_list_t trace)
{
	const struct page_description *desc;
	char save_id = info->id;
	struct timespec save_mtime = info->mtime;
	const char *trace_name;

	if (gl_list_size (descs) && trace) {
		GL_LIST_FOREACH (trace, trace_name)
			debug ("trace: '%s'\n", trace_name);
	}

	GL_LIST_FOREACH (descs, desc) {
		/* Either it's the real thing or merely a reference. Get the
		 * id and pointer right in either case.
		 */
		bool found_real_page = false;
		bool found_external = false;

		if (STREQ (base, desc->name)) {
			info->id = save_id;
			info->pointer = NULL;
			info->whatis = desc->whatis;
			info->mtime = save_mtime;
			found_real_page = true;
		} else if (trace) {
			GL_LIST_FOREACH (trace, trace_name) {
				struct mandata trace_info;
				char *buf;

				buf = filename_info (trace_name,
						     &trace_info, "");
				if (trace_info.name &&
				    STREQ (trace_info.name, desc->name)) {
					struct stat st;

					if (path && !is_prefix (path, buf)) {
						/* Link outside this manual
						 * hierarchy; skip this
						 * description.
						 */
						found_external = true;
						free (trace_info.name);
						free (buf);
						break;
					}
					if (!gl_list_next_node (trace,
								trace_node) &&
					    save_id == SO_MAN)
						info->id = ULT_MAN;
					else
						info->id = save_id;
					info->pointer = NULL;
					info->whatis = desc->whatis;
					if (lstat (trace_name, &st) == 0)
						info->mtime = get_stat_mtime
							(&st);
					else
						info->mtime = save_mtime;
					found_real_page = true;
				}

				free (trace_info.name);
				free (buf);
			}
		}

		if (found_external) {
			debug ("skipping '%s'; link outside manual "
			       "hierarchy\n", desc->name);
			continue;
		}

		if (!found_real_page) {
			if (save_id < STRAY_CAT)
				info->id = WHATIS_MAN;
			else
				info->id = WHATIS_CAT;
			info->pointer = xstrdup (base);
			/* Don't waste space storing the whatis in the db
			 * more than once.
			 */
			info->whatis = NULL;
			info->mtime = save_mtime;
		}

		debug ("name = '%s', id = %c\n", desc->name, info->id);
		if (dbstore (dbf, info, desc->name) > 0) {
			gripe_bad_store (base, info->ext);
			break;
		}
	}
}
