#!/usr/bin/env python2.7

from __future__ import print_function
import os
import sys
import argparse
import re


def eprint(*args, **kwards):
    print(*args, file=sys.stderr, **kwards)


def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-s', '--source-dir', required=True, help='Directory with original DB files')
    parser.add_argument('-d', '--dest-dir', help='Directory where to write customized DB files')
    parser.add_argument('-b', '--db-name', action='append', required=True, help='DB file names to be customized')
    parser.add_argument('-p', '--file-prefix', default='', help='Prefix added to DB file names')
    parser.add_argument('-r', '--rename-pv', action='append', nargs=2, metavar=('base_name', 'new_suffix'),
                        help='Rename a PV. The base_name does not include the default : infix but '
                              'new_suffix needs to include any desired infix. Can be passed multiple '
                              ' times. Example: -p arm .Arm -p autoRestart .AutoRestart')
    parser.add_argument('-m', '--subst-macro', action='append', nargs=2, metavar=('macro_name', 'value'),
                        help='Substitute a macro. Example: -m AUTORESTART_ZNAM off')
    args = parser.parse_args()
    
    if args.dest_dir is None:
        eprint('Error: Destination directory is not specified.')
        sys.exit(1)
    
    # Collect PV mapping from arguments.
    if args.rename_pv is None:
        mapping = dict()
    else:
        mapping = dict((base_name, new_suffix) for base_name, new_suffix in args.rename_pv)
    
    # Collect macro substitutions from arguments.
    if args.subst_macro is None:
        subst = dict()
    else:
        subst = dict((macro_name, value) for macro_name, value in args.subst_macro)
    
    # Do the customization.
    customize_pvs(args.source_dir, args.dest_dir, args.db_name, args.file_prefix, mapping, subst)


def customize_pvs(source_dir, dest_dir, db_names, file_prefix, mapping, subst):
    # Prevent overwriting original DBs.
    if file_prefix == '' and os.path.abspath(source_dir) == os.path.abspath(dest_dir):
        raise ValueError('Error: Refusing to overwrite original DB files.')
    
    # Get the base PV names and macro names, ensure deterministic order.
    base_names = list(mapping.keys())
    base_names.sort()
    macro_names = list(subst.keys())
    macro_names.sort()
    
    # Regex pattern for renaming PVs.
    # The pattern will match "$(PREFIX)" followed by any of the base PV
    # names followed by a space, double quote or dot. The first group
    # captures the base PV name and the second group matches the
    # termination character.
    pattern_rename = re.escape('$(PREFIX):') + '(' + '|'.join(map(re.escape, base_names)) + ')( |"|\\.)'
    
    # Regex pattern for substituting macros.
    # The pattern will match $(MACRO) or $(MACRO=VALUE) where MACRO is
    # one of the specified macro names to be substituted and VALUE is
    # anything excluding ). The first and only group captures the
    # macro name.
    pattern_subst = r'\$\((' + '|'.join(map(re.escape, macro_names)) + r')(?:=[^\)]*\)|\))'
    
    # Make the full pattern to match either one.
    pattern = '({})|(?:{})'.format(pattern_rename, pattern_subst)
    
    # Determine group offsets for sub-regexes.
    rename_group_offset = 1
    subst_group_offset = 1 + re.compile(pattern_rename).groups
    
    # Compile the regex for use with multiple files.
    regex = re.compile(pattern)
    
    # This function will be called by regex.sub to determine the
    # replacement for a match.
    def replace_func(match):
        if match.group(1) is not None:
            # pattern_rename matched
            base_name = match.group(rename_group_offset + 1)
            terminator = match.group(rename_group_offset + 2)
            return '$(PREFIX)' + mapping[base_name] + terminator
        else:
            # pattern_subst matched
            macro_name = match.group(subst_group_offset + 1)
            return subst[macro_name]
    
    # Process all the DB files.
    for db_name in db_names:
        # Read the original DB file.
        orig_db_path = os.path.join(source_dir, db_name)
        with open(orig_db_path, 'rb') as f:
            orig_db_contents = f.read()
        
        # Transform the contents.
        new_db_contents = regex.sub(replace_func, orig_db_contents)
        
        # Write the customized DB file.
        new_db_file = os.path.join(dest_dir, file_prefix+db_name)
        with open(new_db_file, 'wb') as f:
            f.write(new_db_contents)


if __name__ == '__main__':
    main()

