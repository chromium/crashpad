#!/usr/bin/env python

# usage: rewrite_expectassert_eq.py $(git grep -El '(EXPECT|ASSERT)_EQ')

import re
import sys


def main(args):
  for path in args:
    with open(path, 'r') as file:
      contents = file.read()

    matches = re.finditer(r'(ASSERT|EXPECT)_EQ(\()', contents, re.MULTILINE)
    match_positions = []
    for match in matches:
      match_positions += [match.start(2)]

    new_contents = contents
    last_index = -1
    for match_position in match_positions:
      line_num = contents.count('\n', 0, match_position) + 1
      if last_index != -1 and match_position < last_index:
        raise AssertionError(path, line_num, last_index, match_position)

      index = match_position
      parens = 0
      dquotes = 0
      comma = -1
      while True:
        c = contents[index]
        if dquotes == 0:
          if c == '(':
            parens += 1
          elif c == ')':
            parens -= 1
            if parens == 0:
              ex_lhs = contents[match_position + 1:comma]
              ex_rhs = contents[comma + 1:index]
              new_contents = (new_contents[:match_position + 1] +
                              ex_rhs + ',' + ex_lhs +
                              new_contents[index:])
              last_index = index
              break
          elif c == '"':
            dquotes += 1
          elif parens == 1 and c == ',':
            if comma != -1:
              raise AssertionError(path, line_num, match_position, comma)
            comma = index
        else:
          if c == '"':
            dquotes -= 1

        index += 1

    with open(path, 'w') as file:
      file.write(new_contents)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
