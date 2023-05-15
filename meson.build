#(C) 2021 by Domenico Panella <pandom79@gmail.com>

#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License version 3.
#See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
#
project('Unitd', 'c', version: '6.0.3', default_options: ['warning_level=3', 'prefix=/usr'] )
add_global_arguments(['-Wno-incompatible-pointer-types'], language: 'c')

ver = '6.0.3'
so_ver = '6'
ulib_ver = '3.0.2'
project_name = 'unitd'
unitlogd_name = 'unitlogd'

# Get paths
prefix = get_option('prefix')
sysconfdir = get_option('sysconfdir')
datadir = get_option('datadir')
localstatedir = get_option('localstatedir')
sbin_path = get_option('sbindir')
libdir = get_option('libdir')
mandir = get_option('mandir')
build_root = meson.global_build_root()

# Get doxygen options
doxy_latex = get_option('DOXY_LATEX')
conf_doxy_latex = 'YES'
doxy_html = get_option('DOXY_HTML')
conf_doxy_html = 'YES'

# Setting unitd macro path
unitd_conf_path = sysconfdir/project_name
units_path = prefix/libdir/project_name/'units'
units_user_path = units_path/'user'
units_enab_path = unitd_conf_path/'units'
unitd_data_path = prefix/datadir/project_name
unitd_timer_data_path = unitd_data_path/'timer'
unitd_log_path = localstatedir/'log'/project_name+'.log'

# Setting unitlogd macro path
unitlogd_path = localstatedir/'log'/unitlogd_name
unitlogd_data_path = prefix/datadir/unitlogd_name
unitlogd_log_path = unitlogd_path/unitlogd_name+'.log'
unitlogd_index_path = unitlogd_path/'index.dat'
unitlogd_boot_log_path = unitlogd_path/'boot.log'
unitlogd_lock_path = unitlogd_path/'.lock'
unitlogd_kmsg_path = unitlogd_path/'kmsg.log'

# Setting build options macro paths
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
conf_data.set('UNITD_VER', ver)
conf_data.set('ULIB_VER', ulib_ver)
conf_data.set('OS_NAME', get_option('OS_NAME'))
conf_data.set('OUR_UTMP_FILE', our_utmp_file)
conf_data.set('OUR_WTMP_FILE', our_wtmp_file)
conf_data.set('UNITS_PATH', units_path)
conf_data.set('UNITS_USER_PATH', units_user_path)
conf_data.set('UNITS_ENAB_PATH', units_enab_path)
conf_data.set('UNITD_DATA_PATH', unitd_data_path)
conf_data.set('UNITD_TIMER_DATA_PATH', unitd_timer_data_path)
conf_data.set('UNITD_CONF_PATH', unitd_conf_path)
conf_data.set('UNITD_LOG_PATH', unitd_log_path)
conf_data.set('UNITD_TEST', get_option('UNITD_TEST'))
# Unitlogd
conf_data.set('UNITLOGD_PATH', unitlogd_path)
conf_data.set('UNITLOGD_DATA_PATH', unitlogd_data_path)
conf_data.set('UNITLOGD_LOG_PATH', unitlogd_log_path)
conf_data.set('UNITLOGD_INDEX_PATH', unitlogd_index_path)
conf_data.set('UNITLOGD_BOOT_LOG_PATH', unitlogd_boot_log_path)
conf_data.set('UNITLOGD_LOCK_PATH', unitlogd_lock_path)
conf_data.set('UNITLOGD_KMSG_PATH', unitlogd_kmsg_path)
configure_file(input: 'unitd_config.h.in', output: 'unitd_config.h', configuration: conf_data)

# Copy the configuration header
run_command('/bin/cp', 'build/unitd_config.h', 'src/include/.', check: true)

# Dependencies
rt_dep = meson.get_compiler('c').find_library('rt', required : true)
deps = [dependency('threads'), dependency('ulib', version: ulib_ver), rt_dep]
# Checking commands/packages dependencies
# Awk
find_program('awk', required: true)
# Pkill and pgrep (procps-ng package)
find_program('pkill', required: true)
find_program('pgrep', required: true)
# Doxygen
if doxy_html == true or doxy_latex == true
    find_program('doxygen', required: true)
    if (doxy_latex == false)
        conf_doxy_latex = 'NO'
    endif
    if (doxy_html == false)
        conf_doxy_html = 'NO'
    endif
    # Configuration
    conf_doxy = configuration_data()
    conf_doxy.set('BUILD_ROOT', build_root)
    conf_doxy.set('LATEX', conf_doxy_latex)
    conf_doxy.set('HTML', conf_doxy_html)
    configure_file(input: 'Doxyfile.in', output: 'Doxyfile', configuration: conf_doxy)
    message('Building the documentation ... ')
    run_command('doxygen', build_root/'Doxyfile', check: true)
endif

# Unitd sources
sources = files(
                'src/include/unitd_config.h',
                'src/include/unitd_types.h',
                'src/include/unitd.h',
                'src/core/unitd_impl.h',
                'src/core/init/init.c',
                'src/core/init/init.h',
                'src/core/logger/logger.c',
                'src/core/logger/logger.h',
                'src/core/common/common.c',
                'src/core/common/common.h',
                'src/core/parser/parser.c',
                'src/core/parser/parser.h',
                'src/core/units/units.c',
                'src/core/units/units.h',
                'src/core/units/utimers/utimers.c',
                'src/core/units/utimers/utimers.h',
                'src/core/units/upath/upath.c',
                'src/core/units/upath/upath.h',
                'src/core/commands/commands.c',
                'src/core/commands/commands.h',
                'src/core/processes/process.c',
                'src/core/processes/process.h',
                'src/core/handlers/signals.c',
                'src/core/handlers/signals.h',
                'src/core/handlers/cleaner.c',
                'src/core/handlers/cleaner.h',
                'src/core/handlers/notifier.c',
                'src/core/handlers/notifier.h',
                'src/core/socket/socket_client.c',
                'src/core/socket/socket_client.h',
                'src/core/socket/socket_server.c',
                'src/core/socket/socket_server.h',
                'src/core/socket/socket_common.c',
                'src/core/socket/socket_common.h',
                'src/core/socket/socket_request.c',
                'src/core/socket/socket_request.h',
                'src/core/socket/socket_response.c',
                'src/core/socket/socket_response.h',
                'src/core/file/file.c',
                'src/core/file/file.h'
                )

# Unitd lib
libunitd = library(project_name,
                   sources,
                   dependencies: deps,
                   version: ver,
                   soversion: so_ver, install: true
                  )

# Unitd lib dependency
libunitd_dep = declare_dependency(link_with: libunitd)

# Generate pc file
pkg = import('pkgconfig')
pkg.generate(libunitd,
             description: 'Unitd init system library',
             subdirs: project_name
            )

# Install headers
install_headers('src/include/unitd.h', subdir: project_name)
install_headers('src/include/unitd_types.h', subdir: project_name)
install_headers('src/include/unitd_config.h', subdir: project_name)

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

executable('seedrng', 'external/seedrng.c',
           install: true,
           install_dir: sbin_path
          )

# Unitlogd
subdir('src'/unitlogd_name)

# Install configuration
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

# Install doxy documentation
if (doxy_html == true)
    install_subdir(build_root/'html', install_dir: doc_path)
endif
if (doxy_latex == true)
    install_subdir(build_root/'latex', install_dir: doc_path)
endif

# Install bash completion
install_data('completion/bash/unitctl', install_dir: bash_comp_path)
install_data('completion/bash/unitd', install_dir: bash_comp_path)

# Install zzz script
install_data('external/zzz', install_dir: sbin_path)
run_command('ln', '-s', prefix/sbin_path/'zzz', prefix/sbin_path/'ZZZ', check: false)

# Install man pages
install_subdir('man/man8', install_dir: mandir)

# Install mime-type
install_data('mime-type/unit.xml', install_dir: prefix/datadir/'mime'/'packages')
install_data('mime-type/utimer.xml', install_dir: prefix/datadir/'mime'/'packages')
install_data('mime-type/upath.xml', install_dir: prefix/datadir/'mime'/'packages')

# Install icon
install_data('artwork/unit.svg', install_dir: prefix/datadir/'icons'/'hicolor'/'scalable'/'apps')
install_data('artwork/utimer.svg', install_dir: prefix/datadir/'icons'/'hicolor'/'scalable'/'apps')
install_data('artwork/upath.svg', install_dir: prefix/datadir/'icons'/'hicolor'/'scalable'/'apps')

# Install unitd logo
install_data('artwork/unitd-logo.svg', install_dir: unitd_data_path/'artwork')

# Execute 'unitd-check' initialization unit to create the folders tree
env = environment()
env.set('UNITS_PATH', units_path)
env.set('UNITS_USER_PATH', units_user_path)
env.set('UNITS_ENAB_PATH', units_enab_path)
env.set('UNITD_TIMER_DATA_PATH', unitd_timer_data_path)
r = run_command('src/extra/init.state/scripts/unitd-check.sh', env: env, check: false)
if r.returncode() != 0
    message(r.stderr().strip())
    error('An error has occurred')
endif
