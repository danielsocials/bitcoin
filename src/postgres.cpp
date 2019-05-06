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

#include "dblayer.h"

#ifdef HAVE_POSTGRESQL

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <syslog.h>
#ifdef MAC_OSX
#include <libpq-fe.h>
#else
#include <postgresql/libpq-fe.h>
#endif
#include "logging.h"
#include <db.h>
#include "util/system.h"

#define PGOK(_res)                                                             \
  ((_res) == PGRES_COMMAND_OK || (_res) == PGRES_TUPLES_OK ||                  \
   (_res) == PGRES_EMPTY_QUERY)

#define WHERE_FFL " - from %s %s() line %d"
#define WHERE_FFL_PASS __FILE__, __func__, __LINE__

#define PQ_READ true
#define PQ_WRITE false

// size to allocate for pgsql text and display (bigger than needed)
#define DATE_BUFSIZ (63 + 1)
#define CDATE_BUFSIZ (127 + 1)
#define BIGINT_BUFSIZ (63 + 1)
#define INT_BUFSIZ (63 + 1)
#define DOUBLE_BUFSIZ (63 + 1)

#define TXT_BIG 256
#define TXT_MED 128
#define TXT_SML 64
#define TXT_FLAG 1

enum data_type {
  TYPE_STR,
  TYPE_BIGINT,
  TYPE_INT,
  TYPE_BYTEA,
  TYPE_TV,
  TYPE_TVS,
  TYPE_CTV,
  TYPE_BLOB,
  TYPE_BOOL,
  TYPE_DOUBLE,
  TYPE_HASH,
  TYPE_SCRIPT,
  TYPE_ADDR
};

typedef struct timeval tv_t;

#define DEFAULT_SELECT_BLK "select id from blk where hash=$1::bytea"
#define DEFAULT_SELECT_TX "select id from tx where hash=$1::bytea"
#define DEFAULT_SELECT_FILTER_TX "select id from filter_tx where hash=$1::bytea"
#define DEFAULT_SELECT_ADDR_OUT "select * from addr_txout where addr_id=$1::bigint and txout_id=$2::bigint"

#define DEFAULT_SAVE_BLK                                                       \
  "insert into blk (hash, height, version, prev_hash, mrkl_root, time, bits, nonce, blk_size, work, tx_count, pool_id, recv_time, pool_bip,ip) \
    values($1::bytea,$2,$3,$4::bytea,$5::bytea,$6::bigint,$7,$8,$9,$10::bytea, $11, $12, $13, $14, $15) RETURNING id"

#define DEFAULT_SAVE_BLK_TX                                                    \
  "insert into blk_tx (blk_id, tx_id, idx) \
    values($1,$2,$3)"

#define DEFAULT_READD_BLK                                                       \
  " select readd_blk ($1::bytea) "

#define DEFAULT_SAVE_TX                                                        \
  "insert into tx (hash, version, lock_time, coinbase, tx_size, recv_time, ip, wtxid, wsize, vsize, metatype) \
    values($1::bytea,$2,$3,$4::boolean,$5,$6,$7, $8::bytea, $9, $10, $11)  RETURNING id"

#define DEFAULT_SAVE_MEMPOOL                                                   \
  "insert into mempool (tx_id , hash,  entryPriority , nFee , inChainInputValue  , nTxSize  , nTime  , entryHeight  , hadNoDependencies  , sigOpCount  , modifiedFee  , nModSize  , nUsageSize  , dirty  , nCountWithDescendants , nSizeWithDescendants  , nModFeesWithDescendants  , spendsCoinbase ) \
    values($1,$2::bytea,$3,$4,$5,$6,$7, $8,$9::boolean,$10,$11, $12,$13,$14::boolean,$15, $16,$17, $18::boolean)"

#define DEFAULT_READD_TX "select readd_tx($1);"

#define DEFAULT_SAVE_UTX "INSERT INTO utx (id) values ($1);"
#define DEFAULT_SAVE_MEMPOOL_TX "INSERT INTO mtx (hash) values ($1::bytea);"

#define DEFAULT_SAVE_TXIN                                                      \
  "insert into txin (tx_id, tx_idx, prev_out_index, sequence, script_sig, prev_out, witness) \
      values($1,$2,$3,$4,$5::bytea,$6::bytea, $7::bytea)  RETURNING id"

#define DEFAULT_SAVE_TXOUT                                                     \
  "insert into txout (tx_id, tx_idx, pk_script, value, type) \
    values($1,$2,$3::bytea,$4,$5) RETURNING id"

#define DEFAULT_SAVE_ADDR "select insert_addr($1,$2);"

#define DEFAULT_SAVE_ADDR_OUT                                                  \
  "insert into addr_txout (addr_id, txout_id) \
    values($1,$2)"

#define DEFAULT_ADD_TX_STATICS "select add_tx_statics($1,$2,$3,$4,$5);"

#define DEFAULT_ADD_BLK_STATICS "select add_blk_statics($1);"

#define DEFAULT_DELETE_BLK "select delete_blk($1::bytea);"

#define DEFAULT_DELETE_SOME_UTX "select delete_some_utx();"

#define DEFAULT_DELETE_TX "select delete_tx($1);"

const char hextbl[] = "0123456789abcdef";

extern ArgsManager gArgs;

extern struct DBSERVER dbSrv;

static unsigned char *bintostr(const unsigned char *from, size_t from_length,
                               size_t *to_length, bool revert) {
  const unsigned char *vp;
  unsigned char *rp;
  unsigned char *result;
  size_t i;
  size_t len;
  unsigned char c;

  /*
   * empty string has 1 char ('\0')
   */
  len = 1;

  len += 2 * from_length;

  *to_length = len;
  rp = result = (unsigned char *)malloc(len);
  if (rp == NULL) {
    return NULL;
  }

  if ((from_length > 0) && (from != NULL)) {
    if (revert) {
      vp = from + from_length - 1;
      for (i = from_length; i > 0; i--, vp--) {
        c = *vp;

        *rp++ = hextbl[(c >> 4) & 0xF];
        *rp++ = hextbl[c & 0xF];
      }
    } else {
      vp = from;
      for (i = from_length; i > 0; i--, vp++) {
        c = *vp;

        *rp++ = hextbl[(c >> 4) & 0xF];
        *rp++ = hextbl[c & 0xF];
      }
    }
  }
  *rp = '\0';

  return result;
}

static unsigned char *bintohex(const unsigned char *from, size_t from_length,
                               size_t *to_length, bool revert) {
  const unsigned char *vp;
  unsigned char *rp;
  unsigned char *result;
  size_t i;
  size_t len;
  size_t bslash_len = 1;
  unsigned char c;

  /*
   * empty string has 1 char ('\0')
   */
  len = 1;

  len += bslash_len + 1 + 2 * from_length;

  *to_length = len;
  rp = result = (unsigned char *)malloc(len);
  if (rp == NULL) {
    return NULL;
  }

  *rp++ = '\\';
  *rp++ = 'x';

  if ((from_length > 0) && (from != NULL)) {
    if (revert) {
      vp = from + from_length - 1;
      for (i = from_length; i > 0; i--, vp--) {
        c = *vp;

        *rp++ = hextbl[(c >> 4) & 0xF];
        *rp++ = hextbl[c & 0xF];
      }
    } else {
      vp = from;
      for (i = from_length; i > 0; i--, vp++) {
        c = *vp;

        *rp++ = hextbl[(c >> 4) & 0xF];
        *rp++ = hextbl[c & 0xF];
      }
    }
  }
  *rp = '\0';

  return result;
}

char *data_to_buf(enum data_type typ, void *data, char *buf, size_t siz) {
  struct tm tm;
  size_t to_len;

  if (!buf) {
    switch (typ) {
    case TYPE_STR:
    case TYPE_BLOB:
      siz = strlen((char *)data) + 1;
      break;
    case TYPE_BIGINT:
      siz = BIGINT_BUFSIZ;
      break;
    case TYPE_INT:
      siz = INT_BUFSIZ;
      break;
    case TYPE_BYTEA:
      if (siz == 0)
        siz = 32;
      break;
    case TYPE_HASH:
      siz = 32;
      break;
    case TYPE_SCRIPT:
      break;
    case TYPE_ADDR:
      break;
    case TYPE_TV:
    case TYPE_TVS:
      siz = DATE_BUFSIZ;
      break;
    case TYPE_CTV:
      siz = CDATE_BUFSIZ;
      break;
    case TYPE_DOUBLE:
      siz = DOUBLE_BUFSIZ;
      break;
    case TYPE_BOOL:
      siz = 5;
      break;
    default:
      LogPrint(BCLog::DBLAYER, "Unknown field (%d) to convert" WHERE_FFL, (int)typ,
               WHERE_FFL_PASS);
      break;
    }

    if ((typ != TYPE_SCRIPT) && (typ != TYPE_HASH) && (typ != TYPE_ADDR) &&
        (typ != TYPE_BYTEA)) {
      buf = (char *)malloc(siz);
      if (!buf)
        LogPrint(BCLog::DBLAYER, "(%d) OOM" WHERE_FFL, (int)siz, WHERE_FFL_PASS);
    }
  }

  switch (typ) {
  case TYPE_STR:
  case TYPE_BLOB:
    snprintf(buf, siz, "%s", (char *)data);
    break;
  case TYPE_BIGINT:
    snprintf(buf, siz, "%" PRIu64, *((uint64_t *)data));
    break;
  case TYPE_INT:
    snprintf(buf, siz, "%" PRIu32, *((uint32_t *)data));
    break;
  case TYPE_BYTEA:
    buf = (char *)bintohex((const unsigned char *)data, siz, &to_len, true);
    break;
  case TYPE_HASH:
    buf = (char *)bintohex((const unsigned char *)data, siz, &to_len, true);
    break;
  case TYPE_SCRIPT:
    buf = (char *)bintohex((const unsigned char *)data, siz, &to_len, false);
    break;
  case TYPE_ADDR:
    buf = (char *)bintostr((const unsigned char *)data, siz, &to_len, false);
    break;
  case TYPE_TV:
    gmtime_r(&(((tv_t *)data)->tv_sec), &tm);
    snprintf(buf, siz, "%d-%02d-%02d %02d:%02d:%02d.%06ld+00",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
             tm.tm_min, tm.tm_sec, (((tv_t *)data)->tv_usec));
    break;
  case TYPE_CTV:
    snprintf(buf, siz, "%ld,%ld", (((tv_t *)data)->tv_sec),
             (((tv_t *)data)->tv_usec));
    break;
  case TYPE_TVS:
    snprintf(buf, siz, "%ld", (((tv_t *)data)->tv_sec));
    break;
  case TYPE_DOUBLE:
    snprintf(buf, siz, "%f", *((double *)data));
    break;
  case TYPE_BOOL:
    snprintf(buf, siz, "%d", *((char *)data));
    break;
  }

  return buf;
}

void txt_to_data(enum data_type typ, char *nam, char *fld, void *data,
                 size_t siz) {
  char *tmp;

  switch (typ) {
  case TYPE_STR:
    // A database field being bigger than local storage is a fatal error
    if (siz < (strlen(fld) + 1)) {
      LogPrint(BCLog::DBLAYER,
               "Field %s structure size %d is smaller than db %d" WHERE_FFL,
               nam, (int)siz, (int)strlen(fld) + 1, WHERE_FFL_PASS);
    }
    strcpy((char *)data, fld);
    break;
  case TYPE_BIGINT:
    if (siz != sizeof(int64_t)) {
      LogPrint(BCLog::DBLAYER, "Field %s bigint incorrect structure size %d - "
                          "should be %d" WHERE_FFL,
               nam, (int)siz, (int)sizeof(int64_t), WHERE_FFL_PASS);
    }
    *((long long *)data) = atoll(fld);
    break;
  case TYPE_INT:
    if (siz != sizeof(int32_t)) {
      LogPrint(
          BCLog::DBLAYER,
          "Field %s int incorrect structure size %d - should be %d" WHERE_FFL,
          nam, (int)siz, (int)sizeof(int32_t), WHERE_FFL_PASS);
    }
    *((int32_t *)data) = atoi(fld);
    break;
  case TYPE_BLOB:
    tmp = strdup(fld);
    if (!tmp) {
      LogPrint(BCLog::DBLAYER, "Field %s (%d) OOM" WHERE_FFL, nam, (int)strlen(fld),
               WHERE_FFL_PASS);
    }
    *((char **)data) = tmp;
    break;
  case TYPE_DOUBLE:
    if (siz != sizeof(double)) {
      LogPrint(
          BCLog::DBLAYER,
          "Field %s int incorrect structure size %d - should be %d" WHERE_FFL,
          nam, (int)siz, (int)sizeof(double), WHERE_FFL_PASS);
    }
    *((double *)data) = atof(fld);
    break;
  default:
    LogPrint(BCLog::DBLAYER, "Unknown field %s (%d) to convert" WHERE_FFL, nam,
             (int)typ, WHERE_FFL_PASS);
    break;
  }
}

static bool pg_conncheck(void) {
  if (PQstatus((const PGconn *)(dbSrv.db_conn)) != CONNECTION_OK) {
    LogPrint(BCLog::DBLAYER, "Connection to PostgreSQL lost: reconnecting.\n");
    PQreset((PGconn *)(dbSrv.db_conn));
    if (PQstatus((const PGconn *)(dbSrv.db_conn)) != CONNECTION_OK) {
      LogPrint(BCLog::DBLAYER, "Reconnect attempt failed.\n");
      return false;
    }
    
  }
  return true;
}
 
static void pg_rollback(void) {
  PGresult *res;
  res = PQexec((PGconn *)dbSrv.db_conn, "Rollback");
  if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
    LogPrint(BCLog::DBLAYER, "Rollback command failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
  }
 
  PQclear(res);
}

static bool pg_begin(void) {
  PGresult *res;

  /*
  * check to see that the backend connection was successfully made
  */
  if (!pg_conncheck())
    return false;

  /* debug = fopen("/tmp/trace.out","w"); */
  /* PQtrace(conn, debug);  */

  /* start a transaction block */
  res = PQexec((PGconn *)dbSrv.db_conn, "BEGIN");
  if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
    LogPrint(BCLog::DBLAYER, "Begin command failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return false;
  }

  /*
   * should PQclear PGresult whenever it is no longer needed to avoid
   * memory leaks
   */
  PQclear(res);

  return true;
}

static void pg_commit(void) {
  PGresult *res;
  res = PQexec((PGconn *)dbSrv.db_conn, "Commit");
  if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
    LogPrint(BCLog::DBLAYER, "Commit command failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
  }
 
  PQclear(res);
}

static void pg_close(void) {
  if (pg_conncheck())
    PQfinish((PGconn *)dbSrv.db_conn);
}

static bool pg_open(void) {
  std::string dbname = gArgs.GetArg("-dbname", "bitcoin");
  std::string dbuser = gArgs.GetArg("-dbuser", "postgres");
  std::string dbpass = gArgs.GetArg("-dbpass", "postgres");
  std::string dbhost = gArgs.GetArg("-dbhost", "127.0.0.1");
  std::string dbport = gArgs.GetArg("-dbport", "5432");
 
  dbSrv.db_conn = (void *)PQsetdbLogin(dbhost.c_str(), dbport.c_str(), NULL, NULL, dbname.c_str(),
                           dbuser.c_str(), dbpass.c_str());

  if (PQstatus((const PGconn *)dbSrv.db_conn) != CONNECTION_OK) {
    LogPrint(BCLog::DBLAYER, "failed to connect to postgresql: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    pg_close();
    return false;
  }

  if (PQsetClientEncoding((PGconn *)dbSrv.db_conn, "UTF-8")) {
    LogPrint(BCLog::DBLAYER, "failed to connect to postgresql: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    pg_close();
    return false;
  }

  LogPrint(BCLog::DBLAYER, "Connect to postgresql\n");
  return true;
}

static int pg_query_maxHeight() {
  PGresult *res;
  ExecStatusType rescode;
  int height = 0;

  res = PQexec((PGconn *)dbSrv.db_conn, "select max(height) from blk");
  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_query_maxHeight error: %s\n",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }
  height = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);
  return height;
}

static int pg_query_blk(unsigned char *hash) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  int id = 0;
  const char *paramvalues[1];

  paramvalues[i++] = data_to_buf(TYPE_HASH, (void *)(hash), NULL, 0);
  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_SELECT_BLK, i, NULL,
                     paramvalues, NULL, NULL, PQ_READ);

  free((char *)paramvalues[0]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_query_blk error: %s\n",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }

  if (PQntuples(res) > 0) {
    id = ntohl(*((int *)PQgetvalue(res, 0, 0)));
    }
  else
    id = -1;

  PQclear(res);

  return id;
}

static int pg_delete_blk(const unsigned char *hash) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  const char *paramvalues[1];

  paramvalues[i++] = data_to_buf(TYPE_HASH, (void *)hash, NULL, 0);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_DELETE_BLK, i, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);
  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_deletek error: %s\n",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    free((char *)paramvalues[0]);
    PQclear(res);
    return -1;
  }

  free((char *)paramvalues[0]);
  PQclear(res);

  return 0;
}

static int pg_add_tx_statics(int txid, int in_count, int out_count,
                             long long in_value, long long out_value) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  int n = 0;
  const char *paramvalues[5];

  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)&txid, NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)&in_count, NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)&out_count, NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BIGINT, (void *)&in_value, NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BIGINT, (void *)&out_value, NULL, 0);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_ADD_TX_STATICS, i, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);

  for (n = 0; n < i; n++)
    free((char *)paramvalues[n]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_add_tx_statics error: %s\n",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }

  PQclear(res);

  return 0;
}

static int pg_add_blk_statics(int blkid) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  const char *paramvalues[1];

  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)&blkid, NULL, 0);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_ADD_BLK_STATICS, i, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);
  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_add_blk_statics error: %s\n",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    free((char *)paramvalues[0]);
    PQclear(res);
    return -1;
  }

  free((char *)paramvalues[0]);
  PQclear(res);

  return 0;
}

static int pg_readd_blk(unsigned char *hash)
{
  PGresult *res;
  ExecStatusType rescode;
  int id = 0;
  const char *paramvalues[1];

  /* PG does a fine job with timestamps so we won't bother. */

  paramvalues[0] = data_to_buf(TYPE_BYTEA, (void *)(hash), NULL, 0);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_READD_BLK, 1, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);

  free((char *)paramvalues[0]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_readd_blk failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }
  id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return id;
}
 
static int pg_save_blk(unsigned char *hash, int height, int version,
                       unsigned char *prev_hash, unsigned char *mrkl_root,
                       long long time, int bits, unsigned int nonce, int blk_size,
                       unsigned char *work, int txnum, int pool_id, long long recv_time, int pool_bip, const char *ip) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  int n = 0;
  int id = 0;
  const char *paramvalues[15];

  // check if block in database
  if (pg_query_blk(hash) > 0) {
    LogPrint(BCLog::DBLAYER, "pg_save_blk : block %d exists in database.\n", height);
    return -1;
  }

  /* PG does a fine job with timestamps so we won't bother. */

  paramvalues[i++] = data_to_buf(TYPE_BYTEA, (void *)(hash), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&height), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&version), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BYTEA, (void *)(prev_hash), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BYTEA, (void *)(mrkl_root), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BIGINT, (void *)(&time), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&bits), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&nonce), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&blk_size), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BYTEA, (void *)(work), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&txnum), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&pool_id), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BIGINT, (void *)(&recv_time), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&pool_bip), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_STR, (void *)(ip), NULL, 0);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_SAVE_BLK, i, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);

  for (n = 0; n < i; n++)
    free((char *)paramvalues[n]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_save_blk failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }
  id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return id;
}

int pg_save_blk_tx(int blk_id, int tx_id, int idx) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  int n = 0;
  const char *paramvalues[3];

  /* PG does a fine job with timestamps so we won't bother. */

  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&blk_id), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&tx_id), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&idx), NULL, 0);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_SAVE_BLK_TX, i, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);

  for (n = 0; n < i; n++)
    free((char *)paramvalues[n]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_save_blk_tx failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }

  PQclear(res);
  return 0;
}

int pg_save_tx(unsigned char *hash, int version, int lock_time, bool coinbase,
               int tx_size, long long recv_time, const char *ip, 
               unsigned char* wtxid, int wsize, int vsize, int metatype) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  int n = 0;
  int id = 0;
  const char *paramvalues[11];

  /* PG does a fine job with timestamps so we won't bother. */

  paramvalues[i++] = data_to_buf(TYPE_BYTEA, (void *)(hash), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&version), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&lock_time), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BOOL, (void *)(&coinbase), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&tx_size), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BIGINT, (void *)(&recv_time), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_STR, (void *)(ip), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BYTEA, (void *)(wtxid), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&wsize), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&vsize), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&metatype), NULL, 0);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_SAVE_TX, i, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);

  for (n = 0; n < i; n++)
    free((char *)paramvalues[n]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_save_tx failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }

  id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return id;
}

int pg_empty_mempool() {
  PGresult *res;
  ExecStatusType rescode;

  res = PQexec((PGconn *)dbSrv.db_conn, "delete from mempool");
  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
      LogPrint(BCLog::DBLAYER, "delete mempool failed: %s",
               PQerrorMessage((const PGconn *)dbSrv.db_conn));
      PQclear(res);
      pg_rollback();
      return -1;
  }

  PQclear(res);
  return 0;
}

int pg_save_mempool(int tx_id, unsigned char *hash, double priority, long long nFee, long long inChainInputValue, int nTxSize,
                    long long nTime , int entryHeight, bool hadNoDependencies, int sigOpCount, long long modifiedFee,
                    int nModSize, int nUsageSize, bool dirty, long long nCountWithDescendants, long long nSizeWithDescendants,
                    long long nModFeesWithDescendants, bool boolspendsCoinbase) {

  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  int n = 0;
  const char *paramvalues[18];

  /* PG does a fine job with timestamps so we won't bother. */

  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&tx_id), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BYTEA, (void *)(hash), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_DOUBLE, (void *)(&priority), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BIGINT, (void *)(&nFee), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BIGINT, (void *)(&inChainInputValue), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&nTxSize), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BIGINT, (void *)(&nTime), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&entryHeight), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BOOL, (void *)(&hadNoDependencies), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&sigOpCount), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BIGINT, (void *)(&modifiedFee), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&nModSize), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&nUsageSize), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BOOL, (void *)(&dirty), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BIGINT, (void *)(&nCountWithDescendants), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BIGINT, (void *)(&nSizeWithDescendants), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BIGINT, (void *)(&nModFeesWithDescendants), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_BOOL, (void *)(&boolspendsCoinbase), NULL, 0);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_SAVE_MEMPOOL, i, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);

  for (n = 0; n < i; n++)
    free((char *)paramvalues[n]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_save_mempool failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }

  PQclear(res);

  return 0;
}

int pg_readd_tx(int txid) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  const char *paramvalues[1];

  /* PG does a fine job with timestamps so we won't bother. */

  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&txid), NULL, 0);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_READD_TX, i, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);

  free((char *)paramvalues[0]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_readd_tx failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }

  PQclear(res);

  return 0;
}
 

int pg_save_utx(int txid) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  const char *paramvalues[1];

  /* PG does a fine job with timestamps so we won't bother. */

  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&txid), NULL, 0);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_SAVE_UTX, i, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);

  free((char *)paramvalues[0]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_save_utx failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }

  PQclear(res);

  return 0;
}

static int pg_query_tx(const unsigned char *hash) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  int id = 0;
  const char *paramvalues[1];

  paramvalues[i++] = data_to_buf(TYPE_HASH, (void *)(hash), NULL, 0);
  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_SELECT_TX, i, NULL,
                     paramvalues, NULL, NULL, PQ_READ);
  free((char *)paramvalues[0]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_query_tx error: %s\n",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }

  if (PQntuples(res) > 0)
    id = ntohl(*((int *)PQgetvalue(res, 0, 0)));
  else
    id = -1;

  PQclear(res);

  return id;
}

static int pg_query_filter_tx(const unsigned char *hash) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  int id = -1;
  const char *paramvalues[1];

  paramvalues[i++] = data_to_buf(TYPE_HASH, (void *)(hash), NULL, 0);
  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_SELECT_FILTER_TX, i, NULL,
                     paramvalues, NULL, NULL, PQ_READ);
  free((char *)paramvalues[0]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_query_filter_tx error: %s\n",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }

  if (PQntuples(res) > 0)
    id = ntohl(*((int *)PQgetvalue(res, 0, 0)));
  else
    id = -1;

  PQclear(res);

  return id;
}

int pg_save_txin(int tx_id, int tx_idx, int prev_out_index, unsigned int sequence,
                 const unsigned char *script_sig, int script_len,
                 const unsigned char *prev_out, const unsigned char *witness, int witness_len) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  int n = 0;
  int id = 0;
  const char *paramvalues[7];

  /* PG does a fine job with timestamps so we won't bother. */

  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&tx_id), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&tx_idx), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&prev_out_index), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&sequence), NULL, 0);
  paramvalues[i++] =
      data_to_buf(TYPE_SCRIPT, (void *)(script_sig), NULL, script_len);
  paramvalues[i++] = data_to_buf(TYPE_BYTEA, (void *)(prev_out), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_SCRIPT, (void *)(witness), NULL, witness_len);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_SAVE_TXIN, i, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);

  for (n = 0; n < i; n++)
    free((char *)paramvalues[n]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_save_txin failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }

  id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return id;
}

int pg_save_txout(int tx_id, int idx, const unsigned char *scriptPubKey,
                  int script_len, long long nValue, int txout_type) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  int n = 0;
  int id = 0;
  const char *paramvalues[5];

  /* PG does a fine job with timestamps so we won't bother. */

  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&tx_id), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&idx), NULL, 0);
  paramvalues[i++] =
      data_to_buf(TYPE_SCRIPT, (void *)(scriptPubKey), NULL, script_len);
  paramvalues[i++] = data_to_buf(TYPE_BIGINT, (void *)(&nValue), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&txout_type), NULL, 0);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_SAVE_TXOUT, i, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);

  for (n = 0; n < i; n++)
    free((char *)paramvalues[n]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_save_txout failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }

  id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return id;
}

int pg_save_addr(const char *addr, const char *hash160) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  int n = 0;
  int id = 0;
  const char *paramvalues[2];

  /* PG does a fine job with timestamps so we won't bother. */

  paramvalues[i++] = data_to_buf(TYPE_STR, (void *)(addr), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_ADDR, (void *)(hash160), NULL, 20);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_SAVE_ADDR, i, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);

  for (n = 0; n < i; n++)
    free((char *)paramvalues[n]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_save_addr failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }

  id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  return id;
}

static int pg_delete_tx(int txid) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  const char *paramvalues[1];

  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&txid), NULL, 0);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_DELETE_TX, i, NULL,
                     paramvalues, NULL, NULL, PQ_WRITE);
  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_delete_tx error: %s\n",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    free((char *)paramvalues[0]);
    PQclear(res);
    return -1;
  }

  free((char *)paramvalues[0]);
  PQclear(res);

  return 0;
}

static int pg_delete_all_utx() {
  PGresult *res;
  ExecStatusType rescode;

  LogPrint(BCLog::DBLAYER, "Start delete utx\n");
  while (true) {

      pg_begin();
      res = PQexec((PGconn *)dbSrv.db_conn, "select * from utx limit 1");
      rescode = PQresultStatus(res);
      if (!PGOK(rescode)) {
          LogPrint(BCLog::DBLAYER, "query utx failed: %s",
                   PQerrorMessage((const PGconn *)dbSrv.db_conn));
          PQclear(res);
          pg_rollback();
          return -1;
      }

      if (PQntuples(res) <= 0)
      {
        PQclear(res);
        pg_commit();
        return 0;    
      }
      
      PQclear(res); 
      res = PQexec((PGconn *)dbSrv.db_conn, DEFAULT_DELETE_SOME_UTX);
      rescode = PQresultStatus(res);
      if (!PGOK(rescode)) {
          LogPrint(BCLog::DBLAYER, "pg_delete_all_utx error: %s\n",
                   PQerrorMessage((const PGconn *)dbSrv.db_conn));
          PQclear(res);
          pg_rollback();
          return -1;
      }

      PQclear(res);
      pg_commit();
  }

  LogPrint(BCLog::DBLAYER, "Delete all utx finished.\n");

  return 0;
}

static bool pg_query_addr_out(int addr_id, int txout_id) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  int n = 0;
  bool result;
  const char *paramvalues[2];

  /* PG does a fine job with timestamps so we won't bother. */

  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&addr_id), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&txout_id), NULL, 0);
  
  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_SELECT_ADDR_OUT, i, NULL,
                     paramvalues, NULL, NULL, PQ_READ);
  for (n = 0; n < i; n++)
      free((char *)paramvalues[n]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_query_addr_out error: %s\n",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }

  if (PQntuples(res) > 0)
    result = true;
  else
    result = false;

  PQclear(res);

  return result;
}

int pg_save_addr_out(int addr_id, int txout_id) {
  PGresult *res;
  ExecStatusType rescode;
  int i = 0;
  int n = 0;
  const char *paramvalues[2];

  if (pg_query_addr_out(addr_id, txout_id))
  {
    /* code */
    return 0;
  }

  /* PG does a fine job with timestamps so we won't bother. */

  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&addr_id), NULL, 0);
  paramvalues[i++] = data_to_buf(TYPE_INT, (void *)(&txout_id), NULL, 0);

  res = PQexecParams((PGconn *)dbSrv.db_conn, DEFAULT_SAVE_ADDR_OUT, i, NULL,
                     paramvalues, NULL, NULL, false);

  for (n = 0; n < i; n++)
    free((char *)paramvalues[n]);

  rescode = PQresultStatus(res);
  if (!PGOK(rescode)) {
    LogPrint(BCLog::DBLAYER, "pg_save_addr_out failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }
  PQclear(res);

  return 0;
}

int pg_sync_mempool_start() {
  PGresult *res;

  /* PG does a fine job with timestamps so we won't bother. */

  res = PQexec((PGconn *)dbSrv.db_conn, "select sync_mempool_start()");

  if (!res) {
    LogPrint(BCLog::DBLAYER, "pg_sync_mempool_start failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }
  PQclear(res);

  return 0;
}

int pg_sync_mempool_end() {
  PGresult *res;

  /* PG does a fine job with timestamps so we won't bother. */

  res = PQexec((PGconn *)dbSrv.db_conn, "select sync_mempool_end()");

  if (!res) {
    LogPrint(BCLog::DBLAYER, "pg_sync_mempool_end failed: %s",
             PQerrorMessage((const PGconn *)dbSrv.db_conn));
    PQclear(res);
    return -1;
  }
  PQclear(res);

  return 0;
}

struct SERVER_DB_OPS postgresql_db_ops = {
  .save_blk = pg_save_blk,
  .delete_blk = pg_delete_blk,
  .add_blk_statics = pg_add_blk_statics,
  .add_tx_statics = pg_add_tx_statics,
  .save_blk_tx = pg_save_blk_tx,
  .save_tx = pg_save_tx,
  .readd_tx = pg_readd_tx,
  .save_utx = pg_save_utx,
  .save_txin = pg_save_txin,
  .save_txout = pg_save_txout,
  .save_addr = pg_save_addr,
  .save_addr_out = pg_save_addr_out,
  .empty_mempool = pg_empty_mempool,
  .save_mempool = pg_save_mempool,
  .sync_mempool_start = pg_sync_mempool_start,
  .sync_mempool_end = pg_sync_mempool_end,
  .query_tx = pg_query_tx,
  .query_filter_tx = pg_query_filter_tx,
  .delete_tx = pg_delete_tx,
  .delete_all_utx = pg_delete_all_utx,
  .query_maxHeight = pg_query_maxHeight,
  .begin = pg_begin,
  .rollback = pg_rollback,
  .commit = pg_commit,
  .open = pg_open,
  .close = pg_close,
  .connected = pg_conncheck,
};

#endif /* HAVE_POSTGRESQL */
