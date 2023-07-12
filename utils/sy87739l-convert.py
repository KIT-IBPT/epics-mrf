#!/usr/bin/env python3

# Copyright 2020 aquenos GmbH.
# Copyright 2020 Karlsruhe Institute of Technology.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program.  If not, see
# <http://www.gnu.org/licenses/>.
#
# This software has been developed by aquenos GmbH on behalf of the
# Karlsruhe Institute of Technology's Institute for Beam Physics and
# Technology.
#
# The algorithms used by the cw_to_freq and freq_to_cw functions have
# been derived from C code originally written by Jukka Pietarinen.


import argparse
import math
import sys


table_post_div_sel = [1, 3, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
                      15, 16, 18, 20, 22, 24, 26, 28, 30, 32, 36, 40,
                      44, 48, 52, 56, 60]

table_mdiv_sel = [16, 16, 18, 17, 31, 14, 32, 15]

ref_osc = 24.000


def cw_to_freq(cw):
    '''
    Convert a configuration word to a frequency.

    This is identical to Jukka's C program, with the exception that there are a
    few additional checks to verify that the given configuration word actually
    represents a valid configuration.
    '''
    qp = ((cw & 0x0f800000) >> 23)
    qpm1 = ((cw & 0x007c0000) >> 18)
    div_sel = ((cw & 0x0003c000) >> 14) + 17
    post_div_sel = table_post_div_sel[((cw & 0x000007c0) >> 6)]
    mdiv_sel = table_mdiv_sel[cw & 0x00000007]
    ndiv_sel = table_mdiv_sel[(cw & 0x00000038) >> 3]
    if qpm1 + qp == 0:
        raise Exception('Qp1 and Qp-1 cannot both be zero')
    if qpm1 + qp > 31:
        raise Exception('Qp1 + Qp-1 must be <= 31')
    if ((mdiv_sel <= 18 and ndiv_sel >= 31)
            or (ndiv_sel <= 18 and mdiv_sel >= 31)
            or (ndiv_sel == 18 and mdiv_sel == 14)):
        raise Exception(
            'Invalid mdiv_sel %d, ndiv_sel %d combination.'
            % (mdiv_sel, ndiv_sel))
    freq = ref_osc * (div_sel - qpm1 / (qpm1 + qp))
    if freq < 540.0:
        raise Exception('VCO frequency too low %f < 540 MHz' % freq)
    if freq > 729.0:
        raise Exception('VCO frequency too high %f > 729 MHz' % freq)
    freq = freq / post_div_sel * ndiv_sel / mdiv_sel
    return freq


def freq_to_cw(freq):
    '''
    Converts a frequency to a configuration word.

    This is identical to Jukka's C program.
    '''
    mdiv_sel = 5
    ndiv_sel = 5
    min_post_div_sel = 31
    max_post_div_sel = 31
    for i in range(0, 32):
        if i == 1:
            # We skip value 3 at location 1.
            continue
        if freq * table_post_div_sel[i] < 540.0:
            min_post_div_sel = i
        if freq * table_post_div_sel[i] < 729.0:
            max_post_div_sel = i
    min_post_div_sel += 1

    best_err = 1000.0

    for i in range(min_post_div_sel, max_post_div_sel + 1):
        try_div_sel = int(((freq * table_post_div_sel[i]) / ref_osc) + 0.8)
        for try_qp in range(1, 32):
            for try_qpm1 in range(0, 32 - try_qp):
                for try_m in range(0, 8):
                    for try_n in range(0, 8):
                        if (not ((try_m <= 18 and try_n >= 31)
                                 or (try_n <= 18 and try_m >= 31)
                                 or (try_n == 18 and try_m == 14))):
                            f = ((ref_osc * (try_div_sel - try_qpm1 / (try_qpm1
                                 + try_qp)) / table_post_div_sel[i])
                                 * table_mdiv_sel[try_n]
                                 / table_mdiv_sel[try_m])
                            err = abs(f - freq)
                            if err < best_err:
                                post_div_sel = i
                                div_sel = try_div_sel
                                qpm1 = try_qpm1
                                qp = try_qp
                                mdiv_sel = try_m
                                ndiv_sel = try_n
                                best_err = err
    cw = ((qp << 23) + (qpm1 << 18) + ((div_sel - 17) << 14)
          + (post_div_sel << 6) + (ndiv_sel << 3) + mdiv_sel)
    return cw


def freq_to_cw_perfect(freq):
    '''
    Converts a frequency to a configuration word producing a perfect match.

    This function does so by iterating over all possible configuration words
    and choosing the one that best matches the requested frequency.
    '''
    best_err = math.inf
    best_cw = 0
    for cw1 in range(0, 0x4000):
        for cw2 in range(0, 0x800):
            cw = (cw1 << 14) + cw2
            try:
                try_freq = cw_to_freq(cw)
                err = abs(try_freq - freq)
                if err < best_err:
                    best_err = err
                    best_cw = cw
            except Exception:
                # We ignore exception that can occur due to invalid
                # configuration words.
                pass
    return best_cw


def main():
    '''
    Parse command-line arguments and call the appropriate conversion function.
    '''
    parser = argparse.ArgumentParser(
        description='Convert frequencies for SY87739L.')
    parser.add_argument(
        '--configword',
        dest='configword',
        help='configuration word to be converted (as hex number)')
    parser.add_argument(
        '--frequency',
        dest='frequency',
        help='frequency to be converted (in MHz)')
    parser.add_argument(
        '--perfect',
        action='store_true',
        dest='perfect',
        help='use perfect mode when converting frequencies')
    args = parser.parse_args()
    if args.configword is None and args.frequency is None:
        print('Either --configword or --frequency argument must be specified.')
        sys.exit(1)
    if args.configword is not None and args.frequency is not None:
        print('Only one of the --configword or --frequency arguments must be '
              'specified.')
        sys.exit(1)
    if args.configword is not None:
        configword = int(args.configword, 16)
        frequency = cw_to_freq(configword)
        print('Config word: %#010x' % configword)
        print('Frequency: %.4f' % frequency)
    if args.frequency is not None:
        frequency = float(args.frequency)
        if args.perfect:
            configword = freq_to_cw_perfect(frequency)
        else:
            configword = freq_to_cw(frequency)
        frequency = cw_to_freq(configword)
        print('Config word: %#010x' % configword)
        print('Frequency: %.4f' % frequency)


if __name__ == '__main__':
    main()
