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

output_dir=doc/generated
maybe_mkdir "${output_dir}"

maybe_mkdir "${output_dir}/doxygen"
rsync -Ilr --delete --exclude .git "out/doc/doxygen/html/" \
    "${output_dir}/doxygen"

# Remove old things that used to be present
rm -rf "${output_dir}/doc"
rm -rf "${output_dir}/man"
rm -f "${output_dir}/index.html"

# Ensure a favicon exists at the root since the browser will always request it.
cp doc/favicon.ico "${output_dir}/"

# Create man/index.html. Do this in two steps so that the built-up list of man
# pages can be sorted according to the basename, not the entire path.
list_file=$(mktemp)
for man_path in $(find . -name '*.md' |
                  ${sed_ext} -e 's%^\./%%' |
                  grep -Ev '^(README.md$|(third_party|doc)/)'); do
  # These should show up in all man pages, but probably not all together in any
  # other Markdown documents.
  if ! (grep -q '^## Name$' "${man_path}" &&
        grep -q '^## Synopsis$' "${man_path}" &&
        grep -q '^## Description$' "${man_path}"); then
    continue
  fi

  man_basename=$(${sed_ext} -e 's/\.md$//' <<< $(basename "${man_path}"))
  cat >> "${list_file}" << __EOF__
<!-- ${man_basename} --><a href="https://chromium.googlesource.com/crashpad/crashpad/+/master/${man_path}">${man_basename}</a>
__EOF__
done

maybe_mkdir "${output_dir}/man"

cd "${output_dir}/man"
cat > index.html << __EOF__
<!DOCTYPE html>
<meta charset="utf-8">
<title>Crashpad Man Pages</title>
<ul>
__EOF__

sort "${list_file}" | while read line; do
  line=$(${sed_ext} -e 's/^<!-- .* -->//' <<< "${line}")
  cat >> index.html << __EOF__
  <li>
${line}
  </li>
__EOF__
done

rm -f "${list_file}"

cat >> index.html << __EOF__
</ul>
__EOF__
