#(C) 2021 by Domenico Panella <pandom79@gmail.com>

#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License version 3.
#See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
#
project('Unitd', 'c', version: '3.0.1', default_options: ['warning_level=3', 'prefix=/usr'] )
add_global_arguments(['-Wno-incompatible-pointer-types'], language: 'c')

ver = '3.0.1'
so_ver = '3'
project_name = 'unitd'

# Get paths
prefix = get_option('prefix')
sysconfdir = get_option('sysconfdir')
datadir = get_option('datadir')
localstatedir = get_option('localstatedir')
sbin_path = get_option('sbindir')
libdir = get_option('libdir')
mandir = get_option('mandir')

# Setting unitd macro path
unitd_conf_path = sysconfdir/project_name
units_path = prefix/libdir/project_name/'units'
units_user_path = units_path/'user'
units_enab_path = unitd_conf_path/'units'
unitd_data_path = prefix/datadir/project_name
unitd_log_path = localstatedir/'log'/project_name+'.log'
doc_path = get_option('DOC_PATH')
if doc_path == ''
    doc_path  = prefix/datadir/'doc'/project_name+'-'+ver
endif
bash_comp_path = get_option('BASH_COMP_PATH')
if (bash_comp_path == '')
    bash_comp_path = prefix/datadir/'bash-completion'/'completions'
endif
our_utmp_file = get_option('OUR_UTMP_FILE')
if (our_utmp_file == '')
    our_utmp_file = localstatedir/'run'/'utmp'
endif
our_wtmp_file = get_option('OUR_WTMP_FILE')
if (our_wtmp_file == '')
    our_wtmp_file = localstatedir/'log'/'wtmp'
endif

# Configuration
conf_data = configuration_data()
conf_data.set('OS_NAME', get_option('OS_NAME'))
conf_data.set('OUR_UTMP_FILE', our_utmp_file)
conf_data.set('OUR_WTMP_FILE', our_wtmp_file)
conf_data.set('UNITS_PATH', units_path)
conf_data.set('UNITS_USER_PATH', units_user_path)
conf_data.set('UNITS_ENAB_PATH', units_enab_path)
conf_data.set('UNITD_DATA_PATH', unitd_data_path)
conf_data.set('UNITD_CONF_PATH', unitd_conf_path)
conf_data.set('UNITD_LOG_PATH', unitd_log_path)
conf_data.set('UNITD_TEST', get_option('UNITD_TEST'))
conf_data.set('DAEMON_SIGNAL_TIME', get_option('DAEMON_SIGNAL_TIME'))
configure_file(input: 'unitd_config.h.in', output: 'unitd_config.h', configuration: conf_data)

# Dependencies
deps = [dependency('threads'), dependency('wrapper', version: '>=2.0.0')]
# Checking commands/packages dependencies
# If it is not exists then meson will show us the error
# We set 'check: false' because we don't interest about its exit code
# Awk
message('Checking awk command ...')
run_command('awk', check: false)
message('Awk command found!')
# Pkill and pgrep (procps-ng package)
message('Checking pkill command ...')
run_command('pkill', check: false)
message('Pkill command found!')
message('Checking pgrep command ...')
run_command('pgrep', check: false)
message('Pgrep command found!')

# Unitd lib
libunitd = library(project_name,
                   'src/include/unitd.h', 'src/include/unitd_types.h', 'src/core/unitd_impl.h',
                   'src/core/init/init.c', 'src/core/logger/logger.c',
                   'src/core/common/common.c', 'src/core/socket/socket_client.c',
                   'src/core/socket/socket_server.c', 'src/core/socket/socket_common.c',
                   'src/core/socket/socket_request.c', 'src/core/socket/socket_response.c',
                   'src/core/parser/parser.c', 'src/core/units/units.c',
                   'src/core/commands/commands.c', 'src/core/processes/process.c',
                   'src/core/handlers/signals.c', 'src/core/handlers/cleaner.c', 'src/core/handlers/notifier.c',
                   dependencies: deps,
                   version: ver,
                   soversion: so_ver, install: true
                   )

# Generate pc file
pkg = import('pkgconfig')
pkg.generate(libunitd,
             description: 'Unitd init system library',
             subdirs: project_name
            )

# Install headers
install_headers('src/include/unitd.h', subdir : project_name)
install_headers('src/include/unitd_types.h', subdir : project_name)
install_headers('build/unitd_config.h', subdir : project_name)

# Install executables
executable(project_name, 'src/bin/unitd/main.c',
           link_with: libunitd,
           install: true,
           dependencies: deps,
           install_dir: sbin_path
          )

executable('unitctl', 'src/bin/unitctl/main.c',
           link_with: libunitd,
           install: true,
           dependencies: deps,
           install_dir: sbin_path
           )

executable('seedrng', 'src/extra/src/seedrng.c',
           install: true,
           install_dir: sbin_path
           )

# Install Configuration
install_data(project_name + '.conf', install_dir: unitd_conf_path)
install_data(project_name + '.conf', install_dir: unitd_data_path,
             rename: project_name + '.conf.sample')

# Install extra data
install_subdir('src/extra/functions', install_dir: unitd_data_path)
install_subdir('src/extra/scripts', install_dir: unitd_data_path)
install_subdir('src/extra/init.state', install_dir: unitd_data_path)
install_subdir('src/extra/final.state', install_dir: unitd_data_path)

# Install documentation
install_data('LICENSE', install_dir: doc_path)
install_data('README.md', install_dir: doc_path)

# Install bash completion
install_data('src/extra/completion/bash/unitctl', install_dir: bash_comp_path)

# Install zzz script
install_data('src/extra/scripts/zzz', install_dir: sbin_path)

# Install man pages
install_subdir('src/extra/man/man8', install_dir: mandir)

# Install mime-type
install_data('src/extra/mime-type/unit.xml', install_dir: prefix/datadir/'mime'/'packages')

# Install icon
install_data('src/extra/artwork/unit.svg', install_dir: prefix/datadir/'icons'/'hicolor'/'scalable')

# Install unitd logo
install_data('src/extra/artwork/unitd-logo.svg', install_dir: unitd_data_path/'artwork')

# Execute 'unitd-check' initialization unit to create the folders tree
env = environment()
env.set('UNITS_PATH', units_path)
env.set('UNITS_USER_PATH', units_user_path)
env.set('UNITS_ENAB_PATH', units_enab_path)
r = run_command('src/extra/init.state/scripts/unitd-check.sh', env: env, check: false)
if r.returncode() != 0
    message(r.stderr().strip())
    error('An error has occurred')
endif
