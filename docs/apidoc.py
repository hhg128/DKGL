
import sphinx.apidoc
import os
import sys


if __name__ == '__main__':
    # get current directory
    exec = os.path.basename(__file__)
    script_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), 'dk'))
    output_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'SphinxDoc'))

    args = [exec,]
    args += ('-f',)
    # args += ('-F',)
    # args += ('-H', 'DKScript')
    # args += ('-A', 'Hongtae Kim')
    # args += ('-V', '1.0', '-R', '1.0a')
    args += ('-o', output_dir, script_dir)
    print('args: ', args)
    sphinx.apidoc.main(args)

