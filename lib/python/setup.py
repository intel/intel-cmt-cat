from distutils.core import setup

with open(u'README.txt', 'r') as long_desc_file:
    long_description = long_desc_file.read()

setup(
    name='pqos',
    version='3.1.0',
    maintainer='Intel',
    maintainer_email='adrianx.boczkowski@intel.com',
    packages=['pqos', 'pqos.test'],
    url='https://github.com/intel/intel-cmt-cat',
    license='BSD',
    description='Python interface for PQoS library',
    long_description=long_description,
)
