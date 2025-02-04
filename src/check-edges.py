#!/usr/bin/env python3

# Fan parameters
PWM_FREQ = 25_000
TACH_PULSES_PER_REV = 2
FAN_MAX_SPEED = 5000

# MCU parameters
CLK_FREQ = 80_000_000
PWM_BITS = 11
PWM_RESOLUTION = 2**PWM_BITS


from collections import namedtuple
from enum import IntEnum
import numpy as np
import re


class Channel(IntEnum):
    PWM = 1
    TACH = 0

class Dir(IntEnum):
    RISING = 1
    FALLING = 0


class Signal(namedtuple('Signal', 'name, chan, dir, clk')):
    
    def rising_edges(self):
        print(f'{self.dir=}')
        print(f'{self.dir == Dir.RISING=}')
        print(f'{Dir.RISING=}')
        print(f'{1 == Dir.RISING=}')
        print(f'{np.any(self.dir == Dir.RISING)=}')
        return self.clk[self.dir == Dir.RISING]
    
    def falling_edges(self):
        return self.clk[self.dir == Dir.FALLING]


class Scenario:

    def __init__(self, file, start_sec = 0):
        self.filename = file
        self._read(file, start_clk=int(start_sec * CLK_FREQ))

    def _read(self, file, start_clk=0):
        contents = open(file).read()
        m = re.search(r'.* scenario', contents)
        assert m, f"can't find scenario description in {file}"
        self.description = m.group(0)
        m = re.search(r'(\d+) events logged', contents)
        assert m, f"can't find event count in {file}"
        self.edge_count = int(m.group(1))

        hex_data = contents[m.span()[1]:]
        hex_edges = re.findall(r'[\da-f]+', hex_data)
        assert len(hex_edges) == self.edge_count
        int_edges = [int(x, base=16) for x in hex_edges]
        self.raw_edges = np.array(int_edges, dtype=np.uint32)

        edges = self._unwrap(self.raw_edges)
        # N.B. edges are now (signed) int64.

        # Yes, really.  We captured some of the edges out of order
        # due to software interrupt latency. 
        edges.sort()    # sorts in place

        chan, dir, clk = self._decode(edges)

        offset = clk[0] - start_clk
        print(f'{offset=}')

        pwm_edges = edges[chan == 1]
        tach_edges = edges[chan == 0]
        self.pwm = Signal('pwm', *self._decode(pwm_edges, offset=offset))
        self.tach = Signal('tach', *self._decode(tach_edges, offset=offset))

    def _unwrap(self, edges):
        """Timestamps wrap around.  Convert to unwrapped."""
        # This function will preserve the flags in the low bits.
        # N.B., sometimes edges are out of order, so the data
        # may wrap backward as well as forward.
        edges = np.array(edges, dtype=np.int64)
        delta = edges[1:] - edges[:-1]
        overflow = delta < -2**31
        underflow = delta > +2**31
        # print(f'{len(overflow)=}')
        # print(f'{overflow.nonzero()[0]=}')
        # print(f'{underflow.nonzero()[0]=}')
        eitherflow = overflow.astype(np.int64) - underflow.astype(np.int64)
        adjustment = 2**32 * eitherflow.cumsum()
        adjustment = np.hstack([[0], adjustment])
        return edges + adjustment

        return edges

    def _decode(self, edges, offset=0):
        # Each edge is 32 bits and encodes
        #  - a channel (tach = 0, PWM = 1)
        #  - a direction (falling = 0, rising = 1)
        #  - a 30 bit timestamp
        chan = edges & 1
        dir = edges >> 1 & 1
        clk = ((edges >> 2) - offset) # & 0x3FFF_FFFF
        return (chan, dir, clk)

    def __repr__(self):
        return f'<{self.__class__.__name__}: {self.description}>'

    def __len__(self):
        return self.edge_count;

# Quick unwrap test
edges = np.array([0xFFFFFFFE, 0, 1, 0xFFFFFFFF,
                  1, 2, 0x7FFFFFFF, 0xFFFF0000, 3], dtype=np.uint32)
unwrapped = Scenario._unwrap(None, edges)
assert list(unwrapped) == [0xFFFFFFFE, 0x100000000, 0x100000001, 0xFFFFFFFF,
                           0x100000001, 0x100000002, 0x17FFFFFFF, 0x1FFFF0000,
                           0x200000003]


def verify_alternating_edges(scenario):
    print(f'Verifying {scenario.description}')
    for sig in [scenario.pwm, scenario.tach]:
        dir = sig.dir
        if len(dir):
            try:
                assert np.all(dir[::2] == dir[0])
                assert np.all(dir[1::2] != dir[0])
            except AssertionError:
                d0 = int(dir[0])
                anomalies = (dir != (d0 + np.arange(len(dir))) % 2).nonzero()[0]
                print(f'{sig.name}: {len(anomalies)=}')
                print(f'{sig.dir.shape=}')
                print(f'{sig.dir.sum()=}')
                print(f'{anomalies=}')
                print(f'{sig.clk=}')
                for i in anomalies[:3]:
                    neighbors = np.arange(max(0, i - 4), i + 5)
                    n_clk = sig.clk[neighbors]
                    n_delta = n_clk[1:] - n_clk[:-1]
                    print(f'{scenario.description}: {sig.name}')
                    print(f'    at {i}')
                    print(f'    dir {dir[neighbors]}')
                    print(f'    clk {n_clk}')
                    print(f'    clk \u0394 {n_delta}')
                delta = sig.clk[1:] - sig.clk[:-1]
                print(f'{sig.name}: {np.histogram(delta)=}')
                # raise
            

def check_ordering(scenario):
    print(f'Checking edge order in {scenario.description}')
    for sig in [scenario.pwm, scenario.tach]:
        dir = sig.dir
        # print(f'{sig.name}')
        if len(dir) == 0:
            continue
        delta = sig.clk[1:] - sig.clk[:-1]
        neg_indices = (delta < 0).nonzero()[0]
        if len(neg_indices):
            # print(f'{len(sig.clk)=}')
            print(f'{sig.name}: {len(neg_indices)} out of order sample(s)')
            print(f'{sig.name}: {neg_indices=}')
            # print(f'{sig.name}: {len(neg_indices)=}')
            print(f'{sig.name}: {min(delta[neg_indices])=}')
            # print(f'{sig.clk[591080:591096]=}')
            # print()


def main(argv):
    for file in argv[1:]:
        s = Scenario(file)
        check_ordering(s)
        verify_alternating_edges(s)

if __name__ == '__main__':
    import sys
    sys.exit(main(sys.argv))