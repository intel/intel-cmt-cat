from setuptools import setup

with open('README.txt', 'r') as readme_file:
    long_description = readme_file.read()

setup(
    name='pqos',
    version='4.2.0',
    author='Khawar Abbasi',
    author_email='khawar.abbasi@intel.com',
    description='Python interface for PQoS library',
    long_description=long_description,
    long_description_content_type='text/plain',
    url='https://github.com/intel/intel-cmt-cat',
    project_urls={
        'Bug Tracker': 'https://github.com/intel/intel-cmt-cat/issues'
    },
    license='BSD 3-Clause License',
    classifiers=[
        'Programming Language :: Python :: 3',
        'License :: OSI Approved :: BSD License'
    ],
    packages=['pqos', 'pqos.test'],
    python_requires='>=3.5'
)
