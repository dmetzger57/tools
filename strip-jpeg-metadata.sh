#!/usr/bin/env zsh
#
# strip-jpeg-metadata.zsh
#     Remove *all* metadata from every .jpg / .jpeg found in a folder tree.
#     By default it overwrites the originals IN-PLACE (no backups).
#     Use -b to keep “_original” safety copies beside each file.
#
#   Usage examples:
#       ./strip-jpeg-metadata.zsh ~/Pictures/ToShare
#       ./strip-jpeg-metadata.zsh -b ~/Scans/OldAlbum
#       ./strip-jpeg-metadata.zsh -h
#

set -euo pipefail
autoload -Uz colors && colors
zmodload zsh/stat # faster file tests

print_help () {
  cat <<EOF
${fg[cyan]}Strip${reset_color} ALL metadata from JPEGs (recursively).
Usage: ${fg[green]}$(basename $0)${reset_color} [-b] <folder>

  -b        Keep backup copies (adds _original to each filename)
  -h        Show this help
EOF
}

# ---------- Parse options ----------
keep_backups=false
while getopts ":bh" opt; do
  case $opt in
    b) keep_backups=true ;;
    h) print_help; exit 0 ;;
    \?) print_help; exit 1 ;;
  esac
done
shift $((OPTIND-1))

# ---------- Sanity checks ----------
[[ $# -eq 1 ]] || { print_help; exit 1 }
target="$1"
[[ -d $target ]] || { echo "❌  '$target' is not a directory"; exit 1 }
command -v exiftool >/dev/null 2>&1 \
  || { echo "❌  exiftool not found. Install with 'brew install exiftool'"; exit 1 }

echo "${fg[yellow]}→ Stripping metadata in:${reset_color} $target"
echo

# ---------- Main work ----------
# Use ** globbing for recursive search
setopt extended_glob glob_dots
typeset -i total=0
for img in $target/**/*.(jpg|jpeg)(.N); do
  (( total++ ))
  if $keep_backups; then
    exiftool -overwrite_original_in_place=false -all= "$img" >/dev/null
  else
    exiftool -overwrite_original -all= "$img" >/dev/null
  fi
done

echo "${fg[green]}✔ Done.${reset_color} Cleaned $total file(s)."
if $keep_backups; then
  echo "   Backups saved as *_original."
fi
