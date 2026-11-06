# VS Chamber Orchestra 2: Community Edition (percussion)

WORLDBUILDER's Drama percussion plays orchestral percussion recordings from **VSCO 2:
Community Edition** by Versilian Studios (recorded by Sam Gossner and Simon Dalzell;
sample cutting by Elan Hickler / Soundemote).

- **Source**: https://github.com/sgossner/VSCO-2-CE (the official repository; homepage
  http://vis.versilstudios.net/vsco-community.html)
- **Version**: 1.1.0 (release tag `1.1.0`, https://github.com/sgossner/VSCO-2-CE/releases/tag/1.1.0);
  the files are fetched from the repository's `master` branch at commit
  `440300901dfe9275fd84e0b7763af1f8443ae62e` (2020-08-04, "Updated Readme"), pinned in
  `scripts/ci-fetch-vsco.sh`.
- **Licence**: Creative Commons CC0 1.0 Universal (public domain dedication), the
  repository's `LICENSE` file, reproduced in full below. Checked on 2026-11-06 before any
  sample was used.
- **The library's own terms** (its `Readme.txt`, 2016): "You are permitted to use these
  samples for ANY purpose. We ask that you do not sell the samples directly, and encourage
  you to keep any work done on improving the sample set open and public. Please provide
  credit to Versilian Studios/Sam Gossner, and/or Ivy Audio/Simon Dalzell where applicable,
  and link to the VSCO: CE homepage." These are requests on top of CC0, and WORLDBUILDER
  honours them: the samples are never sold on their own (they ship only as part of the
  instrument, prepared, inside the plugin bundle), are never committed to this repository
  (CI fetches them from the official source), and are credited here and in the README.
- **Files used** (from the repository's `Percussion/` and `VSCO 1 Percussion/` folders):
  concert bass drum (`BDrumNewhit_v1-7_rr1-2`), timpani hits and rolls
  (`Timpani/Timpani1-5_*`), snare (`Snare2-HitNS`, `Snare2-HitSN`, `Snare2-rollNS`), tenor
  drums as toms (`VSCO 1 Percussion/drums/tenor/`), the large ethnic drum
  (`VSCO 1 Percussion/drums/other/ethnic/giant/`), crash cymbal (`cymbal-crash1_*`),
  suspended cymbal hits and crescendo rolls (`susCymb1-hit_*`, `susCymb1-cresc-*`), gong
  (`gongHit_*`). The exact list and how each is mapped are in
  `tools/sample-prep/vsco.py`.

## The licence text (the repository's LICENSE, verbatim)

```
CC0 1.0 Universal

Statement of Purpose

The laws of most jurisdictions throughout the world automatically confer
exclusive Copyright and Related Rights (defined below) upon the creator and
subsequent owner(s) (each and all, an "owner") of an original work of
authorship and/or a database (each, a "Work").

Certain owners wish to permanently relinquish those rights to a Work for the
purpose of contributing to a commons of creative, cultural and scientific
works ("Commons") that the public can reliably and without fear of later
claims of infringement build upon, modify, incorporate in other works, reuse
and redistribute as freely as possible in any form whatsoever and for any
purposes, including without limitation commercial purposes. These owners may
contribute to the Commons to promote the ideal of a free culture and the
further production of creative, cultural and scientific works, or to gain
reputation or greater distribution for their Work in part through the use and
efforts of others.

For these and/or other purposes and motivations, and without any expectation
of additional consideration or compensation, the person associating CC0 with a
Work (the "Affirmer"), to the extent that he or she is an owner of Copyright
and Related Rights in the Work, voluntarily elects to apply CC0 to the Work
and publicly distribute the Work under its terms, with knowledge of his or her
Copyright and Related Rights in the Work and the meaning and intended legal
effect of CC0 on those rights.

1. Copyright and Related Rights. A Work made available under CC0 may be
protected by copyright and related or neighboring rights ("Copyright and
Related Rights"). Copyright and Related Rights include, but are not limited
to, the following:

  i. the right to reproduce, adapt, distribute, perform, display, communicate,
  and translate a Work;

  ii. moral rights retained by the original author(s) and/or performer(s);

  iii. publicity and privacy rights pertaining to a person's image or likeness
  depicted in a Work;

  iv. rights protecting against unfair competition in regards to a Work,
  subject to the limitations in paragraph 4(a), below;

  v. rights protecting the extraction, dissemination, use and reuse of data in
  a Work;

  vi. database rights (such as those arising under Directive 96/9/EC of the
  European Parliament and of the Council of 11 March 1996 on the legal
  protection of databases, and under any national implementation thereof,
  including any amended or successor version of such directive); and

  vii. other similar, equivalent or corresponding rights throughout the world
  based on applicable law or treaty, and any national implementations thereof.

2. Waiver. To the greatest extent permitted by, but not in contravention of,
applicable law, Affirmer hereby overtly, fully, permanently, irrevocably and
unconditionally waives, abandons, and surrenders all of Affirmer's Copyright
and Related Rights and associated claims and causes of action, whether now
known or unknown (including existing as well as future claims and causes of
action), in the Work (i) in all territories worldwide, (ii) for the maximum
duration provided by applicable law or treaty (including future time
extensions), (iii) in any current or future medium and for any number of
copies, and (iv) for any purpose whatsoever, including without limitation
commercial, advertising or promotional purposes (the "Waiver"). Affirmer makes
the Waiver for the benefit of each member of the public at large and to the
detriment of Affirmer's heirs and successors, fully intending that such Waiver
shall not be subject to revocation, rescission, cancellation, termination, or
any other legal or equitable action to disrupt the quiet enjoyment of the Work
by the public as contemplated by Affirmer's express Statement of Purpose.

3. Public License Fallback. Should any part of the Waiver for any reason be
judged legally invalid or ineffective under applicable law, then the Waiver
shall be preserved to the maximum extent permitted taking into account
Affirmer's express Statement of Purpose. In addition, to the extent the Waiver
is so judged Affirmer hereby grants to each affected person a royalty-free,
non transferable, non sublicensable, non exclusive, irrevocable and
unconditional license to exercise Affirmer's Copyright and Related Rights in
the Work (i) in all territories worldwide, (ii) for the maximum duration
provided by applicable law or treaty (including future time extensions), (iii)
in any current or future medium and for any number of copies, and (iv) for any
purpose whatsoever, including without limitation commercial, advertising or
promotional purposes (the "License"). The License shall be deemed effective as
of the date CC0 was applied by Affirmer to the Work. Should any part of the
License for any reason be judged legally invalid or ineffective under
applicable law, such partial invalidity or ineffectiveness shall not
invalidate the remainder of the License, and in such case Affirmer hereby
affirms that he or she will not (i) exercise any of his or her remaining
Copyright and Related Rights in the Work or (ii) assert any associated claims
and causes of action with respect to the Work, in either case contrary to
Affirmer's express Statement of Purpose.

4. Limitations and Disclaimers.

  a. No trademark or patent rights held by Affirmer are waived, abandoned,
  surrendered, licensed or otherwise affected by this document.

  b. Affirmer offers the Work as-is and makes no representations or warranties
  of any kind concerning the Work, express, implied, statutory or otherwise,
  including without limitation warranties of title, merchantability, fitness
  for a particular purpose, non infringement, or the absence of latent or
  other defects, accuracy, or the present or absence of errors, whether or not
  discoverable, all to the greatest extent permissible under applicable law.

  c. Affirmer disclaims responsibility for clearing rights of other persons
  that may apply to the Work or any use thereof, including without limitation
  any person's Copyright and Related Rights in the Work. Further, Affirmer
  disclaims responsibility for obtaining any necessary consents, permissions
  or other rights required for any use of the Work.

  d. Affirmer understands and acknowledges that Creative Commons is not a
  party to this document and has no duty or obligation with respect to this
  CC0 or use of the Work.

For more information, please see
<http://creativecommons.org/publicdomain/zero/1.0/>
```
