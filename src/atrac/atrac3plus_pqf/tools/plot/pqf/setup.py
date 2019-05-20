from distutils.core import setup, Extension

setup(name = 'pqf', version = '1.0', \
        ext_modules = [Extension('pqf', \
                                 sources=['pqf_binding.c', '../../../atrac3plus_pqf.c'], \
                                 include_dirs=['../../../'])])
