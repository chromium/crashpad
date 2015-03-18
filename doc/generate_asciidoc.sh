#!/bin/bash

# Copyright 2015 The Crashpad Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

# Generating AsciiDoc documentation requires AsciiDoc,
# http://www.methods.co.nz/asciidoc/. For “man” and PDF output, a DocBook
# toolchain including docbook-xml and docbook-xsl is also required.

# Run from the Crashpad project root directory.
cd "$(dirname "${0}")/.."

output_dir=out/doc/man

rm -rf "${output_dir}"
mkdir -p \
    "${output_dir}/html" \
    "${output_dir}/man"

# Some extensions of command-line tools behave differently on different systems.
# $sed_ext should be a sed invocation that enables extended regular expressions.
# $date_time_t should be a date invocation that causes it to print the date and
# time corresponding to a time_t string that immediately follows it.
uname_s="$(uname -s)"
case "${uname_s}" in
  Darwin)
    sed_ext="sed -E"
    date_time_t="date -r"
    ;;
  Linux)
    sed_ext="sed -r"
    date_time_t="date -d@"
    ;;
  *)
    echo "${0}: unknown operating system" >& 2
    exit 1
    ;;
esac

# Get the version from package.h.
version=$(${sed_ext} -n -e 's/^#define PACKAGE_VERSION "(.*)"$/\1/p' package.h)

for input in \
    handler/mac/crashpad_handler.ad \
    tools/*.ad \
    tools/mac/*.ad; do
  echo "${input}"

  base=$(${sed_ext} -e 's%^.*/([^/]+)\.ad$%\1%' <<< "${input}")

  # Get the last-modified date of $input according to Git, in UTC.
  git_time_t="$(git log -1 --format=%at "${input}")"
  git_date="$(LC_ALL=C ${date_time_t}"${git_time_t}" -u '+%B %-d, %Y')"

  # Create HTML output.
  asciidoc \
      --attribute mansource=Crashpad \
      --attribute manversion="${version}" \
      --attribute manmanual="Crashpad Manual" \
      --attribute revdate="${git_date}" \
      --conf-file doc/asciidoc.conf \
      --doctype manpage \
      --backend html5 \
      --attribute stylesheet="${PWD}/doc/asciidoc.css" \
      --out-file "${output_dir}/html/${base}.html" \
      "${input}"

  # Create “man” output.
  #
  # AsciiDoc 8.6.9 produces harmless incorrect warnings each time this is run:
  # “a2x: WARNING: --destination-dir option is only applicable to HTML based
  # outputs”. https://github.com/asciidoc/asciidoc/issues/44
  a2x \
      --attribute mansource=Crashpad \
      --attribute manversion="${version}" \
      --attribute manmanual="Crashpad Manual" \
      --attribute revdate="${git_date}" \
      --asciidoc-opts=--conf-file=doc/asciidoc.conf \
      --doctype manpage \
      --format manpage \
      --destination-dir "${output_dir}/man" \
      "${input}"

  # For PDF output, use an a2x command like the one above, with these options:
  # --format pdf --fop --destination-dir "${output_dir}/pdf"
done
