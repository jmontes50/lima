#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
import re
import argparse
import tempfile
import unix_ar
import tarfile
import requests
import urllib.request


URL_DEB = 'https://github.com/aymara/lima-models/releases/download/v0.1.5-beta/lima-deep-models-%s-%s_0.1.5_all.deb'
URL_C2LC = 'https://raw.githubusercontent.com/aymara/lima-models/master/c2lc.txt'
C2LC = { 'lang2code': {}, 'code2lang': {} }

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-l', '--lang', type=str, required=True,
                        help='language name or language code (example: \'english\' or \'eng\'')
    parser.add_argument('-d', '--dest', type=str,
                        help='destination directory')
    args = parser.parse_args()

    code, lang = find_lang_code(args.lang.lower())
    deb_url = URL_DEB % (code, lang)

    if args.dest is None or len(args.dest) == 0:
        home_dir = os.path.expanduser('~')
        target_dir = os.path.join(home_dir, '.lima', 'resources')
    else:
        target_dir = args.dest

    print('Language: %s, code: %s' % (lang, code))
    print('Installation dir: %s' % target_dir)
    print('Downloading %s' % deb_url)

    with tempfile.TemporaryDirectory() as tmpdirname:
        download_binary_file(deb_url, tmpdirname)
        install_model(target_dir, os.path.join(tmpdirname, deb_url.split('/')[-1]))


def install_model(dir, fn):
    ar_file = unix_ar.open(fn)
    tarball = ar_file.open('data.tar.gz')
    tar_file = tarfile.open(fileobj=tarball)
    members = tar_file.getmembers()
    for m in members:
        if m.size > 0:
            file = tar_file.extractfile(m)
            if file is not None:
                full_dir, name = os.path.split(m.name)
                mo = re.match(r'./usr/share/apps/lima/resources/(TensorFlow[A-Za-z\/\-\.0-9]+)', full_dir)
                if mo:
                    subdir = mo.group(1)
                    if subdir is None or len(subdir) == 0:
                        sys.stderr.write('Error: can\'t parse \'%s\'\n' % full_dir)
                        sys.exit(1)
                    target_dir = os.path.join(dir, subdir)
                    os.makedirs(target_dir, exist_ok=True)
                    with open(os.path.join(target_dir, name), 'wb') as f:
                        while True:
                            chunk = file.read(4096)
                            if len(chunk) == 0:
                                break
                            f.write(chunk)


def download_binary_file(url, dir):
    chunk_size = 4096
    response = requests.get(url)
    local_filename = os.path.join(dir, url.split('/')[-1])
    totalbytes = 0
    if response.status_code == 200:
        with open(local_filename, 'wb') as f:
            for chunk in response.iter_content(chunk_size=chunk_size):
                if chunk:
                    totalbytes += chunk_size
                    f.write(chunk)


def find_lang_code(lang_str):
    if len(list(C2LC['lang2code'].keys())) == 0:
        with urllib.request.urlopen(URL_C2LC) as f:
            for line in f.read().decode('utf-8').lower().split('\n'):
                line.strip()
                if len(line) > 0:
                    corpus, code = line.split(' ')
                    lang, corp_id = corpus.split('-')
                    C2LC['lang2code'][lang] = code
                    C2LC['code2lang'][code] = lang

    if lang_str in C2LC['lang2code']:
        return C2LC['lang2code'][lang_str], lang_str
    elif lang_str in C2LC['code2lang'][lang_str]:
        return lang_str, C2LC['code2lang'][lang_str]
    return None


if __name__ == '__main__':
    main()
