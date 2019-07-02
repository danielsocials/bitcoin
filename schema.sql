--
-- PostgreSQL database dump
--

-- Dumped from database version 10.9 (Ubuntu 10.9-0ubuntu0.18.04.1)
-- Dumped by pg_dump version 10.9 (Ubuntu 10.9-0ubuntu0.18.04.1)

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

--
-- Name: plpgsql; Type: EXTENSION; Schema: -; Owner: 
--

CREATE EXTENSION IF NOT EXISTS plpgsql WITH SCHEMA pg_catalog;


--
-- Name: EXTENSION plpgsql; Type: COMMENT; Schema: -; Owner: 
--

COMMENT ON EXTENSION plpgsql IS 'PL/pgSQL procedural language';


--
-- Name: add_blk_statics(integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.add_blk_statics(blkid integer) RETURNS void
    LANGUAGE plpgsql
    AS $_$                                                                                                                     
BEGIN
    update blk set total_in_count=t.a, total_out_count=t.b, total_in_value=t.c, total_out_value=t.d, fees=t.e from (select sum(in_count) as a,sum(out_count) as b, sum(in_value) as c, sum(out_value) as d, sum(fee) as e from tx where id in (select tx_id from blk_tx where blk_id=$1)) as t where blk.id=$1;

    delete from utx where id in (select tx_id from blk_tx where blk_id=$1);
    update tx set removed=false where id in (select tx_id from blk_tx where blk_id=$1);
    delete from mempool where tx_id in (select tx_id from blk_tx where blk_id=$1);
END;
$_$;


ALTER FUNCTION public.add_blk_statics(blkid integer) OWNER TO postgres;

--
-- Name: add_tx_statics(integer, integer, integer, bigint, bigint); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.add_tx_statics(txid integer, inc integer, outc integer, inv bigint, outv bigint) RETURNS void
    LANGUAGE plpgsql
    AS $_$
BEGIN
    IF (inv = 0) THEN
        update tx set in_count=$2, out_count=$3, in_value=$4, out_value=$5, fee=0 where id=$1;
    else
        update tx set in_count=$2, out_count=$3, in_value=$4, out_value=$5, fee=($4-$5) where id=$1;
    END IF;
    perform update_addr_balance($1);
END
$_$;


ALTER FUNCTION public.add_tx_statics(txid integer, inc integer, outc integer, inv bigint, outv bigint) OWNER TO postgres;

--
-- Name: blk_to_json(integer, integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.blk_to_json(blkid integer, txcount integer) RETURNS json
    LANGUAGE plpgsql
    AS $_$
    DECLARE blkJson json;
    DECLARE txJson json;
    DECLARE jStr json;
    DECLARE r record;
    DECLARE block record;
    DECLARE ar json[];
BEGIN
    select * from v_blk where v_blk.id=$1 into block;
    blkJson = (select row_to_json (block));
    jStr = (select row_to_json(t) from (select hash from blk where height=(block.height-1)) as t);
    if jStr is not NULL then
        blkJson = json_merge(blkJson, (select json_build_object('nextblockhash', txJson)));
    end if;

    FOR r in select tx_id from blk_tx where blk_id>$1 order by idx limit $2 LOOP
        txJson = (select row_to_json (t) from (select * from tx where tx.id=r.tx_id) as t);

        jStr := (SELECT json_agg(sub) FROM  (select * from (select address, value, txin_tx_id, txout_tx_hash, in_idx from stxo where txin_tx_id=$1 union select address, value, txin_tx_id, txout_tx_hash, in_idx from vtxo where txin_tx_id=$1 ) as t order by in_idx) as sub);
        txJson = json_merge(txJson, (select json_build_object('in_addresses', jStr)));
 
        jStr := (SELECT json_agg(sub) FROM  (select * from (select address, value, txin_tx_id, txin_tx_hash, out_idx from stxo where txout_tx_id=$1 union select  address, value, txin_tx_id, txout_tx_hash, out_idx from vtxo where txout_tx_id=$1) as t order by out_idx) as sub);
        txJson = json_merge(txJson, (select json_build_object('out_addresses', jStr)));
        ar=(select array_append(ar,txJson));
    END LOOP;

    txJson=(select array_to_json(ar));
    blkJson = json_merge(blkJson, (select json_build_object('txs', txJson)));
    blkJson = json_merge(blkJson, (select json_build_object('type', 'blk')));
     
    return blkJson;
END;
$_$;


ALTER FUNCTION public.blk_to_json(blkid integer, txcount integer) OWNER TO postgres;

--
-- Name: btc_stat_addr(); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.btc_stat_addr() RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN

    drop table IF EXISTS  na,addr_r1,addr_r2,addr_r3;
    create table na as select id,balance from addr where balance >0; 
    create index i_ba_na on na using btree (balance);
    create table addr_r1 as select * from (WITH series AS ( SELECT generate_series(0, (select max(balance) from na), 10000000000) AS r_from), range AS (
    SELECT r_from, (r_from + 10000000000) AS r_to FROM series )
    SELECT r_from/100000000 as min, r_to/100000000 as max, (SELECT count(*) FROM na WHERE balance BETWEEN r_from AND r_to) as count
    FROM range
    ) as a where a. count>0 order by count desc ;
    
    create table addr_r2 as select * from (WITH series AS ( SELECT generate_series(0, 10000000000, 100000000) AS r_from), range AS (
    SELECT r_from, (r_from + 100000000) AS r_to FROM series )
    SELECT r_from/100000000 as min, r_to/100000000 as max, (SELECT count(*) FROM na WHERE balance BETWEEN r_from AND r_to) as count
    FROM range
    ) as a where a. count>0 order by count desc ;
    
    create table addr_r3 as select * from (WITH series AS ( SELECT generate_series(0, 100000000, 1000000) AS r_from), range AS (
    SELECT r_from, (r_from + 1000000) AS r_to FROM series )
    SELECT r_from/1000000 as min, r_to/1000000 as max, (SELECT count(*) FROM na WHERE balance BETWEEN r_from AND r_to) as count
    FROM range
    ) as a where a. count>0 order by count desc ;
 
END;
$$;


ALTER FUNCTION public.btc_stat_addr() OWNER TO postgres;

--
-- Name: btc_stat_json(integer, integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.btc_stat_json(start_time integer, end_time integer) RETURNS json
    LANGUAGE plpgsql
    AS $_$
    DECLARE blk_json json;
    DECLARE tx_json json;
    DECLARE addr_json json;
    DECLARE jStr json;
    DECLARE tStr json;
    DECLARE max_height integer;
    DECLARE min_height integer;
BEGIN
    select min(height),max(height) into min_height, max_height from blk where orphan=false and time>=start_time and time<end_time;

    jStr = (select json_build_object('start_time', (select to_char(to_timestamp(start_time),'YYYY-MM-DD'))));
    jStr = json_merge(jStr, (select json_build_object('end_time', (select to_char(to_timestamp(end_time),'YYYY-MM-DD')))));

    blk_json = (select row_to_json(t) from (select sum(fees)*1.0/(sum(total_in_value)) as fee_total_value, sum(fees)/(count(1) * 12.5) as fees_per_blk,  sum(fees)*1.0/((sum(tx_count)-count(1))) as fee_per_tx, (24*60.0)/count(1) as blk_create_rate, count(1) * 1250000000 as new_btc_value, count(1) as blk_count, sum(blk_size) as sum_blk_size, sum(total_in_count) as total_in_count, sum(total_in_value) as total_in_value, sum(total_out_count) as total_out_count, sum(total_out_value) as total_out_value, sum(tx_count)/(24*3600.0) as tx_per_second, sum(tx_count) as tx_count, sum(fees) as fees, min(height) as min_blk_height, max(height) as max_blk_height, min(blk_size) as min_blk_size, max(blk_size) as max_blk_size, min(tx_count) as min_tx_count, max(tx_count) as max_tx_count from blk where orphan=false and height>=min_height and height<=max_height) as t); 

    blk_json = json_merge(blk_json, (select json_build_object('orphan_blk_count', (select count(1) from blk where orphan=true and time>=$1 and time<$2 ))));
    jStr = json_merge(jStr, (select json_build_object('blkstat', blk_json)));

    tx_json = (select row_to_json(t) from (select count(1) as total_count, max(fee/tx_size) as max_fee_per_byte, sum(tx_size) as total_tx_size, min(tx_size) as min_tx_size, max(tx_size) as max_tx_size, min(fee) as min_fee, max(fee) as max_fee, min(in_count) as min_in_count, max(in_count) as max_in_count, min(in_value) as min_in_value, max(in_value) as max_in_value, min(out_count) as min_out_count,max(out_count) as max_out_count, min(out_value) as min_out_value, max(out_value) as max_out_value  from tx where recv_time>=$1 and recv_time<$2) as t); 
    jStr = json_merge(jStr, (select json_build_object('rcvtxstat', tx_json)));
    tx_json = (select row_to_json(t) from (select count(1) as total_count, max(fee/tx_size) as max_fee_per_byte, sum(tx_size) as total_tx_size, min(tx_size) as min_tx_size, max(tx_size) as max_tx_size, min(fee) as min_fee, max(fee) as max_fee, min(in_count) as min_in_count, max(in_count) as max_in_count, min(in_value) as min_in_value, max(in_value) as max_in_value, min(out_count) as min_out_count,max(out_count) as max_out_count, min(out_value) as min_out_value, max(out_value) as max_out_value, sum(abs(c.confirm_time-tx.recv_time)) as total_confirm_time, max(abs(c.confirm_time-tx.recv_time)) as max_confirm_time, min(abs(c.confirm_time-tx.recv_time)) as min_confirm_time from tx join (select tx_id, recv_time as confirm_time from blk_tx a join blk b on (b.id=a.blk_id) where b.orphan=false and b.height>=min_height and b.height<=max_height) as c on (c.tx_id=tx.id and tx.coinbase=false and c.confirm_time>=$1 and tx.recv_time>=$1)) as t); 
    jStr = json_merge(jStr, (select json_build_object('realtxstat', tx_json)));
    tx_json = (select row_to_json(t) from (select max(fee/tx_size) as max_fee_per_byte, sum(tx_size) as total_tx_size, min(tx_size) as min_tx_size, max(tx_size) as max_tx_size, min(fee) as min_fee, max(fee) as max_fee, min(in_count) as min_in_count, max(in_count) as max_in_count, min(in_value) as min_in_value, max(in_value) as max_in_value, min(out_count) as min_out_count,max(out_count) as max_out_count, min(out_value) as min_out_value, max(out_value) as max_out_value from tx join (select tx_id from blk_tx a join blk b on (b.id=a.blk_id) where b.orphan=false and b.height>=min_height and b.height<=max_height) as c on (c.tx_id=tx.id and tx.coinbase=false)) as t); 
    tx_json = json_merge(tx_json, (select json_build_object('max_fee_per_byte_tx', (select hash from tx where recv_time>=$1 and recv_time<$2 and fee/tx_size=(select max(fee/tx_size) from tx where recv_time>=$1 and recv_time<$2) limit 1))));
    tx_json = json_merge(tx_json, (select json_build_object('max_fee_tx', (select hash from tx where recv_time>=$1 and recv_time<$2 and fee=(select max(fee) from tx where recv_time>=$1 and recv_time<$2) limit 1))));
    tx_json = json_merge(tx_json, (select json_build_object('max_size_tx', (select hash from tx where recv_time>=$1 and recv_time<$2 and tx_size=(select max(tx_size) from tx where recv_time>=$1 and recv_time<$2) limit 1))));
    tx_json = json_merge(tx_json, (select json_build_object('total_tx_count', (select count(1) from tx where recv_time>=$1 and recv_time<$2))));
    tx_json = json_merge(tx_json, (select json_build_object('removed_tx_count', (select count(1) from tx where removed=true and recv_time>=$1 and recv_time<$2))));
    tx_json = json_merge(tx_json, (select json_build_object('confirm_tx_count', (select count(1) from tx where coinbase=false and id in (select tx_id from blk_tx a join blk b on (b.id=a.blk_id) where b.orphan=false and b.height>=min_height and b.height<=max_height)))));
    tx_json = json_merge(tx_json, (select json_build_object('confirm_today_tx_count', (select count(1) from tx where coinbase=false and recv_time>=$1 and recv_time<$2 and id in (select tx_id from blk_tx a join blk b on (b.id=a.blk_id) where b.orphan=false and b.height>=min_height and b.height<=max_height)))));
    tx_json = json_merge(tx_json, (select json_build_object('confirm_prev_tx_count', (select count(1) from tx where recv_time!=0 and recv_time<$1 and coinbase=false and id in (select tx_id from blk_tx a join blk b on (b.id=a.blk_id) where b.orphan=false and b.height>=min_height and b.height<=max_height)))));
    tx_json = json_merge(tx_json, (select json_build_object('unconfirmed_tx_count', (select count(1) from tx where coinbase=false and recv_time>=$1 and recv_time<$2 and id not in (select tx_id from blk_tx a join blk b on (b.id=a.blk_id) where b.orphan=false and b.height>=min_height and b.height<=max_height)))));
    tx_json = json_merge(tx_json, (select json_build_object('unconfirmed_tx_count_after_6_blk', (select count(1) from tx where coinbase=false and recv_time>=$1 and recv_time<$2 and id not in (select tx_id from blk_tx a join blk b on (b.id=a.blk_id) where b.orphan=false and b.height>=min_height and b.height<=(max_height+6))))));
    tx_json = json_merge(tx_json, (select json_build_object('rcv_free_tx_count', (select count(1) from tx where fee=0 and recv_time>=$1 and recv_time<$2))));
    tx_json = json_merge(tx_json, (select json_build_object('confirm_totlal_free_tx_count', (select count(1) from tx where fee=0 and coinbase=false and id in (select tx_id from blk_tx a join blk b on (b.id=a.blk_id) where b.orphan=false and b.height>=min_height and b.height<=max_height)))));
    tx_json = json_merge(tx_json, (select json_build_object('confirm_today_free_tx_count', (select count(1) from tx where fee=0 and recv_time>=$1 and recv_time<$2 and coinbase=false and id in (select tx_id from blk_tx a join blk b on (b.id=a.blk_id) where b.orphan=false and b.height>=min_height and b.height<=max_height)))));
    jStr = json_merge(jStr, (select json_build_object('txstat', tx_json)));
    tx_json = (select json_agg(t) from (select hash,tx_size,fee, fee/tx_size as fee_byte, recv_time from tx  where recv_time>=$1 and recv_time<(select time from blk where height=(max_height-1) and orphan=false) and id not in (select tx_id from blk_tx a join blk b on (b.id=a.blk_id) where b.orphan=false and b.height>=min_height and b.height<=max_height) order by fee_byte desc limit 10) as t);
    jStr = json_merge(jStr, (select json_build_object('utxs', tx_json)));

    drop table IF EXISTS  t_out, t_in, s_in, s_out; 

    create UNLOGGED table t_out as select a.tx_id,a.value, c.id as addr_id, a.type  from txout a left join addr_txout b on (b.txout_id=a.id) left join addr c on (c.id=b.addr_id) join blk_tx d on (d.tx_id=a.tx_id) join blk e on (e.id=d.blk_id and e.orphan=false and e.height>=min_height and e.height<=max_height);
    create UNLOGGED table t_in  as (select addr_id,value,txin_tx_id as tx_id, txout_id from stxo where  txin_tx_id in (select tx_id from blk_tx a join blk b on (b.id=a.blk_id) where b.orphan=false and b.height>=min_height and b.height<=max_height));

    tx_json = (select json_agg(t) from ((select count,e.name from (select count(1),type from (select * from t_in a join txout b on (b.id=a.txout_id)) as c group by type) as d join txout_type e on (e.id=d.type))) as t);
    jStr = json_merge(jStr, (select json_build_object('txin_type', tx_json)));

    tx_json = (select json_agg(t) from ((select count,b.name from (select count(1),type from t_out group by type) as a join txout_type b on (b.id=a.type))) as t);
    jStr = json_merge(jStr, (select json_build_object('txout_type', tx_json)));

    create UNLOGGED table s_in as select sum(value) as value, count(1),addr_id,tx_id  from t_in group by addr_id,tx_id;
    create UNLOGGED table s_out as select sum(value) as value, count(1),addr_id,tx_id  from t_out group by addr_id,tx_id;
    update s_in a set value=(a.value-b.value) from s_out b where a.tx_id=b.tx_id and a.addr_id=b.addr_id; 
    delete from t_out a using t_in b where a.tx_id=b.tx_id and a.addr_id=b.addr_id;
    
    addr_json=(select json_agg(t) from (select sum(value)/100000000 as value,c.name from t_in a join addr_g b on(b.id=a.addr_id) left join addr_g_tag c on(c.id=b.group_id) group by c.name order by value desc limit 10) as t); 
    jStr = json_merge(jStr, (select json_build_object('spent_app', addr_json)));

    addr_json=(select json_agg(t) from (select sum(value)/100000000 as value,c.name from t_out a join addr_g b on(b.id=a.addr_id) left join addr_g_tag c on(c.id=b.group_id) group by c.name order by value desc limit 10) as t); 
    jStr = json_merge(jStr, (select json_build_object('recv_app', addr_json)));

    addr_json=(select json_agg(t) from (select i.*,h.address,g.name from (select sum(value)/100000000 as recv_value, count(1) as recv_count,addr_id from t_out where addr_id is not NULL group by addr_id order by recv_count desc limit 10) as i left join addr_g f on (f.id=i.addr_id) left join addr_g_tag g on (g.id=f.group_id) left join addr h on (h.id=i.addr_id) order by recv_count desc) as t);

    jStr = json_merge(jStr, (select json_build_object('recv_tx_count', addr_json)));

    addr_json=(select json_agg(t) from (select i.*,h.address,g.name from (select sum(value)/100000000 as recv_value, count(1) as recv_count,addr_id from t_out group by addr_id order by recv_value desc limit 10) as i left join addr_g f on (f.id=i.addr_id) left join addr_g_tag g on (g.id=f.group_id) left join addr h on (h.id=i.addr_id) order by recv_value desc) as t);

    jStr = json_merge(jStr, (select json_build_object('recv_tx_value', addr_json)));

    addr_json=(select json_agg(t) from (select i.*,h.address,g.name from (select value/100000000 as spent_value, count as spent_count,addr_id from s_in order by count desc limit 10) as i  left join addr_g f on (f.id=i.addr_id) left join addr_g_tag g on (g.id=f.group_id) left join addr h on (h.id=i.addr_id) order by spent_count desc) as t);
 
    jStr = json_merge(jStr, (select json_build_object('spent_tx_count', addr_json)));

    addr_json=(select json_agg(t) from ( select i.*,h.address,g.name from (select value/100000000 as spent_value, count as spent_count,addr_id from s_in order by value desc limit 10) as i  left join addr_g f on (f.id=i.addr_id) left join addr_g_tag g on (g.id=f.group_id) left join addr h on (h.id=i.addr_id) order by spent_value desc) as t);

    jStr = json_merge(jStr, (select json_build_object('spent_tx_value', addr_json)));

    jStr = json_merge(jStr, (select json_build_object('new_addr_count', (select (select max(addr_id) from vtxo where  height=max_height) - (select max(addr_id) from vtxo where  height=(min_height-1))))));
    jStr = json_merge(jStr, (select json_build_object('new_multi_addr_count', (select count(1) from addr where id<=(select max(addr_id) from vtxo where  height=max_height) and id>(select max(addr_id) from vtxo where  height=(min_height-1)) and address like '3%'))));

    tStr = (select json_agg(t) from (select max(fee) as max_fee, min(fee) as min_fee, max(tx_size) as max_tx_size, min(tx_size) as min_tx_size, sum(fee)/count(1) as fee_per_tx, sum(c.tx_size) as sum_tx_size, sum(c.fee) as sum_fee, sum(c.fee)/sum(tx_size) as fee_byte, count(1),d.name from blk_tx a join blk b on (a.blk_id=b.id) join tx c on (c.id=a.tx_id) join meta d on (d.id=c.metatype) where  b.height>=min_height and b.height<=max_height group by d.name order by count desc) as t);
    jStr = json_merge(jStr, (select json_build_object('meta', tStr)));
     
    tStr = (select json_agg(t) from (select max(fee) as max_fee, min(fee) as min_fee, max(tx_size) as max_tx_size, min(tx_size) as min_tx_size, sum(fee)/count(1) as fee_per_tx, sum(c.tx_size) as sum_tx_size, sum(c.fee) as sum_fee, sum(c.fee)/sum(tx_size) as fee_byte, count(1) from blk_tx a join blk b on (a.blk_id=b.id) join tx c on (c.id=a.tx_id) join meta d on (d.id=c.metatype) where  b.height>=min_height and b.height<=max_height) as t);

    jStr = json_merge(jStr, (select json_build_object('meta_total', tStr)));

    tStr = (select json_agg(t) from (select count(1),b.name from blk a join pool b on (a.pool_id=b.id) where a.height>=min_height and a.height<=max_height group by b.name order by count desc) as t);
    jStr = json_merge(jStr, (select json_build_object('pool', tStr)));
    tStr = (select json_agg(t) from (select a.address,a.balance,c.name from addr a left join addr_g b on (b.id=a.id) left join addr_g_tag c on (c.id=b.group_id) order by a.balance desc limit 10) as t);
    jStr = json_merge(jStr, (select json_build_object('top', tStr)));

    return jStr;
END;
$_$;


ALTER FUNCTION public.btc_stat_json(start_time integer, end_time integer) OWNER TO postgres;

--
-- Name: check_blk_count(); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.check_blk_count() RETURNS boolean
    LANGUAGE plpgsql
    AS $$
    DECLARE blk_count1 integer;
    DECLARE blk_count2 integer;
    DECLARE const_stat RECORD;
BEGIN
    select * into const_stat from blk_stat order by id desc limit 1;
    blk_count1 = (select count(1) from blk where height>const_stat.max_height and orphan!=true) + const_stat.max_height;
    blk_count2 = (select max(height) from blk where height>const_stat.max_height and orphan!=true);
    if blk_count1 != blk_count2 then return false; end if;
    return true;
END;
$$;


ALTER FUNCTION public.check_blk_count() OWNER TO postgres;

--
-- Name: check_tx_count(); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.check_tx_count() RETURNS boolean
    LANGUAGE plpgsql
    AS $$
    DECLARE tx_count1 integer;
    DECLARE tx_count2 integer;
    DECLARE tx_count3 integer;
    DECLARE max_id record;
    DECLARE const_stat RECORD;
BEGIN
    select * into const_stat from blk_stat order by id desc limit 1;
    for max_id in (select blk_id,tx_id from blk_tx order by tx_id desc limit 1) loop
    tx_count1 = (select coalesce(sum(tx_count),0) from blk where id<=max_id.blk_id and id>const_stat.max_blk_id);
    tx_count2 = (select coalesce(count(1),0) from tx a join blk_tx b on (a.id=b.tx_id) where b.blk_id<=max_id.blk_id and b.blk_id>const_stat.max_blk_id);
    tx_count3 = (select coalesce(count(tx_id),0) from blk_tx  where blk_id<=max_id.blk_id  and blk_id>const_stat.max_blk_id);
    if tx_count1 != tx_count2 then return false; end if;
    if tx_count3 != tx_count2 then return false; end if;
    end loop;
    return true;  
END;
$$;


ALTER FUNCTION public.check_tx_count() OWNER TO postgres;

--
-- Name: create_group_table(integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.create_group_table(maxtxid integer) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN               
delete from addr_group_tmp;
delete from addr_send;
CREATE MATERIALIZED VIEW group_vout as SELECT g.id as addr_id, e.id AS txin_tx_id FROM txout a LEFT JOIN tx b ON b.id = a.tx_id left join txin c on (c.prev_out=b.hash and c.prev_out_index=a.tx_idx) left JOIN tx e ON e.id = c.tx_id left JOIN addr_txout f on f.txout_id=a.id left JOIN addr g on g.id=f.addr_id where e.id is not NULL and g.id is not NULL and b.id>maxTxId;
insert into addr_send select distinct addr_id,txin_tx_id as tx_id from group_vout;
end;
$$;


ALTER FUNCTION public.create_group_table(maxtxid integer) OWNER TO postgres;

--
-- Name: delete_all_utx(); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.delete_all_utx() RETURNS void
    LANGUAGE plpgsql
    AS $$
    DECLARE txid integer;
BEGIN
     FOR txid IN select id from utx LOOP
         perform delete_tx(txid);
     END LOOP;
END;
$$;


ALTER FUNCTION public.delete_all_utx() OWNER TO postgres;

--
-- Name: delete_blk(bytea); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.delete_blk(blkhash bytea) RETURNS void
    LANGUAGE plpgsql
    AS $$
    declare blkid integer;
    declare txid integer;
    BEGIN
    blkid=(select id from blk where hash=blkhash);
    txid=(select tx_id from blk_tx where blk_id=blkid and idx=0);
    insert into utx select tx_id from blk_tx where blk_id=blkid and tx_id!=txid;
    update blk set orphan=true where id=blkid; 
    perform delete_tx(txid);
    END
$$;


ALTER FUNCTION public.delete_blk(blkhash bytea) OWNER TO postgres;

--
-- Name: delete_some_utx(); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.delete_some_utx() RETURNS void
    LANGUAGE plpgsql
    AS $$
    DECLARE txid integer;
BEGIN
     FOR txid IN select id from utx order by id desc limit 100 LOOP
         perform delete_tx(txid);
     END LOOP;
END;
$$;


ALTER FUNCTION public.delete_some_utx() OWNER TO postgres;

--
-- Name: delete_tx(integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.delete_tx(txid integer) RETURNS void
    LANGUAGE plpgsql
    AS $_$
    DECLARE ntx RECORD;
BEGIN
    if (select removed from tx where id=$1)=true then
       return;
    end if;
     FOR ntx IN select txin_tx_id from vout where txout_tx_id=$1 and txin_tx_id is not NULL LOOP
         perform delete_tx(ntx.txin_tx_id);
     END LOOP;
     perform  rollback_addr_balance($1);
     delete from utx where id=$1;
     update tx set removed=true where id=$1;
END;
$_$;


ALTER FUNCTION public.delete_tx(txid integer) OWNER TO postgres;

--
-- Name: get_confirm(integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.get_confirm(txid integer) RETURNS integer
    LANGUAGE plpgsql
    AS $_$
    DECLARE tx_height integer;
    DECLARE max_height integer;
BEGIN
    tx_height=(select c.height from tx a join blk_tx b on(b.tx_id=a.id) join blk c on (c.id=b.blk_id and c.orphan!=true) where a.id=$1 order by c.height asc limit 1);
    max_height=(select max(height) from blk where orphan!=true);
    return (max_height-tx_height+1);
END;
$_$;


ALTER FUNCTION public.get_confirm(txid integer) OWNER TO postgres;

--
-- Name: group_addr(); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.group_addr() RETURNS void
    LANGUAGE plpgsql
    AS $$
    DECLARE groupid INTEGER;
    DECLARE count1 INTEGER;
    DECLARE count2 INTEGER;
BEGIN
    groupid=4;
    while TRUE LOOP
        insert into addr_group_tmp (select addr_id, tx_id from addr_send where group_id is NULL limit 1);

        GET DIAGNOSTICS count1 = ROW_COUNT;
        IF count1=0  THEN
           RAISE NOTICE 'exit 0';
           EXIT;
        END IF;

        while TRUE LOOP
            update addr_send set group_id=groupid where addr_id in (select distinct addr_id from addr_group_tmp);
            insert into addr_group_tmp (select addr_id, tx_id from addr_send where group_id is NULL and addr_id in (select distinct addr_id from addr_group_tmp));
            GET DIAGNOSTICS count1 = ROW_COUNT;
            RAISE NOTICE 'count1 %', count1;
            GET DIAGNOSTICS count1 = ROW_COUNT;
            insert into addr_group_tmp (select addr_id, tx_id from addr_send where group_id is NULL and tx_id in (select distinct tx_id from addr_group_tmp));
            GET DIAGNOSTICS count2 = ROW_COUNT;
            RAISE NOTICE 'count2 %', count2;
            if count1=0 and count2=0 THEN
                RAISE NOTICE 'exit 0';
                EXIT;
            END IF;
        END LOOP;
        groupid = groupid + 1;
        truncate table addr_group_tmp;
        if groupid=5 THEN
            RAISE NOTICE 'exit 1';
            EXIT;
        END IF;
    END LOOP;
END
$$;


ALTER FUNCTION public.group_addr() OWNER TO postgres;

--
-- Name: insert_addr(text, text); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.insert_addr(a text, h text) RETURNS integer
    LANGUAGE plpgsql
    AS $$                                                                                                                     
    declare addrid integer;                                                                                                   
BEGIN                                                                                                                         
    addrid = (select id from addr where address = a);                                                                         
    IF addrid is NULL THEN                                                                                                    
        insert into addr (address, hash160) values(a, h) RETURNING id into addrid;                                            
    END IF;                                                                                                                   
    return addrid;                                                                                                            
END                                                                                                                           
$$;


ALTER FUNCTION public.insert_addr(a text, h text) OWNER TO postgres;

--
-- Name: insert_tag(); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.insert_tag() RETURNS void
    LANGUAGE plpgsql
    AS $$                                                                                                                     
    declare bid integer;                                                                                                      
BEGIN                                                                                                                         
    FOR bid IN select distinct(id) from addr_tag LOOP
      insert into addr_tag1 (select * from addr_tag where id=bid limit 1);
    END LOOP;                                                                                                                 
END;                                                                                                                          
$$;


ALTER FUNCTION public.insert_tag() OWNER TO postgres;

--
-- Name: json_merge(json, json); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.json_merge(data json, merge_data json) RETURNS json
    LANGUAGE sql IMMUTABLE
    AS $$
    SELECT ('{'||string_agg(to_json(key)||':'||value, ',')||'}')::json
    FROM (
        WITH to_merge AS (
            SELECT * FROM json_each(merge_data)
        )
        SELECT *
        FROM json_each(data)
        WHERE key NOT IN (SELECT key FROM to_merge)
        UNION ALL
        SELECT * FROM to_merge
    ) t;
$$;


ALTER FUNCTION public.json_merge(data json, merge_data json) OWNER TO postgres;

--
-- Name: readd_blk(bytea); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.readd_blk(blkhash bytea) RETURNS integer
    LANGUAGE plpgsql
    AS $_$
    DECLARE blkId integer;
    DECLARE o RECORD;
BEGIN
    if (select orphan from blk where hash=$1)!=true then
       return (select id from blk where hash=$1);
    end if;
    blkId := (select id from blk where hash=blkHash);
    FOR o IN select tx_id from blk_tx where blk_id=blkId LOOP                               
       perform readd_tx(o.tx_id);
    END LOOP;

    update blk set orphan=false where id=blkId;
    return blkId;
END
$_$;


ALTER FUNCTION public.readd_blk(blkhash bytea) OWNER TO postgres;

--
-- Name: readd_tx(integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.readd_tx(txid integer) RETURNS void
    LANGUAGE plpgsql
    AS $_$
BEGIN
    if (select removed from tx where id=$1)!=true then
       return;
    end if;
    update tx set removed=false where id=$1;
    perform update_addr_balance($1);
END
$_$;


ALTER FUNCTION public.readd_tx(txid integer) OWNER TO postgres;

--
-- Name: refresh_json_cache(integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.refresh_json_cache(blkid integer) RETURNS void
    LANGUAGE plpgsql
    AS $_$
    DECLARE r record;
    DECLARE t record;
BEGIN
   for r in select  tx_id from blk_tx where blk_id=$1 LOOP
      for t in select distinct txout_tx_id as id, txout_tx_hash as hash from vout where txin_tx_id=r.tx_id LOOP
          if (select 1 from json_cache where key=encode(t.hash,'hex')) then
               update json_cache set val=(select tx_to_json(t.id) where key=encode(t.hash,'hex'));
          end if;
      END LOOP;
   END LOOP;
END;
$_$;


ALTER FUNCTION public.refresh_json_cache(blkid integer) OWNER TO postgres;

--
-- Name: refresh_json_cache(text); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.refresh_json_cache(blkhash text) RETURNS void
    LANGUAGE plpgsql
    AS $_$
    DECLARE r record;
    DECLARE t record;
BEGIN
   for r in select  tx_id from blk_tx where blk_id=(select id from blk where hash=decode($1,'hex')) LOOP
      for t in select distinct txout_tx_id as id, txout_tx_hash as hash from vout where txin_tx_id=r.tx_id LOOP
          if (select 1 from json_cache where key=encode(t.hash,'hex')) then
               update json_cache set val=(select tx_to_json(t.id) where key=encode(t.hash,'hex'));
          end if;
      END LOOP;
   END LOOP;
END;
$_$;


ALTER FUNCTION public.refresh_json_cache(blkhash text) OWNER TO postgres;

--
-- Name: rollback_addr_balance(integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.rollback_addr_balance(txid integer) RETURNS void
    LANGUAGE plpgsql
    AS $$
    DECLARE o RECORD;
BEGIN
    FOR o IN select addr_id, value from vout where txin_tx_id=txid and addr_id is not NULL LOOP
       update addr set balance=(balance + o.value), spent_value=(spent_value-o.value) where id=o.addr_id;
    END LOOP;

    FOR o IN select addr_id, value from vout where txout_tx_id=txid and addr_id is not NULL LOOP
       update addr set balance=(balance - o.value), recv_value=(recv_value-o.value)  where id=o.addr_id;    
    END LOOP;

    FOR o IN select distinct addr_id from vout where txin_tx_id=txid and addr_id is not NULL LOOP
       update addr set spent_count=(spent_count-1) where id=o.addr_id;
    END LOOP;

    FOR o IN select distinct addr_id from vout where txout_tx_id=txid and addr_id is not NULL LOOP
       update addr set recv_count=(recv_count-1) where id=o.addr_id;
    END LOOP;
END;
$$;


ALTER FUNCTION public.rollback_addr_balance(txid integer) OWNER TO postgres;

--
-- Name: save_bigtx_to_json_cache(integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.save_bigtx_to_json_cache(itemcount integer) RETURNS void
    LANGUAGE plpgsql
    AS $_$
    DECLARE r record;
BEGIN
   for r in select id,hash from tx where in_count>$1 or out_count>$1 LOOP
             insert into json_cache (type,hash,js) values(1, r.hash, (select tx_to_json(r.id)));
   END LOOP;
END;
$_$;


ALTER FUNCTION public.save_bigtx_to_json_cache(itemcount integer) OWNER TO postgres;

--
-- Name: save_bigtx_to_json_cache(integer, integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.save_bigtx_to_json_cache(mincount integer, maxcount integer) RETURNS void
    LANGUAGE plpgsql
    AS $_$
    DECLARE r record;
BEGIN
   for r in select id,hash from tx where (in_count+out_count)>$1 and (in_count+out_count)<$2 LOOP
       insert into json_cache (key,val) values(r.hash, (select tx_to_json(r.id)));
   END LOOP;
END;
$_$;


ALTER FUNCTION public.save_bigtx_to_json_cache(mincount integer, maxcount integer) OWNER TO postgres;

--
-- Name: save_bigtx_to_redis(integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.save_bigtx_to_redis(itemcount integer) RETURNS void
    LANGUAGE plpgsql
    AS $_$
    DECLARE r record;
BEGIN
   for r in select id,encode(hash,'hex') as hash from tx where (in_count + out_count)>$1 LOOP
        insert into redis_db0 (key,val) values(r.hash, (select tx_to_json(r.id)));
   END LOOP;
END;
$_$;


ALTER FUNCTION public.save_bigtx_to_redis(itemcount integer) OWNER TO postgres;

--
-- Name: save_bigtx_to_redis(integer, integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.save_bigtx_to_redis(mincount integer, maxcount integer) RETURNS void
    LANGUAGE plpgsql
    AS $_$
    DECLARE r record;
BEGIN
   for r in select id,encode(hash,'hex') as hash from tx where (in_count + out_count)>$1 and (in_count + out_count)<$2 LOOP
        insert into redis_db0 (key,val) values(r.hash, (select tx_to_json(r.id)));
   END LOOP;
END;
$_$;


ALTER FUNCTION public.save_bigtx_to_redis(mincount integer, maxcount integer) OWNER TO postgres;

--
-- Name: sync_mempool_end(); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.sync_mempool_end() RETURNS void
    LANGUAGE plpgsql
    AS $$
    DECLARE txid integer;
BEGIN
     FOR txid IN select a.id from utx_old a left join utx b on (b.id=a.id) where b.id is NULL  LOOP
         perform delete_tx(txid);
     END LOOP;
END;
$$;


ALTER FUNCTION public.sync_mempool_end() OWNER TO postgres;

--
-- Name: sync_mempool_start(); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.sync_mempool_start() RETURNS void
    LANGUAGE plpgsql
    AS $$                                                                                                                     
BEGIN
    insert into utx_old select * from utx;
    delete from utx;
END;
$$;


ALTER FUNCTION public.sync_mempool_start() OWNER TO postgres;

--
-- Name: test(integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.test(txid integer) RETURNS void
    LANGUAGE plpgsql
    AS $_$
    DECLARE ntx RECORD;
BEGIN
     FOR ntx IN select txin_tx_id from vout where txout_tx_id=$1 and txin_tx_id is not NULL LOOP
         perform  test(ntx.txin_tx_id );
     END LOOP;
END;
$_$;


ALTER FUNCTION public.test(txid integer) OWNER TO postgres;

--
-- Name: tru_utx(); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.tru_utx() RETURNS void
    LANGUAGE plpgsql
    AS $$
    declare did integer;
BEGIN
    FOR did IN select id from utx LOOP
      perform delete_tx(did);
    END LOOP;
    truncate table utx;
END;
$$;


ALTER FUNCTION public.tru_utx() OWNER TO postgres;

--
-- Name: tx_to_json(integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.tx_to_json(id integer) RETURNS json
    LANGUAGE plpgsql
    AS $_$
    DECLARE txJson json;
    DECLARE jStr json;
BEGIN
    txJson = (select row_to_json (t) from (select * from v_tx where v_tx.id=$1) as t);

    jStr := (SELECT json_agg(sub) FROM  (select * from txin where tx_id=$1 order by tx_idx) as sub);
    txJson = json_merge(txJson, (select json_build_object('vin', jStr)));

    jStr := (SELECT json_agg(sub) FROM  (select * from txout where tx_id=$1 order by tx_idx) as sub);
    txJson = json_merge(txJson, (select json_build_object('vout', jStr)));

    jStr := (SELECT json_agg(sub) FROM  (select * from (select address, value, txin_tx_id, txout_tx_hash, in_idx from stxo where txin_tx_id=$1 union select address, value, txin_tx_id, txout_tx_hash, in_idx from vtxo where txin_tx_id=$1 ) as t order by in_idx) as sub);
    txJson = json_merge(txJson, (select json_build_object('in_addresses', jStr)));
 
    jStr := (SELECT json_agg(sub) FROM  (select * from (select address, value, txin_tx_id, txin_tx_hash, out_idx from stxo where txout_tx_id=$1 union select  address, value, txin_tx_id, txout_tx_hash, out_idx from vtxo where txout_tx_id=$1) as t order by out_idx) as sub);
    txJson = json_merge(txJson, (select json_build_object('out_addresses', jStr)));
    txJson = json_merge(txJson, (select json_build_object('type', 'tx')));
 
    return txJson;
END;
$_$;


ALTER FUNCTION public.tx_to_json(id integer) OWNER TO postgres;

--
-- Name: update_addr_balance(integer); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.update_addr_balance(txid integer) RETURNS void
    LANGUAGE plpgsql
    AS $_$                                                                                                                     
    DECLARE o RECORD;                                                                                                         
BEGIN                                                                                                                         
    FOR o IN select addr_id, value from vout where txin_tx_id=$1 and addr_id is not NULL LOOP                               
       update addr set balance=(balance - o.value), spent_value=(spent_value+o.value) where id=o.addr_id;                     
    END LOOP;                                                                                                                 
                                                                                                                              
    FOR o IN select addr_id, value from vout where txout_tx_id=$1 and addr_id is not NULL LOOP                              
       update addr set balance=(balance + o.value), recv_value=(recv_value+o.value)  where id=o.addr_id;                      
    END LOOP;                                                                                                                 
                                                                                                                              
                                                                                                                              
    FOR o IN select distinct addr_id from vout where txin_tx_id=$1 and addr_id is not NULL LOOP                             
       update addr set spent_count=(spent_count+1) where id=o.addr_id;                                                        
       insert into addr_tx (addr_id,tx_id) values(o.addr_id, $1);                                                           
    END LOOP;                                                                                                                 
                                                                                                                              
    FOR o IN select distinct addr_id from vout where txout_tx_id=$1 and addr_id is not NULL LOOP                            
       update addr set recv_count=(recv_count+1) where id=o.addr_id;                                                          
       insert into addr_tx (addr_id,tx_id) values(o.addr_id, $1);                                                           
    END LOOP;                                                                                                                 
END;                                                                                                                          
$_$;


ALTER FUNCTION public.update_addr_balance(txid integer) OWNER TO postgres;

--
-- Name: update_stat(); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.update_stat() RETURNS void
    LANGUAGE plpgsql
    AS $$
    DECLARE newHeight  integer;
    DECLARE txCount    integer;
    DECLARE maxBlkId   integer;
    DECLARE maxTxId    integer;
    DECLARE blkSize    bigint;
    DECLARE txSize     bigint;
    DECLARE const_stat RECORD;
BEGIN
    select * into const_stat from blk_stat order by id desc limit 1;
    newHeight = (select (max(height)-6) from blk where orphan!=true);
    if (newHeight = const_stat.max_height) then
       return;
    end if;
    txCount = (select coalesce(sum(tx_count),0) from blk where height<=newHeight and height>const_stat.max_height and orphan!=true);
    maxBlkId = (select id from blk where height=newHeight and orphan!=true);
    maxTxId = (select max(tx_id) from blk_tx a join blk b on (a.blk_id=b.id and b.orphan!=true) where b.height<=newHeight and height>const_stat.max_height);
    maxTxId = (select GREATEST(maxTxId, const_stat.max_tx_id));
    blkSize = (select coalesce(sum(blk_size),0) from blk where height<=newHeight and orphan!=true and height>const_stat.max_height);
    txSize = (select coalesce(sum(tx_size),0) from tx a join blk_tx b on (b.tx_id=a.id) join blk c on (c.id=b.blk_id and orphan!=true ) where c.height<=newHeight and c.height>const_stat.max_height);
    insert into blk_stat (max_height, total_tx_count, max_blk_id, max_tx_id, sum_blk_size, sum_tx_size)
    values(newHeight, (txCount + const_stat.total_tx_count), maxBlkId, maxTxId, (blkSize + const_stat.sum_blk_size), (txSize + const_stat.sum_tx_size));
END;
$$;


ALTER FUNCTION public.update_stat() OWNER TO postgres;

--
-- Name: update_stxo(); Type: FUNCTION; Schema: public; Owner: postgres
--

CREATE FUNCTION public.update_stxo() RETURNS void
    LANGUAGE plpgsql
    AS $$
    DECLARE max_blk_height integer;
    DECLARE max_saved_height integer;
BEGIN
    max_blk_height = (select max(height) from blk where orphan=false);
    max_saved_height = (select max(height) from stxo);
    insert into stxo SELECT * from v_stxo where height<=(max_blk_height - 10) and height>max_saved_height;
END;
$$;


ALTER FUNCTION public.update_stxo() OWNER TO postgres;

SET default_tablespace = '';

SET default_with_oids = false;

--
-- Name: addr; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.addr (
    id integer NOT NULL,
    address text NOT NULL,
    hash160 text NOT NULL,
    balance bigint DEFAULT 0,
    recv_value bigint DEFAULT 0,
    recv_count integer DEFAULT 0,
    spent_value bigint DEFAULT 0,
    spent_count integer DEFAULT 0,
    group_id integer,
    new_group_id integer
)
WITH (autovacuum_enabled='on');


ALTER TABLE public.addr OWNER TO postgres;

--
-- Name: addr_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.addr_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.addr_id_seq OWNER TO postgres;

--
-- Name: addr_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.addr_id_seq OWNED BY public.addr.id;


--
-- Name: addr_tx; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.addr_tx (
    addr_id integer NOT NULL,
    tx_id integer NOT NULL
);


ALTER TABLE public.addr_tx OWNER TO postgres;

--
-- Name: blk; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.blk (
    id integer NOT NULL,
    hash bytea NOT NULL,
    height integer NOT NULL,
    version bigint NOT NULL,
    prev_hash bytea NOT NULL,
    mrkl_root bytea NOT NULL,
    "time" bigint NOT NULL,
    bits bigint NOT NULL,
    nonce bigint NOT NULL,
    blk_size integer NOT NULL,
    work bytea,
    total_in_count integer,
    total_in_value bigint,
    fees bigint,
    total_out_count integer,
    total_out_value bigint,
    tx_count integer,
    pool_id integer,
    recv_time bigint,
    pool_bip integer,
    orphan boolean DEFAULT false,
    ip text
);


ALTER TABLE public.blk OWNER TO postgres;

--
-- Name: blk_tx; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.blk_tx (
    blk_id integer NOT NULL,
    tx_id integer NOT NULL,
    idx integer NOT NULL
);


ALTER TABLE public.blk_tx OWNER TO postgres;

--
-- Name: addr_tx_confirmed; Type: VIEW; Schema: public; Owner: postgres
--

CREATE VIEW public.addr_tx_confirmed AS
 SELECT a.tx_id,
    a.addr_id
   FROM ((public.addr_tx a
     JOIN public.blk_tx b ON ((b.tx_id = a.tx_id)))
     LEFT JOIN public.blk c ON (((c.id = b.blk_id) AND (c.orphan = false))));


ALTER TABLE public.addr_tx_confirmed OWNER TO postgres;

--
-- Name: tx; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.tx (
    id integer NOT NULL,
    hash bytea NOT NULL,
    version bigint NOT NULL,
    lock_time bigint NOT NULL,
    coinbase boolean NOT NULL,
    tx_size integer NOT NULL,
    in_count integer,
    in_value bigint,
    out_count integer,
    out_value bigint,
    fee bigint,
    recv_time bigint,
    ip text,
    removed boolean DEFAULT false,
    metatype integer,
    wtxid bytea,
    wsize integer,
    vsize integer
);


ALTER TABLE public.tx OWNER TO postgres;

--
-- Name: addr_tx_normal; Type: VIEW; Schema: public; Owner: postgres
--

CREATE VIEW public.addr_tx_normal AS
 SELECT a.tx_id,
    a.addr_id
   FROM (public.addr_tx a
     JOIN public.tx b ON (((b.id = a.tx_id) AND (b.removed = false))));


ALTER TABLE public.addr_tx_normal OWNER TO postgres;

--
-- Name: addr_tx_removed; Type: VIEW; Schema: public; Owner: postgres
--

CREATE VIEW public.addr_tx_removed AS
 SELECT a.tx_id,
    a.addr_id
   FROM (public.addr_tx a
     JOIN public.tx b ON (((b.id = a.tx_id) AND (b.removed = true))));


ALTER TABLE public.addr_tx_removed OWNER TO postgres;

--
-- Name: utx; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.utx (
    id integer NOT NULL
);


ALTER TABLE public.utx OWNER TO postgres;

--
-- Name: addr_tx_unconfirmed; Type: VIEW; Schema: public; Owner: postgres
--

CREATE VIEW public.addr_tx_unconfirmed AS
 SELECT a.tx_id,
    a.addr_id
   FROM (public.addr_tx a
     JOIN public.utx b ON ((b.id = a.tx_id)));


ALTER TABLE public.addr_tx_unconfirmed OWNER TO postgres;

--
-- Name: addr_txout; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.addr_txout (
    addr_id integer NOT NULL,
    txout_id integer NOT NULL
);


ALTER TABLE public.addr_txout OWNER TO postgres;

--
-- Name: txin; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.txin (
    id integer NOT NULL,
    tx_id integer NOT NULL,
    tx_idx integer NOT NULL,
    prev_out_index bigint NOT NULL,
    sequence bigint NOT NULL,
    script_sig bytea,
    prev_out bytea,
    witness text
);


ALTER TABLE public.txin OWNER TO postgres;

--
-- Name: txout; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.txout (
    id integer NOT NULL,
    tx_id integer NOT NULL,
    tx_idx integer NOT NULL,
    pk_script bytea NOT NULL,
    value bigint,
    type integer NOT NULL
);


ALTER TABLE public.txout OWNER TO postgres;

--
-- Name: all_vout; Type: VIEW; Schema: public; Owner: postgres
--

CREATE VIEW public.all_vout AS
 SELECT g.address,
    g.id AS addr_id,
    a.id AS txout_id,
    c.id AS txin_id,
    e.id AS txin_tx_id,
    b.id AS txout_tx_id,
    a.value,
    a.tx_idx AS out_idx,
    c.tx_idx AS in_idx,
    e.hash AS txin_tx_hash,
    b.hash AS txout_tx_hash
   FROM (((((public.txout a
     JOIN public.tx b ON ((b.id = a.tx_id)))
     LEFT JOIN public.txin c ON (((c.prev_out = b.hash) AND (c.prev_out_index = a.tx_idx))))
     LEFT JOIN public.tx e ON ((e.id = c.tx_id)))
     LEFT JOIN public.addr_txout f ON ((f.txout_id = a.id)))
     LEFT JOIN public.addr g ON ((g.id = f.addr_id)));


ALTER TABLE public.all_vout OWNER TO postgres;

--
-- Name: vout; Type: VIEW; Schema: public; Owner: postgres
--

CREATE VIEW public.vout AS
 SELECT g.address,
    g.id AS addr_id,
    a.id AS txout_id,
    c.id AS txin_id,
    e.id AS txin_tx_id,
    b.id AS txout_tx_id,
    a.value,
    a.tx_idx AS out_idx,
    c.tx_idx AS in_idx,
    e.hash AS txin_tx_hash,
    b.hash AS txout_tx_hash,
    a.type AS txout_type
   FROM (((((public.txout a
     LEFT JOIN public.tx b ON ((b.id = a.tx_id)))
     LEFT JOIN public.txin c ON (((c.prev_out = b.hash) AND (c.prev_out_index = a.tx_idx))))
     LEFT JOIN public.tx e ON ((e.id = c.tx_id)))
     LEFT JOIN public.addr_txout f ON ((f.txout_id = a.id)))
     LEFT JOIN public.addr g ON ((g.id = f.addr_id)));


ALTER TABLE public.vout OWNER TO postgres;

--
-- Name: balance; Type: VIEW; Schema: public; Owner: postgres
--

CREATE VIEW public.balance AS
 SELECT vout.addr_id,
    sum(vout.value) AS value
   FROM public.vout
  WHERE (vout.txin_id IS NULL)
  GROUP BY vout.addr_id;


ALTER TABLE public.balance OWNER TO postgres;

--
-- Name: bip; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.bip (
    id integer NOT NULL,
    name text NOT NULL,
    link text
);


ALTER TABLE public.bip OWNER TO postgres;

--
-- Name: bip_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.bip_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.bip_id_seq OWNER TO postgres;

--
-- Name: bip_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.bip_id_seq OWNED BY public.bip.id;


--
-- Name: bitcoin_stat; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.bitcoin_stat (
    value bigint,
    create_time numeric,
    blk_count bigint,
    blk_size bigint,
    total_in_count bigint,
    total_in_value numeric,
    total_out_count bigint,
    total_out_value numeric,
    tx_count bigint,
    fees numeric
);


ALTER TABLE public.bitcoin_stat OWNER TO postgres;

--
-- Name: blk_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.blk_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.blk_id_seq OWNER TO postgres;

--
-- Name: blk_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.blk_id_seq OWNED BY public.blk.id;


--
-- Name: blk_stat; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.blk_stat (
    id integer NOT NULL,
    "timestamp" timestamp without time zone DEFAULT now(),
    max_height integer,
    total_tx_count integer,
    max_blk_id integer,
    max_tx_id integer,
    sum_blk_size bigint,
    sum_tx_size bigint
);


ALTER TABLE public.blk_stat OWNER TO postgres;

--
-- Name: blk_stat_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.blk_stat_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.blk_stat_id_seq OWNER TO postgres;

--
-- Name: blk_stat_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.blk_stat_id_seq OWNED BY public.blk_stat.id;


--
-- Name: chart_tx; Type: VIEW; Schema: public; Owner: postgres
--

CREATE VIEW public.chart_tx AS
 SELECT a.id,
    a.tx_size,
    a.fee,
    a.recv_time,
    c.recv_time AS confirm_recv_time,
    c."time",
    a.metatype
   FROM ((public.tx a
     LEFT JOIN public.blk_tx b ON ((b.tx_id = a.id)))
     LEFT JOIN public.blk c ON ((c.id = b.blk_id)))
  WHERE ((a.recv_time > 0) AND (c.recv_time > 0) AND (a.id > 152350000) AND (a.id <= 152400000));


ALTER TABLE public.chart_tx OWNER TO postgres;

--
-- Name: daily_stat; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.daily_stat (
    "time" integer,
    js json
);


ALTER TABLE public.daily_stat OWNER TO postgres;

--
-- Name: json_cache; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.json_cache (
    key text,
    val text
);


ALTER TABLE public.json_cache OWNER TO postgres;

--
-- Name: mchart_tx; Type: MATERIALIZED VIEW; Schema: public; Owner: postgres
--

CREATE MATERIALIZED VIEW public.mchart_tx AS
 SELECT a.id,
    a.tx_size,
    a.fee,
    a.recv_time,
    c.recv_time AS confirm_recv_time,
    c."time",
    a.metatype
   FROM ((public.tx a
     LEFT JOIN public.blk_tx b ON ((b.tx_id = a.id)))
     LEFT JOIN public.blk c ON ((c.id = b.blk_id)))
  WHERE ((a.recv_time > 0) AND (c.recv_time > 0) AND (a.id > 152350000) AND (a.id <= 152400000))
  WITH NO DATA;


ALTER TABLE public.mchart_tx OWNER TO postgres;

--
-- Name: mempool; Type: TABLE; Schema: public; Owner: postgres
--

CREATE UNLOGGED TABLE public.mempool (
    tx_id integer,
    hash bytea,
    entrypriority double precision,
    nfee bigint,
    inchaininputvalue bigint,
    ntxsize integer,
    ntime bigint,
    entryheight integer,
    hadnodependencies boolean,
    sigopcount integer,
    modifiedfee bigint,
    nmodsize integer,
    nusagesize integer,
    dirty boolean,
    ncountwithdescendants bigint,
    nsizewithdescendants bigint,
    nmodfeeswithdescendants bigint,
    spendscoinbase boolean
);


ALTER TABLE public.mempool OWNER TO postgres;

--
-- Name: meta; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.meta (
    id integer NOT NULL,
    name text NOT NULL,
    link text
);


ALTER TABLE public.meta OWNER TO postgres;

--
-- Name: metatype_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.metatype_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.metatype_id_seq OWNER TO postgres;

--
-- Name: metatype_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.metatype_id_seq OWNED BY public.meta.id;


--
-- Name: pool; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.pool (
    id integer NOT NULL,
    name text NOT NULL,
    link text
);


ALTER TABLE public.pool OWNER TO postgres;

--
-- Name: pool_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.pool_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.pool_id_seq OWNER TO postgres;

--
-- Name: pool_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.pool_id_seq OWNED BY public.pool.id;


--
-- Name: stxo; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.stxo (
    address text,
    addr_id integer,
    txout_id integer,
    txin_id integer,
    txin_tx_id integer,
    txout_tx_id integer,
    value bigint,
    out_idx integer,
    in_idx integer,
    txout_tx_hash bytea,
    txin_tx_hash bytea,
    height integer,
    "time" bigint
);


ALTER TABLE public.stxo OWNER TO postgres;

--
-- Name: tx_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.tx_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.tx_id_seq OWNER TO postgres;

--
-- Name: tx_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.tx_id_seq OWNED BY public.tx.id;


--
-- Name: txin_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.txin_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.txin_id_seq OWNER TO postgres;

--
-- Name: txin_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.txin_id_seq OWNED BY public.txin.id;


--
-- Name: txout_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.txout_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.txout_id_seq OWNER TO postgres;

--
-- Name: txout_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.txout_id_seq OWNED BY public.txout.id;


--
-- Name: txout_type; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.txout_type (
    id integer NOT NULL,
    name text NOT NULL
);


ALTER TABLE public.txout_type OWNER TO postgres;

--
-- Name: txout_type_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.txout_type_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE public.txout_type_id_seq OWNER TO postgres;

--
-- Name: txout_type_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.txout_type_id_seq OWNED BY public.txout_type.id;


--
-- Name: utx_old; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.utx_old (
    id integer
);


ALTER TABLE public.utx_old OWNER TO postgres;

--
-- Name: utxo; Type: VIEW; Schema: public; Owner: postgres
--

CREATE VIEW public.utxo AS
 SELECT g.address,
    g.id AS addr_id,
    a.id AS txout_id,
    c.id AS txin_id,
    e.id AS txin_tx_id,
    b.id AS txout_tx_id,
    b.hash AS txout_txhash,
    a.value,
    a.tx_idx,
    blk.height,
    blk."time",
    a.pk_script
   FROM (((((((public.txout a
     JOIN public.tx b ON ((b.id = a.tx_id)))
     LEFT JOIN public.txin c ON (((c.prev_out = b.hash) AND (c.prev_out_index = a.tx_idx))))
     JOIN public.tx e ON ((e.id = c.tx_id)))
     LEFT JOIN public.addr_txout f ON ((f.txout_id = a.id)))
     LEFT JOIN public.addr g ON ((g.id = f.addr_id)))
     JOIN public.blk_tx ON ((blk_tx.tx_id = a.tx_id)))
     JOIN public.blk ON (((blk.id = blk_tx.blk_id) AND (blk.orphan <> true))))
  WHERE (c.id IS NULL);


ALTER TABLE public.utxo OWNER TO postgres;

--
-- Name: v_blk; Type: VIEW; Schema: public; Owner: postgres
--

CREATE VIEW public.v_blk AS
 SELECT a.id,
    a.hash,
    a.height,
    a.version,
    a.prev_hash,
    a.mrkl_root,
    a."time",
    a.bits,
    a.nonce,
    a.blk_size,
    a.work,
    a.total_in_count,
    a.total_in_value,
    a.fees,
    a.total_out_count,
    a.total_out_value,
    a.tx_count,
    a.pool_id,
    a.recv_time,
    a.pool_bip,
    a.orphan,
    b.name AS pool_name,
    b.link AS pool_link,
    c.name AS bip_name,
    c.link AS bip_link
   FROM ((public.blk a
     LEFT JOIN public.pool b ON ((a.pool_id = b.id)))
     LEFT JOIN public.bip c ON ((a.pool_bip = c.id)))
  ORDER BY a.height DESC;


ALTER TABLE public.v_blk OWNER TO postgres;

--
-- Name: v_stxo; Type: VIEW; Schema: public; Owner: postgres
--

CREATE VIEW public.v_stxo AS
 SELECT g.address,
    g.id AS addr_id,
    a.id AS txout_id,
    h.txin_id,
    h.txin_tx_id,
    b.id AS txout_tx_id,
    a.value,
    a.tx_idx AS out_idx,
    h.in_idx,
    b.hash AS txout_tx_hash,
    h.txin_tx_hash,
    blk.height,
    blk."time"
   FROM ((((((public.txout a
     LEFT JOIN public.tx b ON (((b.id = a.tx_id) AND (b.removed = false))))
     LEFT JOIN ( SELECT c.prev_out,
            c.prev_out_index,
            c.id AS txin_id,
            c.tx_idx AS in_idx,
            e.id AS txin_tx_id,
            e.hash AS txin_tx_hash
           FROM (public.txin c
             JOIN public.tx e ON (((e.id = c.tx_id) AND (e.removed = false))))) h ON (((h.prev_out = b.hash) AND (h.prev_out_index = a.tx_idx))))
     LEFT JOIN public.addr_txout f ON ((f.txout_id = a.id)))
     LEFT JOIN public.addr g ON ((g.id = f.addr_id)))
     JOIN public.blk_tx ON ((blk_tx.tx_id = a.tx_id)))
     JOIN public.blk ON ((blk.id = blk_tx.blk_id)))
  WHERE (h.txin_id IS NOT NULL);


ALTER TABLE public.v_stxo OWNER TO postgres;

--
-- Name: vin; Type: VIEW; Schema: public; Owner: postgres
--

CREATE VIEW public.vin AS
 SELECT g.address,
    g.group_id AS addr_group_id,
    a.id AS txout_id,
    c.id AS txin_id,
    e.id AS txin_tx_id,
    b.id AS txout_tx_id,
    a.value,
    a.tx_idx AS out_idx,
    c.tx_idx AS in_idx,
    e.hash AS txin_tx_hash,
    b.hash AS txout_tx_hash,
    b.tx_size,
    b.fee
   FROM ((((((public.txout a
     LEFT JOIN public.tx b ON ((b.id = a.tx_id)))
     LEFT JOIN public.txin c ON (((c.prev_out = b.hash) AND (c.prev_out_index = a.tx_idx))))
     LEFT JOIN public.tx e ON ((e.id = c.tx_id)))
     LEFT JOIN public.addr_txout f ON ((f.txout_id = a.id)))
     LEFT JOIN public.addr g ON ((g.id = f.addr_id)))
     RIGHT JOIN public.utx h ON (((h.id = b.id) OR (h.id = e.id))));


ALTER TABLE public.vin OWNER TO postgres;

--
-- Name: vtx; Type: VIEW; Schema: public; Owner: postgres
--

CREATE VIEW public.vtx AS
 SELECT a.id,
    a.hash,
    a.version,
    a.lock_time,
    a.coinbase,
    a.tx_size,
    a.in_count,
    a.in_value,
    a.out_count,
    a.out_value,
    a.fee,
    a.recv_time,
    a.ip,
    a.removed,
    a.metatype,
    a.wtxid,
    a.wsize,
    a.vsize,
    b.idx,
    c.height,
    c."time"
   FROM ((public.tx a
     LEFT JOIN public.blk_tx b ON ((b.tx_id = a.id)))
     LEFT JOIN public.blk c ON ((c.id = b.blk_id)));


ALTER TABLE public.vtx OWNER TO postgres;

--
-- Name: addr id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.addr ALTER COLUMN id SET DEFAULT nextval('public.addr_id_seq'::regclass);


--
-- Name: bip id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.bip ALTER COLUMN id SET DEFAULT nextval('public.bip_id_seq'::regclass);


--
-- Name: blk id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.blk ALTER COLUMN id SET DEFAULT nextval('public.blk_id_seq'::regclass);


--
-- Name: blk_stat id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.blk_stat ALTER COLUMN id SET DEFAULT nextval('public.blk_stat_id_seq'::regclass);


--
-- Name: meta id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.meta ALTER COLUMN id SET DEFAULT nextval('public.metatype_id_seq'::regclass);


--
-- Name: pool id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.pool ALTER COLUMN id SET DEFAULT nextval('public.pool_id_seq'::regclass);


--
-- Name: tx id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.tx ALTER COLUMN id SET DEFAULT nextval('public.tx_id_seq'::regclass);


--
-- Name: txin id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.txin ALTER COLUMN id SET DEFAULT nextval('public.txin_id_seq'::regclass);


--
-- Name: txout id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.txout ALTER COLUMN id SET DEFAULT nextval('public.txout_id_seq'::regclass);


--
-- Name: txout_type id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.txout_type ALTER COLUMN id SET DEFAULT nextval('public.txout_type_id_seq'::regclass);


--
-- Name: blk blk_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.blk
    ADD CONSTRAINT blk_pkey PRIMARY KEY (id);


--
-- Name: blk_stat blk_stat_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.blk_stat
    ADD CONSTRAINT blk_stat_pkey PRIMARY KEY (id);


--
-- Name: pool constraint_name; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.pool
    ADD CONSTRAINT constraint_name UNIQUE (name);


--
-- Name: utx id; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.utx
    ADD CONSTRAINT id UNIQUE (id);


--
-- Name: addr naddr_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.addr
    ADD CONSTRAINT naddr_pkey PRIMARY KEY (id);


--
-- Name: pool pool_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.pool
    ADD CONSTRAINT pool_pkey PRIMARY KEY (id);


--
-- Name: tx tx_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.tx
    ADD CONSTRAINT tx_pkey PRIMARY KEY (id);


--
-- Name: txin txin_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.txin
    ADD CONSTRAINT txin_pkey PRIMARY KEY (id);


--
-- Name: txout txout_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.txout
    ADD CONSTRAINT txout_pkey PRIMARY KEY (id);


--
-- Name: addr_tx u_constrainte; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.addr_tx
    ADD CONSTRAINT u_constrainte UNIQUE (addr_id, tx_id);


--
-- Name: addr uniq_addr_address; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.addr
    ADD CONSTRAINT uniq_addr_address UNIQUE (address);


--
-- Name: blk uniq_blk_hash; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.blk
    ADD CONSTRAINT uniq_blk_hash UNIQUE (hash);


--
-- Name: tx uniq_tx_hash; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.tx
    ADD CONSTRAINT uniq_tx_hash UNIQUE (hash);


--
-- Name: addr_balance_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX addr_balance_index ON public.addr USING btree (balance);


--
-- Name: addr_recv_count_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX addr_recv_count_index ON public.addr USING btree (recv_count);


--
-- Name: addr_spent_count_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX addr_spent_count_index ON public.addr USING btree (spent_count);


--
-- Name: addr_txout_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE UNIQUE INDEX addr_txout_index ON public.addr_txout USING btree (addr_id, txout_id);


--
-- Name: addr_txout_txid_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX addr_txout_txid_index ON public.addr_txout USING btree (addr_id);


--
-- Name: blk_hash_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX blk_hash_index ON public.blk USING btree (hash);


--
-- Name: blk_height_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX blk_height_index ON public.blk USING btree (height);


--
-- Name: blk_prev_hash_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX blk_prev_hash_index ON public.blk USING btree (prev_hash);


--
-- Name: blk_tx_blk_id_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX blk_tx_blk_id_index ON public.blk_tx USING btree (blk_id);


--
-- Name: blk_tx_tx_id_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX blk_tx_tx_id_index ON public.blk_tx USING btree (tx_id);


--
-- Name: inaddr_txout_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX inaddr_txout_index ON public.addr_txout USING btree (txout_id);


--
-- Name: json_cache_key; Type: INDEX; Schema: public; Owner: postgres
--

CREATE UNIQUE INDEX json_cache_key ON public.json_cache USING btree (key);


--
-- Name: mempool_tx_id; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX mempool_tx_id ON public.mempool USING btree (tx_id);


--
-- Name: npi_tx_recv_time; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX npi_tx_recv_time ON public.tx USING btree (recv_time) WHERE ((id > 152400000) AND (id <= 152450000));


--
-- Name: pi_tx_recv_time; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX pi_tx_recv_time ON public.tx USING btree (recv_time) WHERE ((id > 152350000) AND (id <= 152400000));


--
-- Name: stxo_addr_id_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX stxo_addr_id_index ON public.stxo USING btree (addr_id);


--
-- Name: stxo_height_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX stxo_height_index ON public.stxo USING btree (height);


--
-- Name: stxo_time_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX stxo_time_index ON public.stxo USING btree ("time");


--
-- Name: stxo_txin_tx_id_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX stxo_txin_tx_id_index ON public.stxo USING btree (txin_tx_id);


--
-- Name: stxo_txout_id_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX stxo_txout_id_index ON public.stxo USING btree (txout_id);


--
-- Name: stxo_txout_tx_id_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX stxo_txout_tx_id_index ON public.stxo USING btree (txout_tx_id);


--
-- Name: stxo_value_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX stxo_value_index ON public.stxo USING btree (value);


--
-- Name: tx_recv_time_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX tx_recv_time_index ON public.tx USING btree (recv_time);


--
-- Name: tx_removed_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX tx_removed_index ON public.tx USING btree (removed);


--
-- Name: txin_prev_out_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX txin_prev_out_index ON public.txin USING btree (prev_out);


--
-- Name: txin_txid_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX txin_txid_index ON public.txin USING btree (tx_id);


--
-- Name: txout_txid_idx_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX txout_txid_idx_index ON public.txout USING btree (tx_id, tx_idx);


--
-- Name: txout_txid_index; Type: INDEX; Schema: public; Owner: postgres
--

CREATE INDEX txout_txid_index ON public.txout USING btree (tx_id);


--
-- Name: addr_tx addr_tx_on_duplicate_ignore; Type: RULE; Schema: public; Owner: postgres
--

CREATE RULE addr_tx_on_duplicate_ignore AS
    ON INSERT TO public.addr_tx
   WHERE (EXISTS ( SELECT 1
           FROM public.addr_tx
          WHERE ((addr_tx.addr_id = new.addr_id) AND (addr_tx.tx_id = new.tx_id)))) DO INSTEAD NOTHING;


--
-- Name: addr_txout addr_txout_on_duplicate_ignore; Type: RULE; Schema: public; Owner: postgres
--

CREATE RULE addr_txout_on_duplicate_ignore AS
    ON INSERT TO public.addr_txout
   WHERE (EXISTS ( SELECT 1
           FROM public.addr_txout
          WHERE ((addr_txout.addr_id = new.addr_id) AND (addr_txout.txout_id = new.txout_id)))) DO INSTEAD NOTHING;


--
-- Name: utx utx_on_duplicate_ignore; Type: RULE; Schema: public; Owner: postgres
--

CREATE RULE utx_on_duplicate_ignore AS
    ON INSERT TO public.utx
   WHERE (EXISTS ( SELECT 1
           FROM public.utx
          WHERE (utx.id = new.id))) DO INSTEAD NOTHING;


--
-- PostgreSQL database dump complete
--

