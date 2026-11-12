#!/usr/bin/env python3
"""diffparse.hpp reads a bare number from the token itself: a decimal integer >= 2^63 followed on the same line by a float
(`"ret_xmm0": {"f": 0.0}`) is an integer (it was parsed through strtod, a double cast to int64: undefined, 0x8000000000000000
on x86, which made 0x140227E70's case 34 a false "translator bug"); a float token is still read as a float and a hex token's
e/E are digits."""
import os
import subprocess
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = r'''
#include "diffparse.hpp"
#include <cstdio>
int main() {
  const char* cases[] = {"18446744073709551612, \"ret_xmm0\": {\"bits\": \"0x0\", \"f\": 0.0}}",
                         "252, \"f\": 0.5e3}", "0xFE, \"f\": 1.0}", "2.5, \"x\": 1}", "-74, \"f\": 0.0}"};
  for (const char* c : cases) { JVal v; parse_val(c, v); std::printf("%lld\n", (long long)v.n); }
}
'''


class DiffparseToken(unittest.TestCase):
    def test_numbers_are_read_from_their_own_token(self):
        with tempfile.TemporaryDirectory() as d:
            src, exe = os.path.join(d, "t.cpp"), os.path.join(d, "t")
            with open(src, "w") as f:
                f.write(SRC)
            subprocess.run(["g++", "-std=c++17", "-I", HERE, "-o", exe, src], check=True)
            out = subprocess.run([exe], check=True, capture_output=True, text=True).stdout.split()
        self.assertEqual([int(x) for x in out], [-4, 252, 254, 2, -74])


if __name__ == "__main__":
    unittest.main(verbosity=2)
