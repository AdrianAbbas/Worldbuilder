#!/usr/bin/env bash
# Fetches the orchestral percussion WORLDBUILDER's Drama drums use, and the woodwinds and the
# brass its woodwind and brass parts play, from VSCO 2: Community
# Edition (Versilian Studios, CC0 1.0: docs/LICENSES/VSCO-2-CE.md) at a pinned commit of
# its official repository, only the files tools/sample-prep/vsco.py maps (a sparse,
# blob-filtered checkout, about 650 MB), and checks the licence before anything is used.
# CI caches the checkout. The samples are never committed to this repository.
# Usage: scripts/ci-fetch-vsco.sh <checkout dir>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dir="${1:?checkout directory}"
url="https://github.com/sgossner/VSCO-2-CE.git"
commit="440300901dfe9275fd84e0b7763af1f8443ae62e"   # master, 2020-08-04 (release 1.1.0's content)
licence_sha256="36ffd9dc085d529a7e60e1276d73ae5a030b020313e6c5408593a6ae2af39673"   # CC0 1.0 Universal

stamp="$dir/.worldbuilder-fetched"
patterns="$(python3 "$here/tools/sample-prep/vsco.py" --source . --out . --print-fetch)"
want="$commit $(printf '%s' "$patterns" | sha256sum | cut -c1-16)"
if [[ -f "$stamp" && "$(cat "$stamp")" == "$want" ]]; then
    echo "VSCO 2 CE: cached checkout of ${commit:0:7} ($(find "$dir" -name '*.wav' | wc -l | tr -d ' ') WAVs)"
else
    rm -rf "$dir"
    git init -q "$dir"
    git -C "$dir" remote add origin "$url"
    git -C "$dir" config core.sparseCheckout true
    printf '%s\n' "$patterns" | git -C "$dir" sparse-checkout set --no-cone --stdin
    ok=0
    for attempt in 1 2 3 4; do
        if git -C "$dir" fetch -q --depth 1 --filter=blob:none origin "$commit"; then ok=1; break; fi
        sleep $((2 ** attempt))
    done
    if [[ $ok != 1 ]]; then
        echo "::error title=VSCO fetch failed::Could not fetch $url at $commit."
        exit 1
    fi
    git -C "$dir" -c advice.detachedHead=false checkout -q FETCH_HEAD
    echo "$want" > "$stamp"
    echo "VSCO 2 CE: fetched ${commit:0:7} ($(find "$dir" -name '*.wav' | wc -l | tr -d ' ') WAVs, $(du -sh --exclude=.git "$dir" 2>/dev/null | cut -f1 || true))"
fi
# The licence must still be CC0 1.0 (the file this repository recorded).
got="$( (sha256sum "$dir/LICENSE" 2>/dev/null || shasum -a 256 "$dir/LICENSE") | cut -d' ' -f1)"
if [[ "$got" != "$licence_sha256" ]]; then
    echo "::error title=VSCO licence changed::$dir/LICENSE is not the CC0 1.0 text recorded in docs/LICENSES/VSCO-2-CE.md (sha256 $got). Stop and review the licence."
    exit 1
fi
