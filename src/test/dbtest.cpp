// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "data/tx_invalid.json.h"
#include "data/tx_valid.json.h"
#include "test/test_bitcoin.h"

#include "clientversion.h"
#include "checkqueue.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "key.h"
#include "keystore.h"
#include "policy/policy.h"
#include "script/script.h"
#include "script/sign.h"
#include "script/script_error.h"
#include "script/standard.h"
#include "utilstrencodings.h"
#include "chainparams.h"
#include "dblayer.h"
#include "pool.h"

#include <map>
#include <string>

#include "validation.h"
#include "miner.h"
#include "pubkey.h"
#include "txmempool.h"
#include "random.h"
#include "test/test_bitcoin.h"
#include "utiltime.h"
 

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/foreach.hpp>

#include <univalue.h>

using namespace std;

typedef vector<unsigned char> valtype;

// In script_tests.cpp
extern UniValue read_json(const std::string& jsondata);

BOOST_FIXTURE_TEST_SUITE(db_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(test_save_blk)
{
  int i = 0;
  // Random real block (0000000000013b8ab2cd513b0261a14096412195a72a0c4827d229dcc7e0f7af)
  // With 9 txes
  CBlock block;
  CDataStream stream(ParseHex("0100000090f0a9f110702f808219ebea1173056042a714bad51b916cb6800000000000005275289558f51c9966699404ae2294730c3c9f9bda53523ce50e9b95e558da2fdb261b4d4c86041b1ab1bf930901000000010000000000000000000000000000000000000000000000000000000000000000ffffffff07044c86041b0146ffffffff0100f2052a01000000434104e18f7afbe4721580e81e8414fc8c24d7cfacf254bb5c7b949450c3e997c2dc1242487a8169507b631eb3771f2b425483fb13102c4eb5d858eef260fe70fbfae0ac00000000010000000196608ccbafa16abada902780da4dc35dafd7af05fa0da08cf833575f8cf9e836000000004a493046022100dab24889213caf43ae6adc41cf1c9396c08240c199f5225acf45416330fd7dbd022100fe37900e0644bf574493a07fc5edba06dbc07c311b947520c2d514bc5725dcb401ffffffff0100f2052a010000001976a914f15d1921f52e4007b146dfa60f369ed2fc393ce288ac000000000100000001fb766c1288458c2bafcfec81e48b24d98ec706de6b8af7c4e3c29419bfacb56d000000008c493046022100f268ba165ce0ad2e6d93f089cfcd3785de5c963bb5ea6b8c1b23f1ce3e517b9f022100da7c0f21adc6c401887f2bfd1922f11d76159cbc597fbd756a23dcbb00f4d7290141042b4e8625a96127826915a5b109852636ad0da753c9e1d5606a50480cd0c40f1f8b8d898235e571fe9357d9ec842bc4bba1827daaf4de06d71844d0057707966affffffff0280969800000000001976a9146963907531db72d0ed1a0cfb471ccb63923446f388ac80d6e34c000000001976a914f0688ba1c0d1ce182c7af6741e02658c7d4dfcd388ac000000000100000002c40297f730dd7b5a99567eb8d27b78758f607507c52292d02d4031895b52f2ff010000008b483045022100f7edfd4b0aac404e5bab4fd3889e0c6c41aa8d0e6fa122316f68eddd0a65013902205b09cc8b2d56e1cd1f7f2fafd60a129ed94504c4ac7bdc67b56fe67512658b3e014104732012cb962afa90d31b25d8fb0e32c94e513ab7a17805c14ca4c3423e18b4fb5d0e676841733cb83abaf975845c9f6f2a8097b7d04f4908b18368d6fc2d68ecffffffffca5065ff9617cbcba45eb23726df6498a9b9cafed4f54cbab9d227b0035ddefb000000008a473044022068010362a13c7f9919fa832b2dee4e788f61f6f5d344a7c2a0da6ae740605658022006d1af525b9a14a35c003b78b72bd59738cd676f845d1ff3fc25049e01003614014104732012cb962afa90d31b25d8fb0e32c94e513ab7a17805c14ca4c3423e18b4fb5d0e676841733cb83abaf975845c9f6f2a8097b7d04f4908b18368d6fc2d68ecffffffff01001ec4110200000043410469ab4181eceb28985b9b4e895c13fa5e68d85761b7eee311db5addef76fa8621865134a221bd01f28ec9999ee3e021e60766e9d1f3458c115fb28650605f11c9ac000000000100000001cdaf2f758e91c514655e2dc50633d1e4c84989f8aa90a0dbc883f0d23ed5c2fa010000008b48304502207ab51be6f12a1962ba0aaaf24a20e0b69b27a94fac5adf45aa7d2d18ffd9236102210086ae728b370e5329eead9accd880d0cb070aea0c96255fae6c4f1ddcce1fd56e014104462e76fd4067b3a0aa42070082dcb0bf2f388b6495cf33d789904f07d0f55c40fbd4b82963c69b3dc31895d0c772c812b1d5fbcade15312ef1c0e8ebbb12dcd4ffffffff02404b4c00000000001976a9142b6ba7c9d796b75eef7942fc9288edd37c32f5c388ac002d3101000000001976a9141befba0cdc1ad56529371864d9f6cb042faa06b588ac000000000100000001b4a47603e71b61bc3326efd90111bf02d2f549b067f4c4a8fa183b57a0f800cb010000008a4730440220177c37f9a505c3f1a1f0ce2da777c339bd8339ffa02c7cb41f0a5804f473c9230220585b25a2ee80eb59292e52b987dad92acb0c64eced92ed9ee105ad153cdb12d001410443bd44f683467e549dae7d20d1d79cbdb6df985c6e9c029c8d0c6cb46cc1a4d3cf7923c5021b27f7a0b562ada113bc85d5fda5a1b41e87fe6e8802817cf69996ffffffff0280651406000000001976a9145505614859643ab7b547cd7f1f5e7e2a12322d3788ac00aa0271000000001976a914ea4720a7a52fc166c55ff2298e07baf70ae67e1b88ac00000000010000000586c62cd602d219bb60edb14a3e204de0705176f9022fe49a538054fb14abb49e010000008c493046022100f2bc2aba2534becbdf062eb993853a42bbbc282083d0daf9b4b585bd401aa8c9022100b1d7fd7ee0b95600db8535bbf331b19eed8d961f7a8e54159c53675d5f69df8c014104462e76fd4067b3a0aa42070082dcb0bf2f388b6495cf33d789904f07d0f55c40fbd4b82963c69b3dc31895d0c772c812b1d5fbcade15312ef1c0e8ebbb12dcd4ffffffff03ad0e58ccdac3df9dc28a218bcf6f1997b0a93306faaa4b3a28ae83447b2179010000008b483045022100be12b2937179da88599e27bb31c3525097a07cdb52422d165b3ca2f2020ffcf702200971b51f853a53d644ebae9ec8f3512e442b1bcb6c315a5b491d119d10624c83014104462e76fd4067b3a0aa42070082dcb0bf2f388b6495cf33d789904f07d0f55c40fbd4b82963c69b3dc31895d0c772c812b1d5fbcade15312ef1c0e8ebbb12dcd4ffffffff2acfcab629bbc8685792603762c921580030ba144af553d271716a95089e107b010000008b483045022100fa579a840ac258871365dd48cd7552f96c8eea69bd00d84f05b283a0dab311e102207e3c0ee9234814cfbb1b659b83671618f45abc1326b9edcc77d552a4f2a805c0014104462e76fd4067b3a0aa42070082dcb0bf2f388b6495cf33d789904f07d0f55c40fbd4b82963c69b3dc31895d0c772c812b1d5fbcade15312ef1c0e8ebbb12dcd4ffffffffdcdc6023bbc9944a658ddc588e61eacb737ddf0a3cd24f113b5a8634c517fcd2000000008b4830450221008d6df731df5d32267954bd7d2dda2302b74c6c2a6aa5c0ca64ecbabc1af03c75022010e55c571d65da7701ae2da1956c442df81bbf076cdbac25133f99d98a9ed34c014104462e76fd4067b3a0aa42070082dcb0bf2f388b6495cf33d789904f07d0f55c40fbd4b82963c69b3dc31895d0c772c812b1d5fbcade15312ef1c0e8ebbb12dcd4ffffffffe15557cd5ce258f479dfd6dc6514edf6d7ed5b21fcfa4a038fd69f06b83ac76e010000008b483045022023b3e0ab071eb11de2eb1cc3a67261b866f86bf6867d4558165f7c8c8aca2d86022100dc6e1f53a91de3efe8f63512850811f26284b62f850c70ca73ed5de8771fb451014104462e76fd4067b3a0aa42070082dcb0bf2f388b6495cf33d789904f07d0f55c40fbd4b82963c69b3dc31895d0c772c812b1d5fbcade15312ef1c0e8ebbb12dcd4ffffffff01404b4c00000000001976a9142b6ba7c9d796b75eef7942fc9288edd37c32f5c388ac00000000010000000166d7577163c932b4f9690ca6a80b6e4eb001f0a2fa9023df5595602aae96ed8d000000008a4730440220262b42546302dfb654a229cefc86432b89628ff259dc87edd1154535b16a67e102207b4634c020a97c3e7bbd0d4d19da6aa2269ad9dded4026e896b213d73ca4b63f014104979b82d02226b3a4597523845754d44f13639e3bf2df5e82c6aab2bdc79687368b01b1ab8b19875ae3c90d661a3d0a33161dab29934edeb36aa01976be3baf8affffffff02404b4c00000000001976a9144854e695a02af0aeacb823ccbc272134561e0a1688ac40420f00000000001976a914abee93376d6b37b5c2940655a6fcaf1c8e74237988ac0000000001000000014e3f8ef2e91349a9059cb4f01e54ab2597c1387161d3da89919f7ea6acdbb371010000008c49304602210081f3183471a5ca22307c0800226f3ef9c353069e0773ac76bb580654d56aa523022100d4c56465bdc069060846f4fbf2f6b20520b2a80b08b168b31e66ddb9c694e240014104976c79848e18251612f8940875b2b08d06e6dc73b9840e8860c066b7e87432c477e9a59a453e71e6d76d5fe34058b800a098fc1740ce3012e8fc8a00c96af966ffffffff02c0e1e400000000001976a9144134e75a6fcb6042034aab5e18570cf1f844f54788ac404b4c00000000001976a9142b6ba7c9d796b75eef7942fc9288edd37c32f5c388ac00000000"), SER_NETWORK, PROTOCOL_VERSION);
  //  stream >> block;
 

  FILE* file = fopen("data/380943.hex", "r");
  if (file != NULL) {
      fseek(file , 0, SEEK_END);
	  size_t size = ftell(file);
      string mystring( size, '\0' );
      fseek(file , 0, SEEK_SET);
      fread(&mystring[0], sizeof(char), (size_t)size, file);
      CDataStream stream1(ParseHex(mystring.c_str()), SER_NETWORK, PROTOCOL_VERSION);
      stream1 >> block;
      fclose(file );
  }
  else 
     return;
  //CDataStream stream1(ParseHex(fread(file), SER_NETWORK, PROTOCOL_VERSION);
  //fclose(file); 
  //stream1 >> block;

  std::string strHash = ("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38");
  uint256 hash(uint256S(strHash));

  CBlockIndex prev;
  CBlockIndex blockindex;

  prev.phashBlock = &hash;
  blockindex.pprev = &prev;
  blockindex.nHeight = 11;

  for (; i < 1; i++) {

      int64_t nStart = GetTimeMicros();
      if (dbSaveBlock(&blockindex, block) == -1) {
        BOOST_ERROR("Bad test: " << strHash);
      }
      printf("\n- Save block to db: %.2fms height %d\n",
               (GetTimeMicros() - nStart) * 0.001, blockindex.nHeight);

      //pblockindex = pblockindex->pprev; 
  }

}


BOOST_AUTO_TEST_CASE(test_tx_remove)
{
  //CTransaction tx;
  CMutableTransaction mtx;
  const std::string  stx = "0100000001cff3b2773f382c7c801dc9efd0eebff3005ccf381b7563ed7419a1addba23ea500000000fdfe00004830450221009ac49d6ebaa6629d9e42702b2aeb276f07188323e1e04b068cd1c456b79ad66a022012e6f22c456f3247034cc101b8919027f317dcef73eaf2dac17dc4f62ac2e91401483045022100f2e6d941131d2cb8a6646f4ac4cc279902bcf7313c1bdb0894fba2e21752d18d02200827f219104a1211f33f89117b2127c749df8060e935b953056a2ef359451874014c69522102d100fca3227e5bb2a718a4b3ab7065f8387d6cb718cf7349b5cd8e703478ba992102bbd58727664a2f3dc59065b406009705eb502b2aebb9c95ed3e190bfaf30639221029eb3c09aee62fa2c291a144c0ba588c8d1e74ba2f66a6549975190bea91e3e2853aeffffffff02402c4206000000001976a914febfd47ff18d35677ad386c2a1d33c2f494ecf7688acf0c323050000000017a914892bc87c69a3c1220640eb8f164ab93117fbcbc78700000000";

  if (!DecodeHexTx(mtx, stx, true))
        BOOST_ERROR("TX decode failed");

  CTransaction tx = CTransaction(std::move(mtx));
 

  if (dbRemoveTx(tx.GetHash()) == -1) { 
        BOOST_ERROR("Bad test: " << tx.GetHash().begin());

  }

}

BOOST_AUTO_TEST_CASE(test_poolinfo)
{
  //CTransaction tx;
  CMutableTransaction mtx;
  const std::string  stx= "00000000000000000000000000000000000000000000000000000000000000000000000001ffffffff6403410407362f454232352f414432332f384d2f22afdca2e562b434edb3c8d67a81d930cce97687c6fae97b7248889296341c7602000000f09f909f134d696e65642062792077696e6a69616e6a756e00000000000000000000000000000000000000000098e2719c0232be3a56000000001976a914c825a1ecf2a6830c4401620c3a16f1995057c2ab88ac00000000000000002f6a24aa21a9eddac81849576b43344bbfb311730a70a394d7217aa2278c3a3bec04b0a68ac5280800000000000000004f254a3c";

  std::vector<unsigned char>   coinbase= ParseHex("03410407362f454232352f414432332f384d2f22afdca2e562b434edb3c8d67a81d930cce97687c6fae97b7248889296341c7602000000f09f909f134d696e65642062792077696e6a69616e6a756e000000000000000000000000000000000000000000");

  const std::string  addr = "1F1xcRt8H8Wa623KqmkEontwAAVqDSAWCV";
  int id = 0;

  //id = getPoolIdByPrefix((const unsigned char*)coinbase.data(), coinbase.size());
  //BOOST_CHECK(id == POOL_1HASH);
  //printf("id=%d, 1hash=%d", id, POOL_1HASH);

  id = getPoolIdByAddr((const char*)addr.c_str());
  BOOST_CHECK(id == POOL_1HASH);
  printf("id=%d, 1hash=%d", id, POOL_1HASH);

  //if (!DecodeHexTx(mtx, stx, true))
  //      BOOST_ERROR("TX decode failed");

  //CTransaction tx = CTransaction(std::move(mtx));                                                                                                           

  //getPoolId(tx);

}

//BOOST_AUTO_TEST_CASE(test_poolbip)
//{
//    std::vector<unsigned char>   coinbase= ParseHex("03410407362f454232352f414432332f384d2f22afdca2e562b434edb3c8d67a81d930cce97687c6fae97b7248889296341c7602000000f09f909f134d696e65642062792077696e6a69616e6a756e000000000000000000000000000000000000000000");
//
//  std::vector<unsigned char>  bucoinbase= ParseHex("03e30707244d696e656420627920416e74506f6f6c20626a31352f4542312f4144362f422058e73996fabe6d6de96c37208654a8142735f2c2ef61afea04e414d8f2e980817448927a93639e1704000000000000002e000000aa7f0100");
//
//  int poolBip = 0;
//
//  //  if (version == 0x20000000)
//  //      ver &= BIP_9;
//  //  if (version == 0x20000001)
//  //      ver &= BIP_CSV;
//  //  if (version == 0x20000002)
//  //      ver &= BIP_SW;
//  //  if (version == 0x20000004)
//  //      ver &= BIP_101_8M;
//  //  if (version == 0x20000008)
//  //      ver &= BIP_101_2M;
//  //  if ((version & 0x30000000) == 0x30000000)
//  //      ver &= BIP_CLASSIC;
// 
//  poolBip = getPoolSupportBip((const unsigned char*)&coinbase[0], coinbase.size(), 0x20000002);
//  BOOST_CHECK(poolBip  == BIP_SW);
//  printf("poolBip=%d, BIP_BU=%d", poolBip, BIP_SW);
//
//  poolBip = getPoolSupportBip((const unsigned char*)&bucoinbase[0], bucoinbase.size(), 0x20000000);
//  BOOST_CHECK(poolBip  == BIP_BU);
//  printf("poolBip=%d, BIP_BU=%d", poolBip, BIP_BU);
//}

BOOST_AUTO_TEST_SUITE_END() 
BOOST_AUTO_TEST_SUITE(sw_tests)
BOOST_FIXTURE_TEST_CASE(check_segwit_address, TestChain100Setup)
{
    // Test that passing CheckInputs with one set of script flags doesn't imply
    // that we would pass again with a different set of flags.
    {
        LOCK(cs_main);
        InitScriptExecutionCache();
    }

    //hex
    //01000000000101cee8516504a650ac9b729d2459c879fa67c1fb5eda02a005d8ef5ba35dbeed340100000000ffffffff0300c2eb0b000000001976a9142eda4d206bdc8f7f612a07d84d5f835531a1a59588ac80969800000000001976a9149dd2672a5956945375b214a28ed58b2a099e120188ac0066781700000000220020701a8d401c84fb13e6baf169d59684e17abd9fa216c8cc5b9fc63d622ff8c58d0400483045022100a8a4f9980d0426c7bb3d1c8431707c73511361de03d9d8d841fa80dd58f26ece02201ea4ebfa87f8f3862b333e3c1a4cf1ed47356b9c4fc2eb74bbc45f4ac15e8e7801473044022034185008cc7b73f73f828552b00c88b493ac7c3bcc85858feffe2f2a3ac03cfd02205af9836c3f6d12a6fa06f86f384c7f49c67bbdb130def4f5e61e3a0525b00b3e016952210375e00eb72e29da82b89367947f29ef34afb75e8654f6ea368e0acdfd92976b7c2103a1b26313f430c4b15bb1fdce663207659d8cac749a0e53d70eff01874496feff2103c96d495bfdd5ba4145e3e046fee45e84a8a48ad05bd8dbb395c011a32cf9f88053ae00000000
    //
    //string raw_tx = "01000000000101cee8516504a650ac9b729d2459c879fa67c1fb5eda02a005d8ef5ba35dbeed340100000000ffffffff0300c2eb0b000000001976a9142eda4d206bdc8f7f612a07d84d5f835531a1a59588ac80969800000000001976a9149dd2672a5956945375b214a28ed58b2a099e120188ac0066781700000000220020701a8d401c84fb13e6baf169d59684e17abd9fa216c8cc5b9fc63d622ff8c58d0400483045022100a8a4f9980d0426c7bb3d1c8431707c73511361de03d9d8d841fa80dd58f26ece02201ea4ebfa87f8f3862b333e3c1a4cf1ed47356b9c4fc2eb74bbc45f4ac15e8e7801473044022034185008cc7b73f73f828552b00c88b493ac7c3bcc85858feffe2f2a3ac03cfd02205af9836c3f6d12a6fa06f86f384c7f49c67bbdb130def4f5e61e3a0525b00b3e016952210375e00eb72e29da82b89367947f29ef34afb75e8654f6ea368e0acdfd92976b7c2103a1b26313f430c4b15bb1fdce663207659d8cac749a0e53d70eff01874496feff2103c96d495bfdd5ba4145e3e046fee45e84a8a48ad05bd8dbb395c011a32cf9f88053ae00000000";
    
    //CDataStream stream(ParseHex(raw_tx), SER_NETWORK, PROTOCOL_VERSION);
    //CTransaction pre_tx(deserialize, stream);
    //stream >> tx;

    CScript p2pk_scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    CScript p2sh_scriptPubKey = GetScriptForDestination(CScriptID(p2pk_scriptPubKey));
    CScript p2pkh_scriptPubKey = GetScriptForDestination(coinbaseKey.GetPubKey().GetID());
    CScript p2wpkh_scriptPubKey = GetScriptForWitness(p2pkh_scriptPubKey);

    CBasicKeyStore keystore;
    keystore.AddKey(coinbaseKey);
    keystore.AddCScript(p2pk_scriptPubKey);

    // flags to test: SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY, SCRIPT_VERIFY_CHECKSEQUENCE_VERIFY, SCRIPT_VERIFY_NULLDUMMY, uncompressed pubkey thing

    // Create 2 outputs that match the three scripts above, spending the first
    // coinbase tx.
    CMutableTransaction spend_tx;

    spend_tx.nVersion = 1;
    spend_tx.vin.resize(1);
    spend_tx.vin[0].prevout.hash = coinbaseTxns[0].GetHash();
    spend_tx.vin[0].prevout.n = 0;
    spend_tx.vout.resize(4);
    spend_tx.vout[0].nValue = 11*CENT;
    spend_tx.vout[0].scriptPubKey = p2sh_scriptPubKey;
    spend_tx.vout[1].nValue = 11*CENT;
    spend_tx.vout[1].scriptPubKey = p2wpkh_scriptPubKey;
    spend_tx.vout[2].nValue = 11*CENT;
    spend_tx.vout[2].scriptPubKey = CScript() << OP_CHECKLOCKTIMEVERIFY << OP_DROP << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    spend_tx.vout[3].nValue = 11*CENT;
    spend_tx.vout[3].scriptPubKey = CScript() << OP_CHECKSEQUENCEVERIFY << OP_DROP << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;

    // Sign, with a non-DER signature
    {
        std::vector<unsigned char> vchSig;
        uint256 hash = SignatureHash(p2pk_scriptPubKey, spend_tx, 0, SIGHASH_ALL, 0, SIGVERSION_BASE);
        BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
        vchSig.push_back((unsigned char) 0); // padding byte makes this non-DER
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        spend_tx.vin[0].scriptSig << vchSig;
    }

    dbSaveTx(spend_tx);

}
 

BOOST_AUTO_TEST_SUITE_END() 
