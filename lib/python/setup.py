from distutils.core import setup

with open(u'README.txt', 'r') as long_desc_file:
    long_description = long_desc_file.read()

setup(
    name=u'pqos',
    version=u'3.0.1',
    maintainer=u'Intel',
    maintainer_email=u'adrianx.boczkowski@intel.com',
    packages=[u'pqos', u'pqos.test'],
    url=u'https://github.com/intel/intel-cmt-cat',
    license=u'BSD',
    description=u'Python interface for PQoS library',
    long_description=long_description,
)
