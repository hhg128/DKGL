# -*- coding: utf-8 -*-

import os
import sphinx

if __name__ == '__main__':

    builder = 'html'
    build_dir = '_build'

    path = os.path.dirname(__file__)
    doctree = os.path.abspath(os.path.join(path, build_dir, 'doctree'))
    sourcedir = os.path.abspath(path)
    outdir = os.path.abspath(os.path.join(path, build_dir, builder))

    argv = [ 'sphinx-build',
             '-b', builder,
             '-d', doctree,
             sourcedir, outdir]

    sphinx.main(argv)
