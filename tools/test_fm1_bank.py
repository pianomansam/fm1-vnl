#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""fm1_bank.py's layout check: the shipped bank passes; one with "AT" on an extra window's sector's 0xF06 (either
area) or outgrowing an area is refused.   python3 -I tools/test_fm1_bank.py"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fm1_bank as B  # noqa: E402

bank = B.load(B.DEFAULT)
split = B.split_of(B.DEFAULT)
assert [at for at, _ in B.placed(bank, split)] == [B.AREA0, B.AREA1]


def refused(b, s):
    try:
        B.placed(b, s)
    except ValueError:
        return True
    return False


for at in (B.AREA0 + 0x2000 + 0xF06, B.AREA1 + 0x3000 + 0xF06):   # in XDATA0, in XDATA1
    o = at - B.AREA0 if at < B.AREA1 else B.HEAD + split + at - B.AREA1
    assert refused(bank[:o] + b"AT" + bank[o + 2:], split), hex(at)
o = 0x5000 + 0xF06                                                  # the main data area: no rule there
assert not refused(bank[:o] + b"AT" + bank[o + 2:], split)
assert refused(bank, split + 0x20000)                               # a first part past its area
print("test_fm1_bank: ok")
