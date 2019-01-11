#!/usr/bin/env python3
if __name__ == "__main__":
    import os
    import sys

    above_dir = os.path.dirname(os.path.realpath(__file__)) + "/../"
    abspath = os.path.abspath(above_dir)
    sys.path.insert(0, abspath)

    from cf_remote.main import main
    main()
