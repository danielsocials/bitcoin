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
 
#ifdef HAVE_MYSQL

#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include <mysql/mysql.h>

#include "util.h"

static bool my_open(void)
{
	MYSQL *db;
	my_bool reconnect = 1;

	if (mysql_library_init(0, NULL, NULL))
		goto err_out;

	db = mysql_init(NULL);
	if (!db)
		goto err_out_lib;

	mysql_ssl_set(db, NULL, NULL, NULL, NULL, NULL);
	mysql_options(db, MYSQL_OPT_RECONNECT, &reconnect);
	mysql_options(db, MYSQL_OPT_COMPRESS, NULL);

	if (!mysql_real_connect(db, dbsrv.db_host, dbsrv.db_username,
				dbsrv.db_password, dbsrv.db_name,
				dbsrv.db_port > 0 ? dbsrv.db_port : 0,
				NULL, 0))
		goto err_out_db;

	dbsrv.db_cxn = db;

	return true;

err_out_db:
	mysql_close(db);
err_out_lib:
	mysql_library_end();
err_out:
	return false;
}

static void my_close(void)
{
	MYSQL *db = dbsrv.db_cxn;

	mysql_close(db);
	mysql_library_end();
}

struct server_db_ops mysql_db_ops = {
	.open		= my_open,
	.close		= my_close,
};

#endif /* HAVE_MYSQL */

#ifdef __cplusplus
}
#endif

