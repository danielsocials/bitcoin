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

#include <stdint.h>
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "chain.h"

#define HAVE_POSTGRESQL


struct SERVER_DB_OPS {
  int (*save_blk)(unsigned char *hash, int height, int version,
                  unsigned char *prev_hash, unsigned char *mrkl_root,
                  long long time, int bits, unsigned int nonce, int blk_size,
                  unsigned char *work, int txnum, int pool_id, long long recv_time, int pool_bip, const char *ip);

  int (*delete_blk)(const unsigned char *hash);
  int (*add_blk_statics)(int blkid);
  int (*add_tx_statics)(int txid, int in_count, int out_count,
                        long long in_value, long long out_value);
  int (*save_blk_tx)(int blk_id, int tx_id, int idx);
  int (*save_tx)(unsigned char *hash, int version, int lock_time, bool coinbase,
                 int tx_size,  long long recv_time,
                 const char *ip, unsigned char* wtxid, int wsize, int vsize, int metatype);
  int (*readd_tx)(int txId);
  int (*save_utx)(int txid);
  int (*save_txin)(int tx_id, int tx_idx, int prev_out_index, unsigned int sequence,
                   const unsigned char *script_sig, int script_len,
                   const unsigned char *prev_out, const unsigned char *witness, int witness_len);
  int (*save_txout)(int tx_id, int idx, const unsigned char *scriptPubKey,
                    int script_len, long long nValue, int txout_type);
  int (*save_addr)(const char *addr, const char *hash160);
  int (*save_addr_out)(int addr_id, int txout_id);
  int (*empty_mempool)();
  int (*save_mempool)(int, unsigned char *, double, long long, long long, int, long long, int, bool, int, long long, int,int, bool,long long,long long,long long,bool);
  int (*sync_mempool_start)();
  int (*sync_mempool_end)();

  int (*query_tx)(const unsigned char *hash);
  int (*query_filter_tx)(const unsigned char *hash);
  int (*delete_tx)(int txid);
  int (*delete_all_utx)();

  int (*query_maxHeight)();

  bool (*begin)(void);
  void (*rollback)(void);
  void (*commit)(void);
  bool (*open)(void);
  void (*close)(void);
  bool (*connected)(void);
};

enum SERVER_DB_ENGINE {
  SDB_SQLITE,
  SDB_MYSQL,
  SDB_POSTGRESQL,
};

struct DBSERVER {
  enum SERVER_DB_ENGINE db_eng;
  struct SERVER_DB_OPS *db_ops;

  void *db_conn;
};

bool dbOpen();
void dbClose();
int  dbSaveBlock(const CBlockIndex *blockindex, CBlock &block);
int  dbDisconnectBlock(CBlock &block);
int  dbSaveTx(const CTransaction &tx);
int  dbAcceptTx(const CTransaction &tx);
int  dbRemoveTx(uint256 txhash);
int  dbSync(int newHeight);
int  dbDeleteAllUtx();
int  dbFilterTx(uint256 &hash );
int  getPoolId(const CTransaction &tx);
int  dbSynMempoolStart();
int  dbSynMempoolEnd();

extern int getTxMetaType(const CTransaction &tx);

#ifdef HAVE_SQLITE3
extern struct SERVER_DB_OPS sqlite_db_ops;
#endif
#ifdef HAVE_MYSQL
extern struct SERVER_DB_OPS mysql_db_ops;
#endif
#ifdef HAVE_POSTGRESQL
extern struct SERVER_DB_OPS postgresql_db_ops;
#endif

extern struct DBSERVER dbsrv;
