#!/usr/bin/env python

import argparse
import datetime as dt

from enum import Enum
from dataclasses import dataclass
from functools import reduce
from os import path

orig_methods = ['reference',
                'scalar',
                'simd',
                'predicated',
                'hybrid-stream',
                'hybrid-full-range']

render=('pt_const_b1_t0.0001_d2',
        'pt_const_b1_t0.0001_d20',
        'pt_const_b1_t0.0001_d5',
        'pt_const_b1_t0.0005_d2',
        'pt_const_b1_t0.0005_d20',
        'pt_const_b1_t0.0005_d5',
        'pt_const_b1_t0.001_d2',
        'pt_const_b1_t0.001_d20',
        'pt_const_b1_t0.001_d5',
        'pt_const_b20_t0.0001_d2',
        'pt_const_b20_t0.0001_d20',
        'pt_const_b20_t0.0001_d5',
        'pt_const_b20_t0.0005_d2',
        'pt_const_b20_t0.0005_d20',
        'pt_const_b20_t0.0005_d5',
        'pt_const_b20_t0.001_d2',
        'pt_const_b20_t0.001_d20',
        'pt_const_b20_t0.001_d5',
        'pt_const_b5_t0.0001_d2',
        'pt_const_b5_t0.0001_d20',
        'pt_const_b5_t0.0001_d5',
        'pt_const_b5_t0.0005_d2',
        'pt_const_b5_t0.0005_d20',
        'pt_const_b5_t0.0005_d5',
        'pt_const_b5_t0.001_d2',
        'pt_const_b5_t0.001_d20',
        'pt_const_b5_t0.001_d5',
        'pt2_const_b1_t0.0001_d2',
        'pt2_const_b1_t0.0001_d20',
        'pt2_const_b1_t0.0001_d5',
        'pt2_const_b1_t0.0005_d2',
        'pt2_const_b1_t0.0005_d20',
        'pt2_const_b1_t0.0005_d5',
        'pt2_const_b1_t0.001_d2',
        'pt2_const_b1_t0.001_d20',
        'pt2_const_b1_t0.001_d5',
        'pt2_const_b20_t0.0001_d2',
        'pt2_const_b20_t0.0001_d20',
        'pt2_const_b20_t0.0001_d5',
        'pt2_const_b20_t0.0005_d2',
        'pt2_const_b20_t0.0005_d20',
        'pt2_const_b20_t0.0005_d5',
        'pt2_const_b20_t0.001_d2',
        'pt2_const_b20_t0.001_d20',
        'pt2_const_b20_t0.001_d5',
        'pt2_const_b5_t0.0001_d2',
        'pt2_const_b5_t0.0001_d20',
        'pt2_const_b5_t0.0001_d5',
        'pt2_const_b5_t0.0005_d2',
        'pt2_const_b5_t0.0005_d20',
        'pt2_const_b5_t0.0005_d5',
        'pt2_const_b5_t0.001_d2',
        'pt2_const_b5_t0.001_d20',
        'pt2_const_b5_t0.001_d5')

def sm_prep(s) -> str:
    return s.replace('-', '_').upper()

Method = Enum('Method', [sm_prep(s) for s in orig_methods])

@dataclass(frozen=True)
class BenchmarkID:
    method: Method
    type: str = None
    sample_size: int = None
    ts: bool = False
    tbd: bool = True
    UTR: float = None
    stream_size: int = None

@dataclass(frozen=True)
class Record:
    date: dt.datetime
    hash: str
    compiler: str
    fast_math: bool
    cpu: str
    simd: str
    id: BenchmarkID
    time: float

def get_size(s) -> int | None:
    return int(s) if s.isdecimal() else None

def get_type(s) -> str | None:
    return s if s in types else None

def parse_bool(s) -> bool:
    return s == 'true'

def get_prop(s, prop, parser):
    l = s.split(':')
    if len(l) != 2:
        return None

    p, v = l
    if p != prop:
        return None

    return parser(v)

def parse_attrs(strings, p):
    return next((v for v in map(p, strings) if v is not None), None)

def make_bid(l) -> BenchmarkID:
    types = ('f32', 'f64', 'u32', 'u64')
    ss = l.split(';')
    tail = ss[1:]
    extract_prop = lambda prop, parser: parse_attrs(tail, lambda s: get_prop(s, prop, parser))

    return BenchmarkID(method=Method[sm_prep(ss[0])],
                       type=parse_attrs(tail, lambda s: s if s in types else None),
                       sample_size=parse_attrs(tail, get_size),
                       ts=extract_prop('ts', parse_bool),
                       tbd=extract_prop('tbd', parse_bool),
                       UTR=extract_prop('UTR', lambda s: float(s[:-1]) / 100.0),
                       stream_size=extract_prop('size', int))

def parse_record(line) -> Record:
    vals = line.split(',')
    return Record(date=dt.datetime.strptime(vals[0], "%Y-%m-%dT%H:%M:%S%z"),
                  hash=vals[1],
                  compiler=vals[2],
                  fast_math=vals[3].split(':')[1] == "true",
                  cpu=vals[4],
                  simd=vals[5],
                  id= make_bid(vals[6]),
                  time=float(vals[7]))

def get_records(f):
    return [parse_record(l) for l in f.readlines()]

def latest(records):
    lh = records[-1].hash
    return filter(lambda r: r.hash == lh, records)

def get_unique_bids(records):
    return set(r.id for r in records)

def avg_of(bid: BenchmarkID, records):
    sl = [r.time for r in filter(lambda r: r.id == bid, records)]
    return sum(sl) / len(sl)

def best_of_methods(bids, records):
    return ((bid, avg_of(bid, records)) for bid in bids)

def h2p_speedup(records):
    ubids = get_unique_bids(records)
    hybrid_bids = filter(lambda bid:
                         bid.method == Method.HYBRID_STREAM or
                         bid.method == Method.HYBRID_FULL_RANGE,
                         ubids)
    pred_bid = next(filter(lambda bid: bid.method == Method.PREDICATED, ubids))
    hbid, hval = reduce(lambda l, r: l if l[1] < r[1] else r,
                       best_of_methods(hybrid_bids, records))
    pval = avg_of(pred_bid, records)
    return pval / hval

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('path', help='Path to directory with benchmarks logs')
    args = parser.parse_args()

    d = path.abspath(args.path)

    xs = [h2p_speedup(get_records(open(p, 'r'))) for p in
          filter(lambda p: path.exists(p),
                 (path.join(d, n + '.csv') for n in render))]

    max_speedup = max(xs)
    min_speedup = min(xs)
    avg_speedup = sum(xs) / len(xs)

    print("Maximum speedup: {:.2f}x\nAverage speedup: {:.2f}x\nMinimum speedup: {:2f}x".format(max_speedup, avg_speedup, min_speedup))

if __name__ == '__main__':
    main()
