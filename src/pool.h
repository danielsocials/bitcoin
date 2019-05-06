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

enum POOL_TYPE {
    POOL_UNKNOWN = 0,
    POOL_MEGABIGPOWER = 1,
    POOL_BTCC_POOL = 2,
    POOL_HUOBI = 3,
    POOL_POLMINE = 4,
    POOL_8BAOCHI = 11,
    POOL_F2POOL = 12,
    POOL_TELCO_214 = 13,
    POOL_DIGITALX_MINTSY = 14,
    POOL_BITSOLO = 15,
    POOL_COINTERRA = 16,
    POOL_ECLIPSEMC = 17,
    POOL_21_INC = 18,
    POOL_BTC_NUGGETS = 21,
    POOL_MYBTCCOIN_POOL = 22,
    POOL_BTC_POOL_PARTY = 23,
    POOL_GHASH_IO = 25,
    POOL_CLOUDHASHING = 26,
    POOL_BW_COM = 27,
    POOL_DIGITALBTC = 28,
    POOL_BITMINTER = 29,
    POOL_BITFURY = 31,
    POOL_BITCLUB_NETWORK = 35,
    POOL_BITALO = 36,
    POOL_TBDICE = 37,
    POOL_SLUSH = 38,
    POOL_ANTPOOL = 47,
    POOL_MMPOOL = 49,
    POOL_OZCOIN = 50,
    POOL_BITCOIN_AFFILIATE_NETWORK = 54,
    POOL_TRIPLEMINING = 57,
    POOL_GIVE_ME_COINS = 58,
    POOL_BTC_GUILD = 59,
    POOL_SOLO_CKPOOL = 60,
    POOL_MT_RED = 64,
    POOL_NMCBIT = 65,
    POOL_175BTC = 67,
    POOL_ASICMINER = 69,
    POOL_SIMPLECOIN_US = 70,
    POOL_50BTC = 71,
    POOL_BITPARKING = 72,
    POOL_ST_MINING_CORP = 74,
    POOL_BCPOOL_IO = 78,
    POOL_KANO_CKPOOL = 79,
    POOL_ELIGIUS = 81,
    POOL_MULTICOIN_CO = 84,
    POOL_BTCSERV = 86,
    POOL_NICEHASH_SOLO = 87,
    POOL_MAXBTC = 88,
    POOL_COINLAB = 91,
    POOL_YOURBTC_NET = 93,
    POOL_KNCMINER = 94,
    POOL_HHTT = 95,
    POOL_NODE_STRATUM_POOL = 96,
    POOL_STRATUM_POOL = 97,
    POOL_P2POOL = 98,
    POOL_CKPOOL_DEVASTRA = 99,
    POOL_MULTIPOOL = 100, //https://www.multipool.us/
    POOL_MYBTCCOIN = 101, //http://mybtccoin.com
    POOL_BTCDIG= 102,  //https://btcdig.com/stats/blocks/
    POOL_WIZKID057 = 103,
    POOL_2AV0ID51PCT=104,
    POOL_HASHPOOL=105,  //"http://hashpool.com/"
    POOL_TRANSACTIONCOINMINING = 106, //"http://sha256.transactioncoinmining.com/"
    POOL_A_XBT = 107, //"http://www.a-xbt.com/"
    POOL_NEXIOUS = 108, //https://nexious.com/
    POOL_BRAVO =109,    //http://www.bravo-mining.com/
    POOL_TRICKYS =110,  //http://pool.wemine.uk/
    POOL_HOTPOOL =111,  //http://hotpool.co/
    POOL_BTCMP = 112,   //https://www.btcmp.com/
    POOL_BCMONSTER = 113, //http://www.bcmonster.com/
    POOL_MANHATTANMINE = 114, //http://ham.manhattanmine.com
    POOL_EOBOT= 115, //http://eobot.com
    POOL_1HASH = 116,   //https://www.1hash.com/
    POOL_UNOMP = 117,   //http://199.115.116.7:8925/
    POOL_PATELSMINING= 118,   //http://patelsminingpool.com/
    POOL_BIXIN = 119,   //https://bixin.com/common/pool_landing
    POOL_BTC_COM = 120,   //https://pool.btc.com/
    POOL_VIABTC= 121,   //http://6Yviabtc.com/
    POOL_BITCOIN_INDIA= 122,   //http://bitcoin-india.org/
    POOL_SHAWNP0WERS = 123,   //http://www.brainofshawn.com/
    POOL_BTC_TOP = 124,   //http://www.btc.top/
    POOL_CONNECTBTC = 125,   //http://www.ConnectBTC.com/
    POOL_BATPOOL = 126,   //http://www.BATPOOL.com/
    POOL_CANOE = 127,   //http://www.canoepool.com/
    POOL_RIGPOOL = 128,   //http://www.rigpool.com/
    POOL_HAOZHUZHU = 129,   //
    POOL_PHASH = 130,   //https://phash.io/
    POOL_BITCOIN_COM = 131,   //http://www.bitcoin.com/
    POOL_EKANEMBTC = 132,   //https://ekanembtc.com/
    POOL_GOGREENLIGHT = 133,   //http://www.gogreenlight.se
    POOL_GBMINERS = 134,   //http://gbminers.com/
};
 
enum BIP_TYPE {
    BIP_UNKNOWN=0,
    BIP_DEFAULT=1,
    BIP_8M=2,
    BIP_100=4,
    BIP_101_8M=6,
    BIP_101_2M=8,
    BIP_248=0x10,
    BIP_CLASSIC=0x20,
    BIP_CSV=0x40,
    BIP_9=0x80,
    BIP_SW=0x100,
    BIP_BU=0x200,
    BIP_UASF=0x400,
    BIP_148=0x800, //UASF
};
 
int getPoolIdByPrefix(const unsigned char *coinbase, int coinbaseLen);
int getPoolIdByAddr(const char *addr);
int getPoolSupportBip(const unsigned char *coinbase, int coinbaseLen, int version); 
