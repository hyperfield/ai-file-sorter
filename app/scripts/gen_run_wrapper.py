#!/usr/bin/env python3
import argparse
from pathlib import Path

DEV_DECL = "SCRIPT_DIR=\"$(cd \"$(dirname \"$0\")\" && pwd)\"\nAPP_DIR=\"$(cd \"$SCRIPT_DIR/..\" && pwd)\""

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', choices=['dev', 'install'], default='dev')
    parser.add_argument('--install-app-dir', default='')
    parser.add_argument('--binary', required=True)
    parser.add_argument('--template', default='scripts/run_aifilesorter.sh.in')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()

    template = Path(args.template).read_text()
    if args.mode == 'dev':
        app_decl = DEV_DECL
    else:
        app_decl = f'APP_DIR="{args.install_app_dir}"'
    content = template.replace('@APP_DIR_DECLARATION@', app_decl)
    content = content.replace('@WRAPPED_BINARY@', args.binary)
    Path(args.output).write_text(content)

if __name__ == '__main__':
    main()
