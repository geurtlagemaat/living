"""Fabric deployment script.
Contains tasks to install this Bliknet application (in a virtual env) """
import os
import sys
import tempfile
import shutil
from os.path import join
from posixpath import join as posixjoin

from fabric.api import env, put, cd, sudo, task, local, run

SCRIPT_DIR = os.path.abspath(os.path.dirname(os.path.realpath(__file__)))

# targets and common are located 1 level up (contains credentials)
if __name__ == 'fabfile':
    sys.path.insert(0, join(SCRIPT_DIR))
    sys.path.insert(0, join(SCRIPT_DIR, '..'))

    import targets  # noqa

import common  # noqa

VERSION_FILE_NAME = 'release_info'

def prepare_env(svn_rel_path=None, version_name=None):
    # Environment defaults
    env.bliknet_base_dir = '/opt/bliknet'
    env.archive_name = 'bliknet_node-scripts.zip'
    env.git_clone = "https://github.com/geurtlagemaat/weatherstation.git"
    env.circus_path = '/opt/bliknet/circus'
    env.version_name = 'unknown'

    # Where to put/read the locally created installation kit?
    if os.path.exists(join(SCRIPT_DIR, env.archive_name)):
        # We're running from the kit:
        if not env.get('local_kit_dir'):
            env.local_kit_dir = SCRIPT_DIR
        with open(join(env.local_kit_dir, VERSION_FILE_NAME), 'r') as f:
            env.version_name = f.read().strip()
    else:
        env.version_name = env.svn_rel_path.split('/')[-1]
        if not env.get('local_kit_dir'):
            env.local_kit_dir = join(SCRIPT_DIR, 'kits', env.version_name)

    env.remote_kit_dir = posixjoin('/tmp/', env.version_name)

@task
def build_kit(git_clone_path=None, version_name=None):
    """Create a Bliknet installation kit (zip file) from Git repro location """
    if git_clone_path is None:
        print("Please supply a git_clone path ")
        sys.exit(3)

    prepare_env(git_clone_path, version_name)

    if os.path.exists(env.local_kit_dir):
        print('Removing existing kit...')
        shutil.rmtree(env.local_kit_dir)

    os.makedirs(env.local_kit_dir)
    release_tmp_dir = tempfile.mkdtemp()

    clone_git = local("git clone %s %s" % (git_clone_path, release_tmp_dir))
    print()
    print("Git repo cloned to {}!".format(git_clone_path))

    # Create ZIP of exported files
    env.archive_path = posixjoin(env.local_kit_dir, env.archive_name)
    common.add_dir_to_zip(env.archive_path, release_tmp_dir, ext_whitelist=['.sh', '.py'])

    # Copy fabric file to kit dir
    shutil.copy(join(SCRIPT_DIR, 'fabfile.py'), env.local_kit_dir)
    shutil.copy(join(SCRIPT_DIR, '..', 'targets.py'), env.local_kit_dir)
    shutil.copy(join(SCRIPT_DIR, '..', 'common.py'), env.local_kit_dir)

    # Copy Circus dependencies into the kit
    requirements_path = join(SCRIPT_DIR, 'requirements', 'circus.txt')
    repository_path = join(SCRIPT_DIR, '..', 'package_repository')
    target_path = join(env.local_kit_dir, 'package_repository')
    common.collect_requirements(requirements_path, repository_path, target_path)

    shutil.copytree(join(SCRIPT_DIR, 'circus-config'), join(env.local_kit_dir, 'circus-config'))
    shutil.copytree(join(SCRIPT_DIR, 'scripts'), join(env.local_kit_dir, 'scripts'))

    # Save version name in the kit in the "release_info" file
    with open(join(env.local_kit_dir, VERSION_FILE_NAME), 'w') as f:
        f.write(env.version_name)
    
    print('Kit for Bliknet application created in {local_kit_dir}'.format(**env))

@task
def upload_kit():
    """Upload installation kit to remote server"""
    # Create remote work dir
    prepare_env()

    sudo('rm -rf "{remote_kit_dir}"'.format(**env))
    sudo('mkdir -p "{remote_kit_dir}"'.format(**env))
    sudo('chmod 777 "{remote_kit_dir}"'.format(**env))
    for fn in os.listdir(env.local_kit_dir):
        put(join(env.local_kit_dir, fn), env.remote_kit_dir)


@task
def install_kit():
    """Install bliknet application scripts as bliknet """
    prepare_env()

    # Unpack the previously uploaded ZIP with scripts
    env.remote_archive_path = posixjoin(env.remote_kit_dir, env.archive_name)
    with cd(env.bliknet_base_dir):
        sudo('unzip -o "{remote_archive_path}"'.format(**env))

        # Make scripts executable by user bliknet
        sudo('chown bliknet.bliknet *.sh *.py')
        sudo('chmod 700 *.sh')

@task
def install_circus():
    """Install circus process management in a separate virtualenv"""
    prepare_env()

    env.circus_venv_path = posixjoin(env.circus_path, 'virtualenv')

    # Install YUM dependencies
    sudo('yum install -y gcc-c++')

    # Prepare directory structure for Circus
    sudo('mkdir -p {circus_path}'.format(**env))
    sudo('mkdir -p {circus_path}/config'.format(**env))
    sudo('mkdir -p {circus_path}/config/instances'.format(**env))
    sudo('mkdir -p {circus_path}/logs'.format(**env))

    # Create virtualenv and install dependencies into it
    sudo('rm -rf "{circus_venv_path}"'.format(**env))
    sudo('virtualenv --never-download --no-pip --no-setuptools --no-wheel "{circus_venv_path}"'.format(**env))
    common.install_virtualenv_contents_setup(env.circus_venv_path, 
        posixjoin(env.remote_kit_dir, 'package_repository'),
        posixjoin(env.remote_kit_dir, 'package_repository', 'requirement-filenames.txt'))

    # Remove old files
    sudo('rm -f {bliknet_base_dir}/circusd'.format(**env))
    sudo('rm -f {bliknet_base_dir}/circusctl'.format(**env))
    sudo('rm -f {bliknet_base_dir}/circus-top'.format(**env))

    # Install scripts
    sudo('cp {remote_kit_dir}/scripts/*.sh {bliknet_base_dir}/circus'.format(**env))
    sudo('chmod 755 {bliknet_base_dir}/circus/*.sh'.format(**env))

    # Install config
    sudo('cp {remote_kit_dir}/circus-config/* {bliknet_base_dir}/circus/config/'.format(**env))

    # Set rights
    sudo('chown -R bliknet.bliknet {circus_path}'.format(**env))

@task
def install_init_script():
    """Install init.d script to start the Circus process manager"""
    prepare_env()

    # Create pidfile directory for init.d-script
    sudo('mkdir -p /var/run/circus')
    sudo('chown bliknet.bliknet /var/run/circus')

    # Install init.d script
    sudo('cp {remote_kit_dir}/scripts/circus-bliknet /etc/init.d/circus-bliknet'.format(**env))
    sudo('chmod 755 /etc/init.d/circus-bliknet')

    # Enable init.d script by default
    sudo('/sbin/chkconfig circus-bliknet on')

    # Remove manual script for circusd
    sudo('rm -f {bliknet_base_dir}/circus/circusd.sh'.format(**env))

    sudo('echo "Beheer circus met het volgende commando: '
         'sudo service circus-bliknet start/stop/restart/status" > '
         '{bliknet_base_dir}/circus/circusd-README.txt'.format(**env))


@task
def install_systemd_service():
    """Install systemd config to start the Circus process manager"""
    prepare_env()

    sudo('cp {remote_kit_dir}/scripts/circus-bliknet.service /etc/systemd/system/circus-bliknet.service'.format(**env))
    sudo('chmod 644 /etc/systemd/system/circus-bliknet.service')
    sudo('systemctl --system daemon-reload')

    # Remove manual script for circusd
    sudo('rm -f {bliknet_base_dir}/circus/circusd.sh'.format(**env))

    sudo('echo "Beheer circus met het volgende commando: '
         'sudo service circus-bliknet start/stop/restart/status" > '
         '{bliknet_base_dir}/circus/circusd-README.txt'.format(**env))


@task
def create_bliknet_user():
    """Create user bliknet and group bliknet on remote server"""
    group_exists = False
    user_exists = False
    
    group_list = run('getent group')
    user_list = run('getent passwd')

    for line in group_list.split('\n'):
        if line.startswith('bliknet:'):
            group_exists = True
            break
    for line in user_list.split('\n'):
        if line.startswith('bliknet:'):
            user_exists = True
            break

    if not group_exists:
        sudo('groupadd --system bliknet')
    else:
        print('Not creating group bliknet: it already exists')
    if not user_exists:
        sudo('useradd --create-home --home-dir /opt/bliknet --gid bliknet --system bliknet')
        # TODO, test!
        sudo('for GROUP in adm dialout cdrom sudo audio video plugdev games users netdev input spi i2c gpio; do sudo adduser bliknet $GROUP; done')
    else:
        print('Not creating user bliknet: it already exists')
