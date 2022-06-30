#(C) 2021 by Domenico Panella <pandom79@gmail.com>

#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License version 3.
#See http://www.gnu.org/licenses/gpl-3.0.html for full license text.

project('Unitd', 'c', version: '1.0.0', default_options: ['warning_level=3', 'prefix=/usr'] )

add_global_arguments(['-Wno-incompatible-pointer-types'], language: 'c')

project_name = 'unitd'
log_name = 'unitd.log'
unitd_conf_path = get_option('sysconfdir')/project_name

sbin_path = get_option('sbindir')
units_path = get_option('prefix')/get_option('libdir')/project_name/'units'
units_enab_path = unitd_conf_path/'units'
unitd_data_path = get_option('prefix')/get_option('datadir')/project_name
unitd_log_path = get_option('localstatedir')/'log'/log_name

conf_data = configuration_data()
conf_data.set('OS_NAME', get_option('OS_NAME'))
conf_data.set('OUR_UTMP_FILE', get_option('OUR_UTMP_FILE'))
conf_data.set('OUR_WTMP_FILE', get_option('OUR_WTMP_FILE'))
conf_data.set('UNITS_PATH', units_path)
conf_data.set('UNITS_ENAB_PATH', units_enab_path)
conf_data.set('UNITD_DATA_PATH', unitd_data_path)
conf_data.set('UNITD_CONF_PATH', unitd_conf_path)
conf_data.set('UNITD_LOG_PATH', unitd_log_path)
conf_data.set('UNITD_TEST', get_option('UNITD_TEST'))
configure_file(input: 'unitd_config.h.in', output: 'unitd_config.h', configuration: conf_data)

# Dependencies
libwrapper = meson.get_compiler('c').find_library('libwrapper', required : true)
deps = [dependency('threads'), libwrapper]

# Unitd lib
libunitd = library('unitd',
                   'src/include/unitd.h', 'src/include/unitd_types.h', 'src/core/unitd_impl.h',
                   'src/core/init/init.c', 'src/core/logger/logger.c',
                   'src/core/common/common.c', 'src/core/socket/socket_client.c',
                   'src/core/socket/socket_server.c', 'src/core/socket/socket_common.c',
                   'src/core/socket/socket_request.c', 'src/core/socket/socket_response.c',
                   'src/core/parser/parser.c', 'src/core/units/units.c',
                   'src/core/commands/commands.c', 'src/core/processes/process.c',
                   'src/core/handlers/signals.c', 'src/core/handlers/cleaner.c', 'src/core/handlers/notifier.c',
                   dependencies: deps,
                   version: '1.0.0',
                   soversion: '0', install: true
                   )

# Headers
install_headers('src/include/unitd.h', subdir : 'unitd')
install_headers('src/include/unitd_types.h', subdir : 'unitd')
install_headers('build/unitd_config.h', subdir : 'unitd')

# Executables
executable('unitd', 'src/bin/unitd/main.c',
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

# Configuration
install_data(project_name + '.conf', install_dir: unitd_conf_path)
install_data(project_name + '.conf', install_dir: unitd_data_path,
             rename: project_name + '.conf.sample')
install_subdir('src/extra/functions', install_dir : unitd_data_path)
install_subdir('src/extra/scripts', install_dir : unitd_data_path)
install_subdir('src/extra/init.state', install_dir : unitd_data_path)
install_subdir('src/extra/final.state', install_dir : unitd_data_path)

#zzz script
install_data('src/extra/scripts/zzz', install_dir : sbin_path)
install_man('src/extra/man/man8/zzz.8')
