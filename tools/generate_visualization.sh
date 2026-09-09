#!/bin/sh
# Build the C trace, capture clean CSV, and render it using Python stdlib only.
set -eu
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_dir="$repo_root/build/visualization"
mkdir -p "$output_dir"
cd "$repo_root/.zephyr-workspace"
west build -b native_sim -s "$repo_root/samples/telemetry" \
    -d "$repo_root/build/telemetry-demo" > "$output_dir/build.log" 2>&1 || {
    cat "$output_dir/build.log" >&2
    exit 1
}
# A failed executable or malformed CSV must not replace the last valid artifacts.
trap 'rm -f "$output_dir/scenario.csv.tmp" "$output_dir/scenario.html.tmp"' EXIT HUP INT TERM
"$repo_root/build/telemetry-demo/zephyr/zephyr.exe" > "$output_dir/scenario.csv.tmp"
python3 "$repo_root/tools/render_telemetry.py" \
    "$output_dir/scenario.csv.tmp" "$output_dir/scenario.html.tmp"
mv "$output_dir/scenario.csv.tmp" "$output_dir/scenario.csv"
mv "$output_dir/scenario.html.tmp" "$output_dir/scenario.html"
printf '%s\n' "$output_dir/scenario.csv" "$output_dir/scenario.html"
