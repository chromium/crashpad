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

function maybe_mkdir() {
  local dir="${1}"
  if [[ ! -d "${dir}" ]]; then
    mkdir "${dir}"
  fi
}

# Run from the Crashpad project root directory.
cd "$(dirname "${0}")/../.."

source doc/support/compat.sh

doc/support/generate_doxygen.sh
doc/support/generate_asciidoc.sh

output_dir=doc/generated
maybe_mkdir "${output_dir}"

for subdir in doc doxygen man ; do
  output_subdir="${output_dir}/${subdir}"
  maybe_mkdir "${output_subdir}"
  rsync -Ilr --delete --exclude .git "out/doc/${subdir}/html/" \
      "${output_subdir}/"
done

# Move doc/index.html to index.html, adjusting relative paths to other files in
# doc.
base_url=https://crashpad.chromium.org/
${sed_ext} -e 's%<a href="([^/]+)\.html">%<a href="doc/\1.html">%g' \
    -e 's%<a href="'"${base_url}"'">%<a href="index.html">%g' \
    -e 's%<a href="'"${base_url}"'%<a href="%g' \
    < "${output_dir}/doc/index.html" > "${output_dir}/index.html"
rm "${output_dir}/doc/index.html"

# Ensure a favicon exists at the root since the browser will always request it.
cp doc/favicon.ico "${output_dir}/"

# Create man/index.html
cd "${output_dir}/man"
cat > index.html << __EOF__
<!DOCTYPE html>
<meta charset="utf-8">
<title>Crashpad Man Pages</title>
<ul>
__EOF__

for html_file in *.html; do
  if [[ "${html_file}" = "index.html" ]]; then
    continue
  fi
  basename=$(${sed_ext} -e 's/\.html$//' <<< "${html_file}")
  cat >> index.html << __EOF__
  <li>
    <a href="${html_file}">${basename}</a>
  </li>
__EOF__
done

cat >> index.html << __EOF__
</ul>
__EOF__
