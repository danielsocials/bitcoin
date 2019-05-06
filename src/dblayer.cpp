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

#include "base58.h"
#include "keystore.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "version.h"
#include "serialize.h"
#include "pubkey.h"
#include "sync.h"
//#include "util.h"
#include "init.h"
#include <bech32.h>
#include "script/standard.h"
#include "util/strencodings.h"
#include "policy/policy.h"
#include "validation.h"
#include "txmempool.h"
#include "key_io.h"
#include "dblayer.h"
#include "pool.h"
#include <shutdown.h>

#include <boost/foreach.hpp>

extern CTxMemPool mempool;
using namespace std;

#define INDEX_FOREACH(index, a, b)                                             \
  for (unsigned int index = static_cast<unsigned int>(-1);                     \
       index == static_cast<unsigned int>(-1);)                                \
  BOOST_FOREACH(a, b) if (++index, true)

struct DBSERVER dbSrv = {
#if defined(HAVE_SQLITE3)
  .db_eng = SDB_SQLITE, 
  .db_ops = &sqlite_db_ops, 
#elif defined(HAVE_MYSQL)
  .db_eng = SDB_MYSQL,
  .db_ops = &mydb_ops,
#elif defined(HAVE_POSTGRESQL)
  .db_eng = SDB_POSTGRESQL,
  .db_ops = &postgresql_db_ops,
#endif
  .db_conn = NULL,
};


//current block height
static int maxHeight = 0;

CTxDestination DecodeDestination(const std::string& str, const CChainParams& params)
{
    std::vector<unsigned char> data;
    uint160 hash;
    if (DecodeBase58Check(str, data)) {
        // base58-encoded Bitcoin addresses.
        // Public-key-hash-addresses have version 0 (or 111 testnet).
        // The data vector contains RIPEMD160(SHA256(pubkey)), where pubkey is the serialized public key.
        const std::vector<unsigned char>& pubkey_prefix = params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
        if (data.size() == hash.size() + pubkey_prefix.size() && std::equal(pubkey_prefix.begin(), pubkey_prefix.end(), data.begin())) {
            std::copy(data.begin() + pubkey_prefix.size(), data.end(), hash.begin());
            return CKeyID(hash);
        }
        // Script-hash-addresses have version 5 (or 196 testnet).
        // The data vector contains RIPEMD160(SHA256(cscript)), where cscript is the serialized redemption script.
        const std::vector<unsigned char>& script_prefix = params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
        if (data.size() == hash.size() + script_prefix.size() && std::equal(script_prefix.begin(), script_prefix.end(), data.begin())) {
            std::copy(data.begin() + script_prefix.size(), data.end(), hash.begin());
            return CScriptID(hash);
        }
    }
    data.clear();
    auto bech = bech32::Decode(str);
    if (bech.second.size() > 0 && bech.first == params.Bech32HRP()) {
        // Bech32 decoding
        int version = bech.second[0]; // The first 5 bit symbol is the witness version (0-16)
        // The rest of the symbols are converted witness program bytes.
        if (ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, bech.second.begin() + 1, bech.second.end())) {
            if (version == 0) {
                {
                    WitnessV0KeyHash keyid;
                    if (data.size() == keyid.size()) {
                        std::copy(data.begin(), data.end(), keyid.begin());
                        return keyid;
                    }
                }
                {
                    WitnessV0ScriptHash scriptid;
                    if (data.size() == scriptid.size()) {
                        std::copy(data.begin(), data.end(), scriptid.begin());
                        return scriptid;
                    }
                }
                return CNoDestination();
            }
            if (version > 16 || data.size() < 2 || data.size() > 40) {
                return CNoDestination();
            }
            WitnessUnknown unk;
            unk.version = version;
            std::copy(data.begin(), data.end(), unk.program);
            unk.length = data.size();
            return unk;
        }
    }
    return CNoDestination();
}

namespace
{
class DestinationStr: public boost::static_visitor<std::string>
{
private:
    const CChainParams& m_params;

public:
    DestinationStr(const CChainParams& params) : m_params(params) {}

    std::string operator()(const CKeyID& id) const
    {
        std::vector<unsigned char> data = m_params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const CScriptID& id) const
    {
        std::vector<unsigned char> data = m_params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
        data.insert(data.end(), id.begin(), id.end());
        return EncodeBase58Check(data);
    }

    std::string operator()(const WitnessV0KeyHash& id) const
    {
        std::vector<unsigned char> data = {0};
        data.reserve(33);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, id.begin(), id.end());
        return bech32::Encode(m_params.Bech32HRP(), data);
    }

    std::string operator()(const WitnessV0ScriptHash& id) const
    {
        std::vector<unsigned char> data = {0};
        data.reserve(53);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, id.begin(), id.end());
        return bech32::Encode(m_params.Bech32HRP(), data);
    }

    std::string operator()(const WitnessUnknown& id) const
    {
        if (id.version < 1 || id.version > 16 || id.length < 2 || id.length > 40) {
            return {};
        }
        std::vector<unsigned char> data = {(unsigned char)id.version};
        data.reserve(1 + (id.length * 8 + 4) / 5);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, id.program, id.program + id.length);
        return bech32::Encode(m_params.Bech32HRP(), data);
    }

    std::string operator()(const CNoDestination& no) const { return {}; }
};
}
     

std::string GetDestinationStr(const CTxDestination& dest)
{
    return boost::apply_visitor(DestinationStr(Params()), dest);
}

int  getPoolId(const CTransaction &tx) {

    int id = -1;

    id = getPoolIdByPrefix(&tx.vin[0].scriptSig[0], tx.vin[0].scriptSig.size());
    if (id == 0) {
        // as p2pool
        if ((tx.vin[0].scriptSig.size() == 4) && (tx.vout.size()>3)) 
            return POOL_P2POOL;

        if (tx.vout.size()>20) 
            return POOL_P2POOL;

        for (unsigned int i = 0; i < tx.vout.size(); i++) {
            const CTxOut &txout = tx.vout[i];
            txnouttype txout_type;
            vector<CTxDestination> addresses;
            int nRequired;
 
            if (!ExtractDestinations(txout.scriptPubKey, txout_type, addresses, nRequired))
                 return id;

            BOOST_FOREACH(const CTxDestination & dest, addresses) {
                if (boost::get<CNoDestination>(&dest))
                    continue;
                id = getPoolIdByAddr(EncodeDestination(dest).c_str());
                if (id != 0)
                   return id;
                }
            }
    }

    return id;
}

bool dbOpen() {
  if (!dbSrv.db_ops->open()) {
    LogPrint(BCLog::DBLAYER, "\n db open fail!\n");
    return false;
  }
  LogPrint(BCLog::DBLAYER, "\n db open success!\n");

  return true;
}

void dbClose() { dbSrv.db_ops->close(); }

int dbSaveTx(const CTransaction &tx) {
  uint256 hash = tx.GetHash();
  const int32_t version = tx.nVersion;
  const uint32_t lock_time = tx.nLockTime;
  bool coinbase = tx.IsCoinBase();
  unsigned int tx_size = ::GetSerializeSize(tx, PROTOCOL_VERSION);
  int in_count = tx.vin.size();
  int out_count = tx.vout.size();
  long long in_value = 0;
  long long out_value = 0;
  uint256 wtxid;
  int wsize = (int)::GetSerializeSize(tx, PROTOCOL_VERSION);
  int vsize =  (int)::GetVirtualTransactionSize(tx);
  int metaType = getTxMetaType(tx);

  int tx_id = dbSrv.db_ops->query_tx(hash.begin());
  if (tx_id == -1) {

    if (!tx.IsCoinBase())
        wtxid = tx.GetWitnessHash();
    else
        wtxid.SetNull();

    tx_id = dbSrv.db_ops->save_tx(hash.begin(), version, lock_time, coinbase,
                                  tx_size, tx.nTimeReceived,
                                  tx.relayIp.c_str(), wtxid.begin(), wsize, vsize, metaType);
    if (tx_id == -1) {
      LogPrint(BCLog::DBLAYER, "save_tx error tx hash %s \n", hash.ToString());
      return -1;
    }

    INDEX_FOREACH(tx_idx, const CTxIn & txin, tx.vin) {
      CTransactionRef txp;
      uint256 hashBlockp;
      int prev_out_index = txin.prevout.n;
      uint256 prev_out = txin.prevout.hash;
      std::vector<unsigned char> txinwitness;

      if (GetTransaction(txin.prevout.hash, txp, Params().GetConsensus(), hashBlockp)) {
        in_value += txp->vout[txin.prevout.n].nValue;
      }
      else if (tx_idx !=0 ) {
          LogPrint(BCLog::DBLAYER, "Tx hash: %s\n", txin.prevout.hash.ToString());
          LogPrint(BCLog::DBLAYER, "GetTransaction fail, check if txindex is opening!\n");
          return -1;
      }

      if (!txin.scriptWitness.IsNull()) {
          for (unsigned int j = 0; j < txin.scriptWitness.stack.size(); j++) {
              std::vector<unsigned char> item = txin.scriptWitness.stack[j];
              if (item.size()<=0)
                  continue;
              txinwitness.insert(txinwitness.end(),item.begin(), item.end());
          }
      }
      else {
          txinwitness.clear();
      }

      int txin_id = dbSrv.db_ops->save_txin(
          tx_id, tx_idx, prev_out_index, txin.nSequence, &txin.scriptSig[0],
          txin.scriptSig.size(), prev_out.begin(), (const unsigned char *)(&*txinwitness.begin()), txinwitness.size());
      if (txin_id == -1) {
        LogPrint(BCLog::DBLAYER, "save_txin error txid %d txin index %d \n", tx_id,
                 tx_idx);
        return -1;
      }
    }

    for (unsigned int i = 0; i < tx.vout.size(); i++) {
      const CTxOut &txout = tx.vout[i];
      txnouttype txout_type;
      vector<CTxDestination> addresses;
      int nRequired;

      out_value += txout.nValue;

      if (!ExtractDestinations(txout.scriptPubKey, txout_type, addresses,
                               nRequired)) {
        // LogPrint(BCLog::DBLAYER, "ExtractDestinations error: \n");
      }

      int txout_id = dbSrv.db_ops->save_txout(tx_id, i, &txout.scriptPubKey[0],
                                              txout.scriptPubKey.size(),
                                              txout.nValue, txout_type);
      if (txout_id == -1) {
        LogPrint(BCLog::DBLAYER, "save_txout error txid %d txout index %d \n", tx_id,
                 i);
        return -1;
      }

      BOOST_FOREACH(const CTxDestination & dest, addresses) {
        if (boost::get<CNoDestination>(&dest))
          continue;

        int addr_id = dbSrv.db_ops->save_addr(
            EncodeDestination(dest).c_str(), GetDestinationStr(dest).c_str());
        if (addr_id == -1) {
          LogPrint(BCLog::DBLAYER, "save_addr error addr: %s \n", GetDestinationStr(dest));
          return -1;
        }

        if (dbSrv.db_ops->save_addr_out(addr_id, txout_id) == -1) {
          LogPrint(BCLog::DBLAYER, "save_addr_out error addr: %s \n",
                   GetDestinationStr(dest));
          return -1;
        }
      }
    }

    dbSrv.db_ops->add_tx_statics(tx_id, in_count, out_count, in_value,
                                 out_value);
  }
  else
    LogPrint(BCLog::DBLAYER, "save tx fail : %d\n", tx_id);

  return tx_id;
}

int dbSaveBlock(const CBlockIndex *blockindex, CBlock &block) {

  if (!gArgs.GetArg("-savetodb", false))
      return 0;

  /*
  * For these case:
  * 1. when db reconnect 
  * 2. multi node or thread insert db 
  */
  if (dbSync(blockindex->nHeight)==-1)
     return -1;

  uint256 hash = block.GetHash();
  uint256 prev_hash;

  bool liteDb = gArgs.GetArg("-litedb", false);
  if (liteDb) {
      int liteHeight = gArgs.GetArg("-liteheight", 0);
      if (liteHeight > blockindex->nHeight)
          return 0;
  }

  if (blockindex->pprev)
    prev_hash = blockindex->pprev->GetBlockHash();
  else
    LogPrint(BCLog::DBLAYER, "prev blk is Null, blk hash : %s\n",
             block.GetHash().GetHex());

  int blk_size = (int)::GetSerializeSize(block, PROTOCOL_VERSION);
  int height = blockindex->nHeight;
  int version = block.nVersion;
  uint256 mrkl_root = block.hashMerkleRoot;
  uint64_t time = block.GetBlockTime();
  unsigned int nonce = block.nNonce;
  int bits = block.nBits;
  uint256 work = ArithToUint256(blockindex->nChainWork);
  int blk_id = -1;
  int pool_id = POOL_UNKNOWN;
  int poolBip = BIP_DEFAULT;

  if (dbSrv.db_ops->begin() == -1) {
    LogPrint(BCLog::DBLAYER, "block save first roll back height: %d \n", height);
    goto rollback;
  }

  poolBip = getPoolSupportBip(&block.vtx[0]->vin[0].scriptSig[0], block.vtx[0]->vin[0].scriptSig.size(), version);
  pool_id = getPoolId(*block.vtx[0]);
  blk_id = dbSrv.db_ops->save_blk(
      hash.begin(), height, version, prev_hash.begin(), mrkl_root.begin(), time,
      bits, nonce, blk_size, work.begin(), block.vtx.size(), pool_id, block.nTimeReceived, poolBip, block.relayIp.c_str());
  if (blk_id == -1) {
    dbSrv.db_ops->rollback();

    if (dbSrv.db_ops->begin() == -1) {
      LogPrint(BCLog::DBLAYER, "block save second roll back height: %d \n", height);
      goto rollback;
    }
    blk_id = dbSrv.db_ops->save_blk(
        hash.begin(), height, version, prev_hash.begin(), mrkl_root.begin(),
        time, bits, nonce, blk_size, work.begin(), block.vtx.size(), pool_id, block.nTimeReceived,poolBip, block.relayIp.c_str());
    if (blk_id == -1) {
      LogPrint(BCLog::DBLAYER, "block save fail height: %d \n", height);
      goto rollback;
    }
  }

  INDEX_FOREACH(idx, CTransactionRef tx, block.vtx) {
    int tx_id = dbSaveTx(*tx);
    if (tx_id == -1) {
        LogPrint(BCLog::DBLAYER, "tx save fail block height: %d,  txhash: %s \n",
               height, tx->GetHash().ToString());
        goto rollback;
    }
    if (dbSrv.db_ops->save_blk_tx(blk_id, tx_id, idx)==-1) {
        LogPrint(BCLog::DBLAYER, "blk_tx save fail block : %d,  txhash: %s \n",
               height, tx->GetHash().ToString());
        goto rollback;
        }
  }

  if (dbSrv.db_ops->add_blk_statics(blk_id) == -1) {
      LogPrint(BCLog::DBLAYER, "add_blk_statics fail block : %d\n", height);
        goto rollback;
        }

  dbSrv.db_ops->commit();

  maxHeight = height;

  return 0;

rollback:

  dbSrv.db_ops->rollback();
  return -1;

}

//int dbDumpMempool() {
// 
//  if (!GetArg("-savetodb", false))
//      return -1;
//
//  if (dbSrv.db_ops->begin() == -1) {
//    LogPrint("dblayer", "dbdumpMempool roll back: begin failed \n");
//    dbSrv.db_ops->rollback();
//    return -1;
//  }
//
//  //clear mempool first
//  LOCK(mempool.cs);
//  dbSrv.db_ops->empty_mempool();
//
//  for (CTxMemPool::txiter  mi = mempool.mapTx.begin(); mi != mempool.mapTx.end(); mi++) {
//    uint256 hash = mi->GetTx().GetHash();
//    double entryPriority = mi->GetPriority(chainActive.Height());
//    CAmount nFee =mi->GetFee();
//    CAmount inChainInputValue = mi->GetInputValue();
//    size_t nTxSize = mi->GetTxSize();
//    int64_t nTime = mi->GetTime();
//    unsigned int entryHeight = mi->GetHeight();
//    bool hadNoDependencies = mi->WasClearAtEntry();
//    unsigned int sigOpCount = mi->GetSigOpCount();
//    int64_t modifiedFee = mi->GetModifiedFee();
//    size_t nModSize = mi->GetTx().CalculateModifiedSize(nTxSize);
//    size_t nUsageSize = mi->DynamicMemoryUsage();
//    bool dirty = mi->IsDirty();
//    uint64_t nCountWithDescendants = mi->GetCountWithDescendants();
//    uint64_t nSizeWithDescendants = mi->GetSizeWithDescendants();
//    CAmount nModFeesWithDescendants = mi->GetModFeesWithDescendants();
//    bool spendsCoinbase = mi->GetSpendsCoinbase();
//  
//    if (dbSrv.db_ops->save_mempool(0, hash.begin(), entryPriority ,nFee ,inChainInputValue ,nTxSize ,nTime ,entryHeight ,hadNoDependencies ,sigOpCount ,modifiedFee ,nModSize ,nUsageSize ,dirty ,nCountWithDescendants ,nSizeWithDescendants ,nModFeesWithDescendants ,spendsCoinbase) == -1) { 
//      LogPrint("dblayer", "dbdumpMempool roll back: tx %s \n", hash.ToString());
//      dbSrv.db_ops->rollback();
//      return -1;
//      }
//    }
//
//  dbSrv.db_ops->commit();
//  return 0;
// 
//}

//void dbSaveToMempool(int tx_id, const CTransaction &tx) {
// 
//  uint256 hash = tx.GetHash();
//  CTxMemPool::txiter mi = mempool.mapTx.find(hash);
//  if (mi == mempool.mapTx.end())
//      return;
//
//  double entryPriority = mi->GetPriority(chainActive.Height());
//  CAmount nFee =mi->GetFee();
//  CAmount inChainInputValue = mi->GetInputValue();
//  size_t nTxSize = mi->GetTxSize();
//  int64_t nTime = mi->GetTime();
//  unsigned int entryHeight = mi->GetHeight();
//  bool hadNoDependencies = mi->WasClearAtEntry();
//  unsigned int sigOpCount = mi->GetSigOpCount();
//  int64_t modifiedFee = mi->GetModifiedFee();
//  size_t nModSize = tx.CalculateModifiedSize(nTxSize);
//  size_t nUsageSize = mi->DynamicMemoryUsage();
//  bool dirty = mi->IsDirty();
//  uint64_t nCountWithDescendants = mi->GetCountWithDescendants();
//  uint64_t nSizeWithDescendants = mi->GetSizeWithDescendants();
//  CAmount nModFeesWithDescendants = mi->GetModFeesWithDescendants();
//  bool spendsCoinbase = mi->GetSpendsCoinbase();
//
//  if (dbSrv.db_ops->save_mempool(tx_id, hash.begin(), entryPriority ,nFee ,inChainInputValue ,nTxSize ,nTime ,entryHeight ,hadNoDependencies ,sigOpCount ,modifiedFee ,nModSize ,nUsageSize ,dirty ,nCountWithDescendants ,nSizeWithDescendants ,nModFeesWithDescendants ,spendsCoinbase) == -1) { 
//    LogPrint("dblayer", "dbAcceptTx roll back: %s \n", tx.GetHash().ToString());
//    dbSrv.db_ops->rollback();
//  }
//}

int dbAcceptTx(const CTransaction &tx) {

  if (!gArgs.GetArg("-savetodb", false))
      return 0;

  if (dbSrv.db_ops->begin() == -1) {
    LogPrint(BCLog::DBLAYER, "dbAcceptTx roll back: %s \n", tx.GetHash().ToString());
    dbSrv.db_ops->rollback();
    return -1;
  }
  int tx_id = dbSaveTx(tx);
  if (tx_id==-1) {
    LogPrint(BCLog::DBLAYER, "dbAcceptTx roll back: %s \n", tx.GetHash().ToString());
    dbSrv.db_ops->rollback();
    return -1;
  }
 
  if (dbSrv.db_ops->save_utx(tx_id) == -1) {
    LogPrint(BCLog::DBLAYER, "dbAcceptTx roll back: %s \n", tx.GetHash().ToString());
    dbSrv.db_ops->rollback();
    return -1;
  }
  dbSrv.db_ops->commit();

  return 0;
}

int dbRemoveTx(uint256 txhash) {
  if (!gArgs.GetArg("-savetodb", false))
      return 0;

  int txid = dbSrv.db_ops->query_tx(txhash.begin());
  if (txid == -1) {
    LogPrint(BCLog::DBLAYER, "dbRemoveTx: tx: %s  not in database  \n", txhash.ToString());
    return 0;
  }
  if (dbSrv.db_ops->begin() == -1) {
    dbSrv.db_ops->rollback();
    LogPrint(BCLog::DBLAYER, "dbRemoveTx roll back: %s \n", txhash.ToString());
    return -1;
  }

  if (dbSrv.db_ops->delete_tx(txid) == -1) {
    LogPrint(BCLog::DBLAYER, "dbRemoveTx roll back: %s \n", txhash.ToString());
    dbSrv.db_ops->rollback();
    return -1;
  }

  dbSrv.db_ops->commit();

  return 0;
}

int testGetPool() {
  int i = 0;
  CBlock block;
  CBlockIndex *pblockindex;
  int pool_id =-1;

  int maxHeight = dbSrv.db_ops->query_maxHeight();
  if (maxHeight == -1) {
        return -1;
  }

  // syndb
  for (; i < (chainActive.Height() + 1); i++) {

    if (ShutdownRequested()) {
      LogPrint(BCLog::DBLAYER, "Shutdown requested. Exiting.");
      return -1;
    }

    pblockindex = chainActive[i];
    int64_t nStart = GetTimeMicros();
    if (!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus())) {
      return -1;
    }
    LogPrint(BCLog::DBLAYER, "- read block from disk: %.2fms\n",
             (GetTimeMicros() - nStart) * 0.001);

    pool_id = getPoolId(*block.vtx[0]);
    if (pool_id==-1)
        LogPrint(BCLog::DBLAYER, "- can't get poolId height %d\n", pblockindex->nHeight);
  }

  return 0;
}

int testGetPool1() {
  int i = 0;
  CBlock block;
  CBlockIndex *pblockindex;
  int pool_id =-1;

  pblockindex = chainActive[367876];
  if (!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus())) {
    return -1;
  }

  pool_id = getPoolId(*block.vtx[0]);
 

  int maxHeight = dbSrv.db_ops->query_maxHeight();
  if (maxHeight == -1) {
        return -1;
  }

  // syndb
  for (i=360419; i >0; i--) {

    if (ShutdownRequested()) {
      LogPrint(BCLog::DBLAYER, "Shutdown requested. Exiting.");
      return -1;
    }

    pblockindex = chainActive[i];
    if (!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus())) {
      return -1;
    }
    pool_id = getPoolId(*block.vtx[0]);
    if (pool_id==-1)
        LogPrint(BCLog::DBLAYER, "------ can't get poolId height %d size %d %s\n", pblockindex->nHeight, block.vtx[0]->vin[0].scriptSig.size(), &block.vtx[0]->vin[0].scriptSig[0]);
  }

  return 0;
}

int dbSync(int newHeight) {
  int i = 0;
  CBlock block;
  CBlockIndex *pblockindex;
  static bool syncing=false;
  int currentHeight = 0; 


  if ((newHeight <= (maxHeight+1)) && (maxHeight != 0)) return 0; 

  if (syncing) return 0;

  syncing=true;

  if (!dbSrv.db_ops->connected()) {
        syncing=false;
        return -1;
  }

  maxHeight = dbSrv.db_ops->query_maxHeight();

  bool liteDb = gArgs.GetArg("-litedb", false);
  if (liteDb) {
      int liteHeight = gArgs.GetArg("-liteheight", 0);
      maxHeight = (maxHeight>liteHeight) ? maxHeight:liteHeight;
  }

  currentHeight  =  maxHeight ? (maxHeight + 1) : 0; 

  // syndb
  if (currentHeight < chainActive.Height()) {
    for (i = currentHeight; i < (chainActive.Height() + 1); i++) {

      if (ShutdownRequested()) {
        syncing=false;
        LogPrint(BCLog::DBLAYER, "Shutdown requested. Exiting.");
        syncing=false;
        return -1;
      }

      pblockindex = chainActive[i];
      int64_t nStart = GetTimeMicros();
      if (!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus())) {
        syncing=false;
        return -1;
      }
      LogPrint(BCLog::DBLAYER, "- read block from disk: %.2fms\n",
               (GetTimeMicros() - nStart) * 0.001);
      if (dbSaveBlock(pblockindex, block) == -1) {
        syncing=false;
        return -1;
      }
      LogPrint(BCLog::DBLAYER, "- Save block to db: %.2fms height %d\n",
               (GetTimeMicros() - nStart) * 0.001, pblockindex->nHeight);
    }
    currentHeight = i;
  }

  syncing=false;
  return 0;
}

int dbDisconnectBlock(CBlock &block) {

  if (dbSrv.db_ops->begin() == -1) {
    dbSrv.db_ops->rollback();
    return -1;
  }

  // set blk to side chain
  if (dbSrv.db_ops->delete_blk(block.GetHash().begin()) == -1) {
    LogPrint(BCLog::DBLAYER, "-delete blk fail: %s\n", block.GetHash().ToString());
    dbSrv.db_ops->rollback();
    return -1;
  }

  dbSrv.db_ops->commit();
  return 0;
}

int dbSynMempoolStart() {
  if (dbSrv.db_ops->begin() == -1) {
    dbSrv.db_ops->rollback();
    return -1;
  }

  // sync tx hash to mempool table
  if (dbSrv.db_ops->sync_mempool_start() == -1) {
    LogPrint(BCLog::DBLAYER, "-sync mempool start fail! \n");
    dbSrv.db_ops->rollback();
    return -1;
  }

  dbSrv.db_ops->commit();
  return 0; 
}    

int dbSynMempoolEnd() {
  if (dbSrv.db_ops->begin() == -1) {
    dbSrv.db_ops->rollback();
    return -1;
  }

  // sync tx hash to mempool table
  if (dbSrv.db_ops->sync_mempool_end() == -1) {
    LogPrint(BCLog::DBLAYER, "-sync mempool end fail! \n");
    dbSrv.db_ops->rollback();
    return -1;
  }

  dbSrv.db_ops->commit();
  return 0; 
}

 
int dbDeleteAllUtx() {

  // delete all utx 
  return dbSrv.db_ops->delete_all_utx();

}

int dbFilterTx(uint256 &hash ) {

  if (!gArgs.GetArg("-savetodb", false))
      return -1;

  if (!gArgs.GetArg("-filtertx", true))
      return -1;
  return dbSrv.db_ops->query_filter_tx(hash.begin());

}
