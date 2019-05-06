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
#include <string.h>
#include <vector>
#include "logging.h"
#include "pool.h"

struct PoolAddr
{
    const char * addr;
    POOL_TYPE poolType;
};

struct PoolPrefix
{
    const char * prefix;
    POOL_TYPE poolType;
};

struct bipPrefix
{
    const char * prefix;
    BIP_TYPE bipType;
};
 

//TODO: hide unactive pool after import db

static struct PoolAddr poolAddresses[] = {
    {"1KFHE7w8BhaENAswwryaoccDb6qcT6DbYY", POOL_F2POOL},
    {"18cBEMRxXHqzWWCxZNtU91F5sbUNKhL5PX", POOL_VIABTC},
    {"1Hz96kJKF2HLPGY15JWLB5m9qGNxvt8tHJ", POOL_BTC_TOP},
    {"3NA8hsjfdgVkmmVS9moHmkZsVCoLxUkvvv", POOL_BTC_COM },
    {"1KsFhYKLs8qb1GHqrPxHoywNQpet2CtP9t", POOL_BIXIN},
    {"1JLRXD8rjRgQtTS9MvfQALfHgGWau9L9ky", POOL_BW_COM},
    {"1AcAj9p6zJn4xLXdvmdiuPCtY7YkBPTAJo", POOL_BITFURY},
    {"1AqTMY7kmHZxBuLUR5wJjPFUvqGs23sesr", POOL_SLUSH},
    {"1CjPR7Z5ZSyWk6WtXvSFgkptmpoi4UM9BC", POOL_GHASH_IO},
    {"1KuWLoZuoJgz3N6sLoAwGth9XGm8YuFTGt", POOL_GBMINERS},
    {"19qa95rTbDziNCS9EexUbh2hVY4viUU9tt", POOL_HAOZHUZHU},
    {"17kkmDx8eSwj2JTTULb3HkJhCmexfysExz", POOL_POLMINE},
    {"1AajKXkaq2DsnDmP8ZPTrE5gH1HFo1x3AU", POOL_POLMINE},
    {"16cv7wyeG6RRqhvJpY21CnsjxuKj2gAoK2", POOL_POLMINE},
    {"13vWXwzNF5Ef9SUXNTdr7de7MqiV4G1gnL", POOL_POLMINE},
    {"1Nsvmnv8VcTMD643xMYAo35Aco3XA5YPpe", POOL_POLMINE},
    {"1JrYhdhP2jCY6JwuVzdk9jUwc4pctcSes7", POOL_POLMINE},
    {"1N2H8sDjwK7xM1RDZ6o5SVUuoDsynCKfCM", POOL_POLMINE},
    {"1BX5YoLwvqzvVwSrdD4dC32vbouHQn2tuF", POOL_COINTERRA},
    {"19PkHafEN18mquJ9ChwZt5YEFoCdPP5vYB", POOL_BITMINTER},
    {"15xiShqUqerfjFdyfgBH1K7Gwp6cbYmsTW", POOL_ECLIPSEMC},
    {"1BwZeHJo7b7M2op7VDfYnsmcpXsUYEcVHm", POOL_BTC_NUGGETS},
    {"3HuobiNg2wHjdPU2mQczL9on8WF7hZmaGd", POOL_HUOBI},
    {"1ALA5v7h49QT7WYLcRsxcXqXUqEqaWmkvw", POOL_CLOUDHASHING},
    {"1K7znxRfkS8R1hcmyMvHDum1hAQreS4VQ4", POOL_MEGABIGPOWER},
    {"1HTejfsPZQGi3afCMEZTn2xdmoNzp13n3F", POOL_BITALO},
    {"18zRehBcA2YkYvsC7dfQiFJNyjmWvXsvon", POOL_BITSOLO},
    {"155fzsEBHy9Ri2bMQ8uuuR3tv1YzcDywd4", POOL_BITCLUB_NETWORK},
    {"14yfxkcpHnju97pecpM7fjuTkVdtbkcfE6", POOL_BITFURY},
    {"1FeDtFhARLxjKUPPkQqEBL78tisenc9znS", POOL_BITFURY},
    {"1A66YkobmtQvGbq9jt5faw6nCE8tgjGgKT", POOL_BITFURY},
    {"15rQXUSBQRubShPpiJfDLxmwS8ze2RUm4z", POOL_21_INC},
    {"1CdJi2xRTXJF6CEJqNHYyQDNEcM3X7fUhD", POOL_21_INC},
    {"1GC6HxDvnchDdb5cGkFXsJMZBFRsKAXfwi", POOL_21_INC},
    {"1MimPd6LrPKGftPRHWdfk8S3KYBfN4ELnD", POOL_DIGITALBTC},
    {"1NY15MK947MLzmPUa2gL7UgyR8prLh2xfu", POOL_DIGITALX_MINTSY},
    {"1P4B6rx1js8TaEDXvZvtrkiEb9XrJgMQ19", POOL_TELCO_214},
    {"1MoYfV4U61wqTPTHCyedzFmvf2o3uys2Ua", POOL_TELCO_214},
    {"1GaKSh2t396nfSg5Ku2J3Yn1vfVsXrGuH5", POOL_TELCO_214},
    {"1AsEJU4ht5wR7BzV6xsNQpwi5qRx4qH1ac", POOL_TELCO_214},
    {"18ikmzPqk721ZNvWhDos1UL4H29w352Kj5", POOL_TELCO_214},
    {"1DXRoTT67mCbhdHHL1it4J1xsSZHHnFxYR", POOL_TELCO_214},
    {"13Sd8Y7nUao3z4bJFkZvCRXpFqHvLy49YY", POOL_TELCO_214},
    {"18hvMLisvfc58PvA5rHH7NsLN9CV5ddB2x", POOL_TELCO_214},
    {"18EPLvrs2UE11kWBB3ABS7Crwj5tTBYPoa", POOL_GOGREENLIGHT},
    {"152f1muMCNa7goXYhYAQC61hxEgGacmncB", POOL_BTCC_POOL},
    {"1PmRrdp1YSkp1LxPyCfcmBHDEipG5X4eJB", POOL_BTC_POOL_PARTY},
    {"1Hk9gD8xMo2XBUhE73y5zXEM8xqgffTB5f", POOL_8BAOCHI},
    {"151T7r1MhizzJV6dskzzUkUdr7V8JxV2Dx", POOL_MYBTCCOIN_POOL},
    {"1NbycKAmieJkJnnAZEXbTdt4gRN7yHcSfp", POOL_MYBTCCOIN_POOL},
    {"149XQ5cUYnLbdDY2Fw1NqkVJ1YZxGWfNfR", POOL_MYBTCCOIN_POOL},
    {"1BUiW44WuJ2jiJgXiyxJVFMN8bc1GLdXRk", POOL_TBDICE},
    {"15MxzsutVroEE9XiDckLxUHTCDAEZgPZJi", POOL_BTCDIG},
    {"1MeffGLauEj2CZ18hRQqUauTXb9JAuLbGw", POOL_MULTIPOOL},
    {"1qtKetXKgqa7j1KrB19HbvfRiNUncmakk",  POOL_TRANSACTIONCOINMINING},
    {"17judvK4AC2M6KhaBbAEGw8CTKc9Pg8wup",  POOL_HOTPOOL },
    {"1MFsp2txCPwMMBJjNNeKaduGGs8Wi1Ce7X", POOL_A_XBT},
    {"1jKSjMLnDNup6NPgCjveeP9tUn4YpT94Y",  POOL_BTCMP },
    {"1GBo1f2tzVx5jScV9kJXPUP9RjvYXuNzV7", POOL_NEXIOUS},
    {"1AePMyovoijxvHuKhTqWvpaAkRCF4QswC6", POOL_TRICKYS},
    {"19f8Pk3m1GySyowWb8qLy93R5ngAUoVoUE", POOL_MANHATTANMINE},
    {"16GsNC3q6KgVXkUX7j7aPxSUdHrt1sN2yN", POOL_EOBOT},
    {"1MPxhNkSzeTNTHSZAibMaS8HS1esmUL1ne", POOL_EOBOT},
    {"1F1xcRt8H8Wa623KqmkEontwAAVqDSAWCV", POOL_1HASH},
    {"1BRY8AD7vSNUEE75NjzfgiG18mWjGQSRuJ", POOL_UNOMP},
    {"19RE4mz2UbDxDVougc6GGdoT4x5yXxwFq2", POOL_PATELSMINING},
    {"197miJmttpCt2ubVs6DDtGBYFDroxHmvVB", POOL_PATELSMINING},
    {"1JVHw9iyesn9CSTDgbmKgSyyZoH9CFUFaP", POOL_BIXIN},
    {"14KxdJ7DfAPsxCYkpcDTcKotjeProcfRPo", POOL_BRAVO},
    {"1AZ6BkCo4zgTuuLpRStJH8iNsehXTMp456", POOL_BITCOIN_INDIA},
    {"18HEMWFXM9UGPVZHUMdBPD3CMFWYn2NPRX", POOL_SHAWNP0WERS},
    {"12znnESiJ3bgCLftwwrg9wzQKN8fJtoBDa", POOL_SHAWNP0WERS},
    {"1E18BNyobcoiejcDYAz5SjbrzifNDEpM88", POOL_BCMONSTER},
    {"1JpKmtspBJQVXK67DJP64eBJcAPhDvJ9Er", POOL_RIGPOOL},
    {"1Afcpc2FpPnREU6i52K3cicmHdvYRAH9Wo", POOL_CANOE},
    {"167ApWWxUSFQmz2jdz9xop3oAKdLejvMML", POOL_BATPOOL},
    {"1KPQkehgYAqwiC6UCcbojM3mbGjURrQJF2", POOL_CONNECTBTC},
    {"1Cs5RT9SRk1hxsdzivAfkjesNmVVJqfqkw", POOL_EKANEMBTC},
    {"1JvxcA1QpaQ8KZfLHZKUrC3QTboxupRPEx", POOL_UNKNOWN},
    {"1KTNEBhQdj81UhDkJaoe3r8VtAsrnmpmnS", POOL_UNKNOWN},
    {"1Ar2gRkt1u6k4PToeeTKm1KGmtR2GRA1wL", POOL_UNKNOWN},
    {"18UBaMPq6FQfXnKT19rXc8bFFFwQz52Bc1", POOL_UNKNOWN},
    {"17JJ3oZyF4ShQMGukDjpMWhmooCjEvoVVB", POOL_UNKNOWN},
    {"19vvtxUpbidB8MT5CsSYYTBEjMRnowSZj4", POOL_UNKNOWN},
    {"18wac589ratFqqXWbJoGDRZT7Jxhaf7TFw", POOL_UNKNOWN},
    {"1LpfvAA66TojDZ7eTBpLmNz1JTvnWhLRkz", POOL_UNKNOWN},
    {"1M5QVLmg8E9tTKuuEKvd8RAKq5bpQEcJTx", POOL_UNKNOWN},
    {"18PRzhTnmviTqW7WjRf5UTSXWucEqzfQq9", POOL_UNKNOWN},
    {"1LfALjPR8tEEYniesbWu9zmQRsdqUDLpdM", POOL_UNKNOWN},
    {"1DrQ8mc9Fpp26Rq8VGD7zn3zrURn9DaucN", POOL_UNKNOWN},
    {"1FUKuGywGwfFPbi3ySnJzH75rku9sSCMFW", POOL_UNKNOWN},
    {"16FuTJ2dsSFuJ2yAtd6kVcGQTHjJEsKVFr", POOL_UNKNOWN},
    {"1KVcqebRwgwRK6PMCrn34KoSRbm7gfXv8B", POOL_UNKNOWN},
    {"1jKSjMLnDNup6NPgCjveeP9tUn4YpT94Y",  POOL_UNKNOWN},
    {"1JybTVMrT7XD3dXBHvTMZ1BAL3WjtnMMjh", POOL_UNKNOWN},
    {"1Cpri8p1NGcQmV53vz2twDEUk3RXfEtqX7", POOL_UNKNOWN},
    {"1818sPdLt2KDQEaoar68dA7yHD7tN3qXLj", POOL_UNKNOWN},
    {"1GcF7j3YH8Qs8hvNEe7zbrQZftMU6sRLfu", POOL_UNKNOWN},
    {"1Pm2sKzjjd5EP67ErNA1gffUUkTDzkMU2H", POOL_UNKNOWN},
    {"1Mv9oGhatduXxihUvjwxvNpm3ueKT6nJk6", POOL_UNKNOWN},
    {"1HDZTaqRXXscinhnRSBQtjZ9wFPpj6XkYW", POOL_UNKNOWN},
    {"18xf3MQvdVzKhyRvog8Q5P5THWvSjJUkzf", POOL_UNKNOWN},
    {"1L5dyJjaTRtHEFsuYZAeXskXozSvmqw1iA", POOL_UNKNOWN},
    {"1Fdpys8g2Dsnk2vyLARdDidAebCafrH2Wb", POOL_UNKNOWN},
    {"1M4HqWhUYYrANbWCn2TVb9t26KdoLahy3Q", POOL_UNKNOWN},
    {"14BMjogz69qe8hk9thyzbmR5pg34mVKB1e", POOL_UNKNOWN},
    {"1MKx9a4uqAPWe5XKJMGSxQhVEY8Q5EkV1Z", POOL_UNKNOWN},
    {"18Gfccb6sBFuTViuMc5pftxxdZ3q1nTC1Z", POOL_UNKNOWN},
    {"13yF6wf7Pg12QxsCUK5Bw4WTWvdufdn5pF", POOL_UNKNOWN},
    {"1AA3ng2qmAJ9EsVLbVoYqTa6JkCdn2PWwA", POOL_UNKNOWN},
    {"1Ef15oGgaaRnR92w4giaksFGJMbziEDT9W", POOL_UNKNOWN},
    {"1ESxhbSXJxKY9zKsQHignDsHZ6nk563yPi", POOL_UNKNOWN},
    {"1L4mFkDUsfYgp6Qz7RoVYEMnJQobFcNPMg", POOL_UNKNOWN}
};

static struct PoolPrefix poolPrefixes[] = {
    {"Mined by AntPool", POOL_ANTPOOL},
    {"/AntPool/", POOL_ANTPOOL},
    {"七彩神仙鱼", POOL_F2POOL},
    {"Made in China", POOL_F2POOL},
    {"Mined by user", POOL_F2POOL},
    {"For Pierce and Paul",POOL_F2POOL},
    {"mined by gbminers",POOL_GBMINERS},
    {"haozhuzhu",POOL_HAOZHUZHU},
    {"viabtc.com", POOL_VIABTC},
    {"/ViaBTC/", POOL_VIABTC},
    {"viabtc.com deploy", POOL_VIABTC},
    {"Mined By 175btc.com", POOL_175BTC},
    {"ASICMiner", POOL_ASICMINER},
    {"BitMinter", POOL_BITMINTER},
    {"btcserv", POOL_BTCSERV},
    {"HaoBTC", POOL_BIXIN},
    {"simplecoin", POOL_SIMPLECOIN_US},
    {"BTC Guild", POOL_BTC_GUILD},
    {"Slush", POOL_SLUSH},
    {"slush", POOL_SLUSH},
    {"Eligius", POOL_ELIGIUS},
    {"BitMinter", POOL_BITMINTER},
    {"ozco.in", POOL_OZCOIN},
    {"ozcoin", POOL_OZCOIN},
    {"EMC", POOL_ECLIPSEMC},
    {"MaxBTC", POOL_MAXBTC},
    {"triplemining", POOL_TRIPLEMINING},
    {"Triplemining.com", POOL_TRIPLEMINING},
    {"CoinLab", POOL_COINLAB},
    {"50BTC", POOL_50BTC},
    {"50btc", POOL_50BTC},
    {"ghash.io", POOL_GHASH_IO},
    {"GHash.io", POOL_GHASH_IO},
    {"st mining corp", POOL_ST_MINING_CORP},
    {"bitparking", POOL_BITPARKING},
    {"mmpool", POOL_MMPOOL},
    {"by polmine.pl", POOL_POLMINE},
    {"bypmnea", POOL_POLMINE},
    {"KnCMiner", POOL_KNCMINER},
    {"Bitalo", POOL_BITALO},
    {"HHTT", POOL_HHTT},
    {"megabigpower.com", POOL_MEGABIGPOWER},
    {"/mtred/", POOL_MT_RED},
    {"nmcbit.com", POOL_NMCBIT},
    {"yourbtc.net", POOL_YOURBTC_NET},
    {"Give-Me-Coins", POOL_GIVE_ME_COINS},
    {"GIVE-ME-COINS.com", POOL_GIVE_ME_COINS},
    {"Mined by MultiCoin.co", POOL_MULTICOIN_CO},
    {"bcpool.io", POOL_BCPOOL_IO},
    {"cointerra", POOL_COINTERRA},
    {"/Kano", POOL_KANO_CKPOOL},
    {"/solo.ckpool.org/", POOL_SOLO_CKPOOL},
    {"ckpool/mined by devastra/", POOL_CKPOOL_DEVASTRA},
    {"/NiceHashSolo", POOL_NICEHASH_SOLO},
    {"/BitClub Network/", POOL_BITCLUB_NETWORK},
    {"bitcoinaffiliatenetwork.com", POOL_BITCOIN_AFFILIATE_NETWORK},
    {"BitAffNet", POOL_BITCOIN_AFFILIATE_NETWORK},
    {"BTCChina Pool", POOL_BTCC_POOL},
    {"btcchina.com", POOL_BTCC_POOL},
    {"BTCChina.com", POOL_BTCC_POOL},
    {"BTCC", POOL_BTCC_POOL},
    {"BW Pool", POOL_BW_COM},
    {"Bitsolo Pool", POOL_BITSOLO},
    {"/BitFury/", POOL_BITFURY},
    {"/Bitfury/", POOL_BITFURY},
    {"/pool34/", POOL_21_INC},
    {"/agentD/", POOL_DIGITALBTC},
    {"/八宝池 8baochi.com/", POOL_8BAOCHI},
    {"myBTCcoin Pool", POOL_MYBTCCOIN_POOL},
    {"TBDice", POOL_TBDICE},
    {"nodeStratum", POOL_NODE_STRATUM_POOL},
    {"stratumPool", POOL_STRATUM_POOL},
    {"bypmne", POOL_POLMINE},
    {"2av0id51pct", POOL_2AV0ID51PCT},
    {"Josh Zerlan was here!", POOL_ECLIPSEMC},
    {"EclipseMC", POOL_ECLIPSEMC},
    {"Aluminum Falcon", POOL_ECLIPSEMC},
    {"wizkid057",POOL_WIZKID057},
    {"HASHPOOL",POOL_HASHPOOL},
    {"/A-XBT/",POOL_A_XBT},
    {"/Nexious/", POOL_NEXIOUS},
    {"Bravo Mining", POOL_BRAVO},
    {"p2pool", POOL_P2POOL},
    {"Mined by 1hash.com", POOL_1HASH},
    {"pool.bitcoin.com", POOL_BITCOIN_COM},
    {"/BCMonster/", POOL_BCMONSTER},
    {"/CANOE/", POOL_CANOE },
    {"/RigPool.com/", POOL_RIGPOOL },
    {"/BTC.COM/", POOL_BTC_COM },
    {"/ConnectBTC - Home for Miners/", POOL_CONNECTBTC},
    {"/BATPOOL/", POOL_BATPOOL },
    {"/BTC.TOP/", POOL_BTC_TOP},
    {"/phash.cn/", POOL_PHASH },
    {"/phash.io/", POOL_PHASH },
    {"--Nug--", POOL_SHAWNP0WERS},
    {"/Bitcoin-India/", POOL_BITCOIN_INDIA}
};

static struct bipPrefix bipPrefixes[] = {
    {"EB1/AD6", BIP_BU},
    {"BIP148", BIP_148},
    {"8M", BIP_8M},
    {"/BIP100/", BIP_100},
    {"/BIP248/", BIP_248}
};
 
const unsigned char * strMatch (const char* str, const unsigned char * mem, int memlen)
{
    int len = strlen(str);
    int i, offset = 0 ;
    
    for (offset=0; (memlen - offset+1) >= len; offset++ ) {
            for(i = 0; i < len; i++) {
                if (((const unsigned char *)str)[i] != mem[i+offset]) 
                   break;
            }
            if (i==len) return mem+offset;
        }
    return 0;
}

int getPoolIdByPrefix(const unsigned char *coinbase, int coinbaseLen)
{
    for (size_t i = 0; i < (sizeof(poolPrefixes) / sizeof(poolPrefixes[0])); ++i) {
        if (strMatch(poolPrefixes[i].prefix, coinbase, coinbaseLen) != 0) {
            //LogPrint("dblayer", "mined by pool %s\n", poolPrefixes[i].prefix);
            return  poolPrefixes[i].poolType;
        }
    }

    return 0;
}

int getPoolIdByAddr(const char *addr)
{
    for (size_t i = 0; i < (sizeof(poolAddresses) / sizeof(poolAddresses[0])); ++i) {
    if (memcmp(poolAddresses[i].addr, addr, strlen(poolAddresses[i].addr)) == 0) {
       // LogPrint("dblayer", "mined by addr %s\n", poolAddresses[i].addr);
        return  poolAddresses[i].poolType;
        }
    }

    return 0;
}

int getPoolSupportBip(const unsigned char *coinbase, int coinbaseLen, int version)
{
    int ver = BIP_UNKNOWN;
    if (version == 0x20000000)
        ver |= BIP_9;
    if (version == 0x20000001)
        ver |= BIP_CSV;
    if (version == 0x20000002)
        ver |= BIP_SW;
    if (version == 0x20000004)
        ver |= BIP_101_8M;
    if (version == 0x20000008)
        ver |= BIP_101_2M;
    if ((version & 0x30000000) == 0x30000000)
        ver |= BIP_CLASSIC;

    for (size_t i = 0; i < (sizeof(bipPrefixes) / sizeof(bipPrefixes[0])); ++i) {
        if (strMatch(bipPrefixes[i].prefix, coinbase, coinbaseLen) != 0) {
            //LogPrint("dblayer", "mined by pool %s\n", poolPrefixes[i].prefix);
            ver |= bipPrefixes[i].bipType;
        }
    }

    return  ver;
}
