#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <executable> <output-dir>" >&2
    exit 2
fi

exe=$1
out_dir=$2
prefix=${MSYSTEM_PREFIX:-/mingw64}
repo_root=$(git rev-parse --show-toplevel 2>/dev/null || pwd)
licenses_dir="$out_dir/licenses"
notices_file="$out_dir/THIRD-PARTY-NOTICES.txt"

if [ ! -f "$exe" ]; then
    echo "executable not found: $exe" >&2
    exit 1
fi

if [ -z "$out_dir" ] || [ "$out_dir" = "/" ]; then
    echo "refusing unsafe output directory: $out_dir" >&2
    exit 1
fi

rm -rf "$out_dir"
mkdir -p "$out_dir"
mkdir -p "$licenses_dir"
cp -f "$exe" "$out_dir/"
cp -f "$repo_root/LICENSE" "$out_dir/LICENSE.txt"
cp -f "$repo_root/README.md" "$out_dir/README.md"

if [ -f "$repo_root/src/lib/libgha/LICENSE" ]; then
    cp -f "$repo_root/src/lib/libgha/LICENSE" "$licenses_dir/libgha-LICENSE.txt"
fi

if [ -f "$repo_root/src/lib/fft/kissfft_impl/kiss_fft.c" ]; then
    sed -n '1,/^\*\//p' "$repo_root/src/lib/fft/kissfft_impl/kiss_fft.c" > "$licenses_dir/kissfft-NOTICE.txt"
fi

if [ -f "$repo_root/src/platform/win/getopt/getopt.h" ]; then
    awk '/^#pragma/ { exit } { print }' "$repo_root/src/platform/win/getopt/getopt.h" > "$licenses_dir/mingw-getopt-NOTICE.txt"
fi

cat > "$out_dir/SOURCE.txt" <<EOF
AtracDEnc is distributed under the GNU Lesser General Public License,
version 2.1 or later. See LICENSE.txt.

Corresponding source code for this build is available from the project
repository. Build commit:

$(git -C "$repo_root" rev-parse HEAD 2>/dev/null || echo "unknown")

Bundled MSYS2 runtime DLLs are unmodified binaries from MSYS2 packages.
Their sources can be obtained from MSYS2 using the package names listed in
THIRD-PARTY-NOTICES.txt.
EOF

cat > "$notices_file" <<EOF
Third-party notices for the MSYS2 Windows binary package.

Static third-party notices from source components are included under licenses/.

This package bundles DLLs copied from MSYS2/MinGW packages so the executable
can run without a separate MSYS2 installation. License files found in the
local MSYS2 installation are copied under licenses/msys2/<package>/.

Bundled DLLs:
EOF

list_msys2_dlls() {
    ldd "$1" | awk -v prefix="$prefix" '
        {
            for (i = 1; i <= NF; ++i) {
                gsub(/\r/, "", $i)
                if (index($i, prefix "/bin/") == 1 && $i ~ /\.dll$/) {
                    print $i
                }
            }
        }
    '
}

copy_package_license() {
    local dll=$1
    local pkg=""
    local license_src=""
    local license_dst=""

    if ! command -v pacman >/dev/null 2>&1; then
        printf '  %s - owning package unknown: pacman not available\n' "$(basename "$dll")" >> "$notices_file"
        return
    fi

    pkg=$(pacman -Qo "$dll" 2>/dev/null | awk '{ print $(NF - 1) }' || true)
    if [ -z "$pkg" ]; then
        printf '  %s - owning package unknown\n' "$(basename "$dll")" >> "$notices_file"
        return
    fi

    printf '  %s - %s\n' "$(basename "$dll")" "$pkg" >> "$notices_file"

    license_dst="$licenses_dir/msys2/$pkg"
    mkdir -p "$license_dst"

    for license_src in \
        "$prefix/share/licenses/$pkg" \
        "/usr/share/licenses/$pkg"
    do
        if [ -d "$license_src" ]; then
            cp -R "$license_src"/. "$license_dst"/
            return
        fi
    done

    printf '    license directory not found in MSYS2 installation\n' >> "$notices_file"
}

queue=("$exe")

while [ "${#queue[@]}" -gt 0 ]; do
    current=${queue[0]}
    queue=("${queue[@]:1}")

    while IFS= read -r dll; do
        [ -n "$dll" ] || continue

        dst="$out_dir/$(basename "$dll")"
        if [ ! -f "$dst" ]; then
            cp -f "$dll" "$dst"
            copy_package_license "$dll"
            queue+=("$dll")
        fi
    done < <(list_msys2_dlls "$current" | sort -u)
done

echo "MSYS2 package contents:"
ls -la "$out_dir"
