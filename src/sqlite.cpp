/*
 * Copyright 2015 daniel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
 
#ifdef __cplusplus
extern "C" {
#endif
 
#include "dblayer.h"

#ifdef HAVE_SQLITE3

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <sqlite3.h>
#include "util.h"

static bool sql_open(void)
{
	sqlite3 *db;
	int sqlrc = sqlite3_open_v2(dbsrv.db_name, &db,
				SQLITE_OPEN_READWRITE, NULL);
	if (sqlrc != SQLITE_OK) {
		printf("sqlite3_open(%s) failed: %d",
		       dbsrv.db_name, sqlrc);
		return false;
	}

	dbsrv.db_cxn = db;
	return true;
}

static void sql_close(void)
{
	sqlite3 *db = dbsrv.db_cxn;
	if (sqlite3_close(db) != SQLITE_OK)
		printf("db close failed");
}

struct server_db_ops sqlite_db_ops = {
	.open		= sql_open,
	.close		= sql_close,
};

#endif /* HAVE_SQLITE3 */

#ifdef __cplusplus
}
#endif
 
