# Third-party notices

MADFilter Reloaded incorporates the original MADFilter work and statically
links the Tenacity-maintained libmad 0.16.4 decoder. These are separate projects:
MADFilter supplies the Windows filter integration; libmad supplies MPEG decoding.

## Projects and attribution

| Project | Credit | Role in MADFilter Reloaded |
|---|---|---|
| Original MADFilter | Armin Gerritsen | Original Windows DirectShow filter |
| MAD / libmad | Underbit Technologies, Inc.; original author Robert Leslie | Original MPEG audio decoder library |
| Tenacity libmad | Tenacity Team and Contributors | Maintained libmad fork used by this build |

Upstream project references:

- [Tenacity libmad repository](https://codeberg.org/tenacityteam/libmad)
- [Underbit MAD project](https://www.underbit.com/products/mad/)

The Tenacity README identifies these copyright holders:

```text
Copyright (C) 2000-2004 Underbit Technologies, Inc.
Copyright (C) 2021-2026 Tenacity Team and Contributors
```

These lines are attribution from the upstream README, separate from the
original libmad COPYRIGHT notice reproduced below. The separately supplied dependency source's
`libmad/COPYRIGHT`, `libmad/COPYING`, and `libmad/CREDITS` remain unchanged.
The source snapshot declares libmad version 0.16.4; an upstream branch may
continue to change independently of the version included here.

## License and source

The original MADFilter and libmad notices below specify GNU GPL version 2 or,
at the recipient's option, any later version, and include their warranty
disclaimers. The full GPL version 2 text accompanies this package in [LICENSE](LICENSE).
The complete corresponding source is supplied as two downloads alongside the
Windows package: the project source ZIP and the matching dependency-source ZIP.
The latter contains the exact libmad source and its original notices. Both
source downloads are needed to build; neither is needed to run the filter.

The historical notices mention contacting their authors about alternate terms.
MADFilter Reloaded is distributed under its GPL license; those statements do
not grant an alternate license to this combined package.

## Original MADFilter notice

The following notice is retained from the original distribution. Its embedded
MAD credit describes the historical dependency, not the current fork's complete
attribution.

```text
 MAD TransForm Filter
 Copyright (C) 2002 Armin Gerritsen
 (Armin.Gerritsen@hetnet.nl)

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 If you would like to negotiate alternate licensing terms, you may do
 so by contacting the author: Armin Gerritsen <Armin.Gerritsen@hetnet.nl>
 
 Note that this program uses MAD. For MAD the copyright is:
 
 mad - MPEG audio decoder
 Copyright (C) 2000-2001 Robert Leslie

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 If you would like to negotiate alternate licensing terms, you may do
 so by contacting the author: Robert Leslie <rob@mars.org>
```

## libmad copyright and warranty notice

Verbatim from the dependency-source `libmad/COPYRIGHT`:

```text
 libmad - MPEG audio decoder library
 Copyright (C) 2000-2004 Underbit Technologies, Inc.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 If you would like to negotiate alternate licensing terms, you may do
 so by contacting: Underbit Technologies, Inc. <info@underbit.com>
```

## libmad author and contributor credits

Verbatim from the dependency-source `libmad/CREDITS`. Tenacity attribution is listed above.

```text
 libmad - MPEG audio decoder library
 Copyright (C) 2000-2004 Underbit Technologies, Inc.

 $Id: CREDITS,v 1.5 2004/02/17 02:02:03 rob Exp $

===============================================================================

AUTHOR

  Except where otherwise noted, all code was authored by:

      Robert Leslie <rob@underbit.com>

CONTRIBUTORS

  Significant contributions have been incorporated with thanks to:

      Anonymous
        2002/03/15: frame.c
          - Reported problem with use of reserved emphasis value.
        2003/08/31: layer12.c
          - Suggested support for certain disallowed bitrate/mode
            combinations.

      Niek Albers <info@daansystems.com>
        2003/04/21: layer3.c
          - Reported runtime uninitialized use of `ptr' in designating
            ancillary bits after a decoding error.

      Christian Biere <cbiere@users.sourceforge.net>
        2003/02/01: frame.c
          - Reported assertion failure in layer3.c due to an
            invalid/unsupported Layer III free format bitrate.

      David Blythe <blythe@routefree.com>
        2001/01/30: fixed.h
          - Provided initial PowerPC fixed-point assembly.

      Simon Burge <simonb@wasabisystems.com>
        2000/09/20: imdct_l_arm.S
          - Suggested patch for a.out compatibility.

      Brian Cameron <Brian.Cameron@sun.com>
        2003/07/02: huffman.c
          - Suggested changes for improved portability.

      Joshua Haberman <joshua@haberman.com>
        2001/08/10: decoder.c, huffman.c
          - Suggested portability fixes.

      Timothy King <lordzork@lordzork.com>
        2002/05/04: sf_table.dat, layer12.c
          - Reported problem with use of (missing) scalefactor index 63.

      Felix von Leitner <felix-mad@fefe.de>
        2003/01/21: fixed.h
          - Suggested Intel scaling alternative for possible speedup.

      Andre McCurdy <armccurdy@yahoo.co.uk>
        2000/08/10: imdct_l_arm.S
          - ARM optimized assembly replacement for III_imdct_l().
        2000/09/15: imdct_l_arm.S
          - Applied Nicolas Pitre's rounding optimisation in all remaining
            places.
        2001/02/10: layer3.c
          - Inspiration for Huffman decoding and requantization rewrite, and
            other miscellany.
        2001/03/24: imdct_l_arm.S
          - Corrected PIC unsafe code.
        2002/02/16: fixed.h
          - Discovered bug in ARM version of mad_f_scale64().

      Haruhiko OGASAWARA <theta@m1.interq.or.jp>
        2001/01/28: layer3.c
          - Reported discrepancy in alias reduction for mixed short blocks.

      Brett Paterson <brett@fmod.org>
        2001/10/28: global.h
          - Reported missing <assert.h> et al. under MS Embedded Visual C.

      Sean 'Shaleh' Perry <shaleh@via.net>
        2000/04/04: fixed.h
          - Suggested use of size-dependent typedefs.
        2001/10/22: config.guess, config.sub
          - Keep up to date for proper Debian packaging.

      Bertrand Petit <eemad@phoe.frmug.org>
        2001/11/05: synth.h
          - Suggested PCM channel enumeration constants.
        2001/11/05: stream.h
          - Suggested MAD_ERROR_NONE enumeration constant.
        2001/11/05: stream.c
          - Suggested mad_stream_errorstr() function.

      Nicolas Pitre <nico@cam.org>
        2000/09/09: fixed.h
          - Parameterized all scaling for correct use of all multiplication
            methods within mad_synth_frame().
          - Rewrote the FPM_ARM version of mad_f_mul() so we have 64-bit
            multiplication result, rounding and scaling with 3 instructions.
        2000/09/09: imdct_l_arm.S
          - Optimized rounding + scaling operations.
        2000/09/17: synth.c
          - Changed D[] run-time shifts to compile-time.
          - Modified synthesis for better multiply/accumulate code output.
        2001/08/11: fixed.h, synth.c
          - Suggested 64-bit FPM negation and negative term factorization
            during synthesis.
        2001/08/11: fixed.h
          - Suggested unrounded behavior for FPM_DEFAULT when OPT_SPEED.
        2001/11/19: fixed.c
          - Suggested computation of any resampling ratio.

===============================================================================
```
