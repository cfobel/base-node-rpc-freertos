from collections import OrderedDict
from importlib import import_module
import os
import sys

from paver.easy import task, needs, path, sh, options
from paver.setuputils import install_distutils_tasks
try:
    from base_node_rpc.pavement_base import *  # base-node-rpc Paver tasks
except ImportError:
    pass
import platformio_helpers as pioh
import platformio_helpers.develop  # platformio Paver tasks

# Make standard `setuptools.command` tasks available (e.g., `sdist`).
install_distutils_tasks()

# add the current directory as the first listing on the python path
# so that we import the correct version.py
sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))
import versioneer

# Import project module.
rpc_module = import_module('base_node_rpc_freertos')
VERSION = versioneer.get_version()
PROPERTIES = OrderedDict([('name', 'base-node-rpc-freertos'),
                          ('package_name', 'base-node-rpc-freertos'),
                          ('module_name', 'base_node_rpc_freertos'),
                          ('manufacturer', 'Christian Fobel'),
                          ('software_version', VERSION),
                          ('url', 'https://github.com/cfobel/base-node-rpc-freertos')])

# XXX Properties used to generate Arduino library properties file.
LIB_PROPERTIES = PROPERTIES.copy()
LIB_PROPERTIES.update(OrderedDict([('author', 'Christian Fobel'),
                                   ('author_email', 'christian@fobel.net'),
                                   ('camelcase_name', 'BaseNodeRpcFreeRtos'),
                                   ('short_description', 'Base node RPC project using FreeRTOS task for handling serial requests.'),
                                   ('version', VERSION),
                                   ('long_description', ''),
                                   ('category', ''),
                                   ('architectures', 'avr')]))

options(
    rpc_module=rpc_module,
    PROPERTIES=PROPERTIES,
    LIB_PROPERTIES=LIB_PROPERTIES,
    base_classes=['BaseNodeSerialHandler',
                  'BaseNodeEeprom',
                  'BaseNodeI2c',
                  'BaseNodeI2cHandler<Handler>',
                  'BaseNodeConfig<ConfigMessage, Address>',
                  'BaseNodeState<StateMessage>'],
    rpc_classes=['base_node_rpc_freertos::Node'],
    setup=dict(name=PROPERTIES['name'],
               version=VERSION,
               description=LIB_PROPERTIES['short_description'],
               author=LIB_PROPERTIES['author'],
               author_email=LIB_PROPERTIES['author'],
               url=PROPERTIES['url'],
               license='BSD',
               install_requires=['base-node-rpc>=0.23'],
               include_package_data=True,
               packages=['base_node_rpc_freertos',
                         'base_node_rpc_freertos.bin']))


@task
def develop_link():
    import logging; logging.basicConfig(level=logging.INFO)
    pioh.develop.link(working_dir=path('.').realpath(),
                      package_name=PROPERTIES['package_name'])


@task
def develop_unlink():
    import logging; logging.basicConfig(level=logging.INFO)
    pioh.develop.unlink(working_dir=path('.').realpath(),
                        package_name=PROPERTIES['package_name'])


@task
@needs('base_node_rpc.pavement_base.generate_all_code')
def build_firmware():
    sh('pio run')


@task
def upload():
    sh('pio run --target upload --target nobuild')


@task
@needs('generate_setup', 'minilib', 'build_firmware',
       'setuptools.command.sdist')
def sdist():
    """Overrides sdist to make sure that our setup.py is generated."""
    pass
