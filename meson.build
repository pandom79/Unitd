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
conf_data.set('UNITS_PATH', units_path)
conf_data.set('UNITS_ENAB_PATH', units_enab_path)
conf_data.set('UNITD_DATA_PATH', unitd_data_path)
conf_data.set('UNITD_CONF_PATH', unitd_conf_path)
conf_data.set('UNITD_LOG_PATH', unitd_log_path)

configure_file(input: 'unitd_config.h.in', output: 'unitd_config.h', configuration: conf_data)

deps = [dependency('threads')]

libunitd = library('unitd',
                   'src/include/unitd.h', 'src/include/unitd_types.h', 'src/core/unitd_impl.h',
                   'src/core/init/init.c', 'src/core/logger/logger.c',
                   'src/core/wrapper/array_wrapper.c', 'src/core/wrapper/string_wrapper.c',                   
                   'src/core/common/common.c', 'src/core/socket/socket_client.c',
                   'src/core/socket/socket_server.c', 'src/core/socket/socket_common.c',
                   'src/core/socket/socket_request.c', 'src/core/socket/socket_response.c',
                   'src/core/parser/parser.c', 'src/core/units/units.c',
                   'src/core/commands/commands.c', 'src/core/processes/process.c',
                   'src/core/handlers/signals_handler.c',
                   dependencies: deps,
                   version: '1.0.0',
                   soversion: '0', install: true
                   )

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

install_data(project_name + '.conf', install_dir: unitd_conf_path)
install_data(project_name + '.conf', install_dir: unitd_data_path,
             rename: project_name + '.conf.sample')

install_subdir('src/extra/functions', install_dir : unitd_data_path)
install_subdir('src/extra/scripts', install_dir : unitd_data_path)
install_subdir('src/extra/init.state', install_dir : unitd_data_path)
install_subdir('src/extra/final.state', install_dir : unitd_data_path)
