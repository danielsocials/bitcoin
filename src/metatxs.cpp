#include "core_io.h"
#include "util/strencodings.h"
#include "key.h"
#include "validation.h"
#include "rpc/server.h"
#include "script/script.h"
#include "logging.h"
#include "script/standard.h"

#include <stdint.h>
#include <string.h>
#include <vector>

#include "base58.h"
#include <openssl/rc4.h>
#include <boost/foreach.hpp>

using std::string; 

enum metatype
{
    META_OTHER = 0,
    META_SATOSHI_DICE = 1,
    META_BETCOIN_DICE = 2,
    META_SATOSHI_BONES = 3,
    META_CHBS = 4,
    META_TRADLE = 5,
    META_SMARTASS = 6,
    META_LUCKY_BIT = 7,
    META_COUNTERPARTY = 8,
    META_MASTERCOIN = 9,
    META_CHANCECOIN = 10,
    META_DOCPROOF = 11,
    META_OPEN_ASSETS = 12,
    META_BLOCK_SIGN = 13,
    META_COIN_SPARK = 14,
    META_CRYPTO_TESTS = 15,
    META_BLOCKSTACK = 16,
    META_OMNI= 17,
    META_MONEGRAPH =18,
    META_COLU = 19,
    META_ASC = 20,
    META_BITPROOF = 20,
    META_ETERNITYWALL= 21,
    META_STAMPERY = 22, // https://stampery.co/ 
    META_FACTOM = 23,
    META_ASCRIBEPOOL = 24,
    META_OPENCHAIN = 25, //https://docs.openchain.org/en/latest/general/anchoring.html#blockchain-anchor-format
    META_UN = 26, //http://digitalcurrency.unic.ac.cy/free-introductory-mooc/academic-certificates-on-the-blockchain/
    META_SMARTBIT= 27, //https://www.smartbit.com.au/op-returns/smartbit-data
    META_CRYPTO_COPYRIGHT = 28, //crypto-copyright.com
    //unknow
    //META_SB =25,
    //META_5331D4 = 26,
    //META_171396 = 27,
};

static const char* GetMetaType(metatype t)
{
    switch (t)
    {
        case META_SATOSHI_DICE: return "SatoshiDice";
        case META_BETCOIN_DICE: return "BetCoinDice";
        case META_SATOSHI_BONES: return "SatoshiBones";
        case META_LUCKY_BIT: return "LuckyBit";
        case META_COUNTERPARTY: return "Counterparty";
        case META_MASTERCOIN: return "Mastercoin";
        case META_CHANCECOIN: return "Chancecoin";
        case META_OPEN_ASSETS: return "OpenAssets";
        case META_BLOCK_SIGN: return "BlockSign";
        case META_COIN_SPARK: return "CoinSpark";
        case META_CRYPTO_TESTS: return "CryptoTests";

        case META_BITPROOF: return "Bitproof";
        case META_DOCPROOF: return "Docproof";
        case META_FACTOM: return "Factom";
        case META_COLU: return "Colu";
        case META_ASCRIBEPOOL: return "AscribePool";
        case META_ETERNITYWALL: return "EternityWall";
        case META_MONEGRAPH: return "Monegraph";
        case META_BLOCKSTACK: return "Blockstack";
        case META_STAMPERY: return "Stampery";
        case META_OMNI : return "omni";
        case META_CHBS : return "chbs";
        case META_TRADLE: return "tradle";
        case META_SMARTASS : return "smartass";

        case META_OTHER: return "Other";
    }

    return NULL;
};

static int MetaTypeToInt(metatype t)
{
    return static_cast<int>(t);
};

static metatype IntToMetaType(int i)
{
    return static_cast<metatype>(i);
};

struct MetaPrefixRange {
    uint32_t begin;
    uint32_t end;
    metatype name;
};

struct MetaPrefix {
    std::vector<unsigned char> prefix;
    metatype name;
};

static struct MetaPrefixRange PubKeyHashPrefixes[] = {
    {0x06f1b600, 0x06f1b6ff, META_SATOSHI_DICE},              // SatoshiDice
    {0x74db3700, 0x74db59ff, META_BETCOIN_DICE},              // BetCoin Dice
    {0x069532d8, 0x069532da, META_SATOSHI_BONES},             // SatoshiBone
    {0x06c06f,   0x06c06f,   META_SATOSHI_BONES},             // SatoshiBone
    {0xda5dde84, 0xda5dde94, META_LUCKY_BIT},                 // Lucky Bit
    {0x434e5452, 0x434e5452, META_COUNTERPARTY},              // Counterparty
    {0x946cb2e0, 0x946cb2e0, META_MASTERCOIN},                // Mastercoin, 1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P
    {0x643ce12b, 0x643ce12b, META_MASTERCOIN},              // Mastercoin, mpexoDuSkGGqvqrkrjiFng38QPkJQVFyqv
    {0xc4c5d791, 0xc4c5d791, META_CHBS},                    // CHBS, 1JwSSubhmg6iPtRjtyqhUYYH7bZg3Lfy1T
};

static struct MetaPrefix MultisigPrefixes[] = {
    {ParseHex("434e545250525459"), META_COUNTERPARTY},        // "CNTRPRTY", Counterparty
    {ParseHex("4348414e4345434f"), META_CHANCECOIN},          // "CHANCECO", Chancecoin    
    //{ParseHex("747261646c65"),     META_TRADLE}             // "tradle", https://github.com/urbien/tradle/blob/master/README.md
};

static struct MetaPrefix OpReturnPrefixes[] = {    
    {ParseHex("434e545250525459"), META_COUNTERPARTY},        // "CNTRPRTY", Counterparty
    {ParseHex("444f4350524f4f46"), META_DOCPROOF },  // "DOCPROOF", Proof of Existence
    {ParseHex("4f4101"),           META_OPEN_ASSETS},         // "OA", Open Assets    
    {ParseHex("4253"),             META_BLOCK_SIGN},          // "BS", BlockSign    
    {ParseHex("53504b74"),         META_COIN_SPARK},          // "SPKt", CoinSpark
    {ParseHex("43727970746f5465737473"), META_CRYPTO_TESTS},  // "CryptoTests", http://crypto-copyright.com/   
    {ParseHex("42495450524f4f46"), META_BITPROOF},          // "BITPROOF", bitproof.io
    {ParseHex("747261646c65"),     META_TRADLE},            // "tradle", https://github.com/urbien/tradle/blob/master/README.md
    {ParseHex("534d415254415353"), META_SMARTASS},          // "SMARTASS"
    {ParseHex("4661"),     META_FACTOM},            // "Factom"
    //{ParseHex("00756e7375636365"), META_UNSUCCESSFUL_DOUBLE_SPENT},  // " unsuccessful double-spend attempt ", Peter Todd's unsuccessful double spent
    {ParseHex("6964"), META_BLOCKSTACK},  // id+ -->id;
    {ParseHex("4d47"), META_MONEGRAPH},  // 
    {ParseHex("434301"), META_COLU},  // colu
    {ParseHex("435343"), META_ASCRIBEPOOL},  // 
    {ParseHex("4557"), META_ETERNITYWALL},  // 
    {ParseHex("6f6d6169"), META_OMNI},  // 
    {ParseHex("6f6d6e69"), META_OMNI},  // 
    {ParseHex("533400"), META_STAMPERY},  // 
    {ParseHex("533500"), META_STAMPERY},  // 
    {ParseHex("4f43"), META_OPENCHAIN},  // 
    {ParseHex("554e6963444320"), META_UN},  // 
    {ParseHex("53422e44"), META_SMARTBIT},  // 
    {ParseHex("6a2843727970746f50726f6f662d"), META_CRYPTO_COPYRIGHT},  // 
    //{ParseHex("5342"), META_SB},  // 
    //{ParseHex("5331d4"), META_5331D4},  // 
    //{ParseHex("171396"), META_171396},  // 
 
    //{ParseHex("4d47ff"), META_UNSUCCESSFUL_DOUBLE_SPENT},  // " unsuccessful double-spend attempt ", Peter Todd's unsuccessful double spent
    
};

// OP_DUP OP_HASH160 [payload] OP_EQUALVERIFY OP_CHECKSIG
static bool IsLoadedPubKeyHash(const CScript& scriptPubKey, txnouttype& typeRet, metatype& metaRet)
{
    if (scriptPubKey.size() < 7 || scriptPubKey[0] != OP_DUP) {
        return false;
    }

    uint32_t pfx = ntohl(*(uint32_t*)&scriptPubKey[3]);
    
    for (size_t i = 0; i < (sizeof(PubKeyHashPrefixes) / sizeof(PubKeyHashPrefixes[0])); ++i)
    {
        if (pfx >= PubKeyHashPrefixes[i].begin && pfx <= PubKeyHashPrefixes[i].end)
        {
            typeRet = TX_PUBKEYHASH;
            metaRet = PubKeyHashPrefixes[i].name;
            return true;
        }
    }

    return false;
};

// OP_n [payload] ([payload]) ([payload]) OP_m OP_CHECKMULTISIG
static bool IsLoadedMultisig(const CScript& scriptPubKey, txnouttype& typeRet, metatype& metaRet)
{
    if (scriptPubKey.size() < 36 || scriptPubKey.back() != OP_CHECKMULTISIG) {
        return false;
    }
    
    // Find position of data package
    // Assume first key is used for redemption, second carries data
    size_t offset = 0;
    
    // Compressed or uncompressed first public key?
    if (0x21 == scriptPubKey[1] && 0x21 == scriptPubKey[35]) {
        offset = 36;
    }
    else
    if (scriptPubKey.size() >= 68 && 0x41 == scriptPubKey[1] && 0x21 == scriptPubKey[67]) {
        offset = 68;
    }

    for (size_t i = 0; i < (sizeof(MultisigPrefixes) / sizeof(MultisigPrefixes[0])); ++i)
    {
        // The data package might be prefixed
        if ((0 == memcmp(&(MultisigPrefixes[i].prefix)[0],
                &scriptPubKey[offset+1], MultisigPrefixes[i].prefix.size())) ||
            (0 == memcmp(&(MultisigPrefixes[i].prefix)[0],
                &scriptPubKey[offset], MultisigPrefixes[i].prefix.size())))
        {
            typeRet = TX_MULTISIG;
            metaRet = MultisigPrefixes[i].name;
            return true;
        }
    }

    return false;
};

string decode_rc4(const unsigned char *txhash, const unsigned char *data, int len) {
    RC4_KEY key;

    unsigned char *obuf = (unsigned char*)malloc(len+1);
    memset(obuf, 0, len+1);

    RC4_set_key(&key, 32, (const unsigned char*)txhash);
    RC4(&key, len, (const unsigned char*)data, obuf);

    string decode_data((char*)obuf, len);
    free(obuf);

    return decode_data;
}

// OP_RETURN [payload]
static bool IsLoadedOpReturn(const CScript& scriptPubKey, txnouttype& typeRet, metatype& metaRet)
{
    if (scriptPubKey.size() < 3 || scriptPubKey[0] != OP_RETURN || (scriptPubKey.size()>MAX_OP_RETURN_RELAY)) {
        return false;
    }
    
    for (size_t i = 0; i < (sizeof(OpReturnPrefixes) / sizeof(OpReturnPrefixes[0])); ++i)
    {
        if (0 == memcmp(&(OpReturnPrefixes[i].prefix)[0],
                &scriptPubKey[2], OpReturnPrefixes[i].prefix.size()))
        {
            typeRet = TX_NULL_DATA;
            metaRet = OpReturnPrefixes[i].name;            
            return true;
        }
    }
    
    return false;
};

static bool IsLoadedScriptSigOfP2PKH(const CScript& scriptSig, txnouttype& typeRet, metatype& metaRet)
{
    opcodetype opcode;
    std::vector<unsigned char> vch;
    CScript::const_iterator pc = scriptSig.begin();
    
    // Skip first
    if (!scriptSig.GetOp(pc, opcode, vch)) {
        return false;
    }
    
    // Get second
    if (!scriptSig.GetOp(pc, opcode, vch)) {
        return false;
    }
    
    // Is this a public key?
    if (!((vch.size() == 33 && (vch[0] == 0x02 || vch[0] == 0x03))
            || (vch.size() == 65 && vch[0] == 0x04)))
    {
        return false;
    }

    CKeyID id = CPubKey(vch).GetID();
    uint32_t pfx = ntohl(*(uint32_t*)&id);    

    for (size_t i = 0; i < (sizeof(PubKeyHashPrefixes) / sizeof(PubKeyHashPrefixes[0])); ++i)
    {
        if (pfx >= PubKeyHashPrefixes[i].begin && pfx <= PubKeyHashPrefixes[i].end)
        {
            typeRet = TX_PUBKEYHASH;
            metaRet = PubKeyHashPrefixes[i].name;
            return true;
        }
    }

    return false;
};

static bool IsLoadedScriptPubKey(const CScript& scriptPubKey, txnouttype& typeRet, metatype& metaRet)
{
    return  IsLoadedOpReturn(scriptPubKey, typeRet, metaRet) ||
            IsLoadedMultisig(scriptPubKey, typeRet, metaRet) ||
            IsLoadedPubKeyHash(scriptPubKey, typeRet, metaRet);
};

static inline void bswap_256(const unsigned char *src, unsigned char *dst)
{
    const unsigned char *s = src;
    unsigned char *d = dst;
    int i;

    for (i = 0; i < 32; i++)
        d[31 - i] = s[i];
}

static bool CheckTxForPayload(const CTransaction& tx, txnouttype& typeRet, metatype& metaRet)
{
    // Crawl outputs
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        if (IsLoadedScriptPubKey(txout.scriptPubKey, typeRet, metaRet)) {
            return true;
        }
        else if ((txout.scriptPubKey[0] == OP_RETURN) && (txout.scriptPubKey.size()>5) && (txout.scriptPubKey.size()<=MAX_OP_RETURN_RELAY)) { 

            uint256  hash = tx.vin[0].prevout.hash;
            unsigned char key[32];
            bswap_256(hash.begin(), key);
            string str = decode_rc4(key, &txout.scriptPubKey[2], txout.scriptPubKey.size()-2);
            std::vector<unsigned char> cnt = ParseHex("434e545250525459");  // "CNTRPRTY", Counterparty 
            if (0 == memcmp(cnt.data(), str.c_str(), cnt.size())) {
                metaRet = META_COUNTERPARTY;
                return true;
            }
        }
    }

    // Crawl inputs and check, if it's a spent from a listed entry
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        if (IsLoadedScriptSigOfP2PKH(txin.scriptSig, typeRet, metaRet)) {
            return true;
        }
    }

    return false;
};


int getTxMetaType(const CTransaction &tx)
{
    txnouttype type;
    metatype meta = META_OTHER;

    CheckTxForPayload(tx, type, meta);

    int pos = MetaTypeToInt(meta);
    return pos;
}        
