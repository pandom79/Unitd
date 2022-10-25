#(C) 2022 by Domenico Panella <pandom79@gmail.com>

#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License version 3.
#See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
#

sources = files(
                'include/unitlogd.h',
                'unitlogd_impl.h',
                'init/init.c',
                'init/init.h',
                'signals/signals.c',
                'signals/signals.h',
                'socket/socket.c',
                'socket/socket.h',
                'file/file.c',
                'file/file.h',
                'index/index.c',
                'index/index.h',
                'logline/logline.c',
                'logline/logline.h',
                'client/client.c',
                'client/client.h',
                )

deps = [libunitd_dep, dependency('threads'), dependency('wrapper')]

libunitlogd = library(unitlogd_name, sources, dependencies: deps, version: ver,
                      soversion: so_ver,
                      install: true)

executable('unitlogd', '../bin/unitlogd/main.c',
           link_with: libunitlogd,
           install: true,
           dependencies: deps,
           install_dir: sbin_path
          )

executable('unitlogctl', '../bin/unitlogctl/main.c',
           link_with: libunitlogd,
           install: true,
           dependencies: deps,
           install_dir: sbin_path
          )

# Install headers
install_headers('include/unitlogd.h', subdir: 'unitlogd')

# Install scripts
install_subdir('extra/scripts', install_dir: unitlogd_data_path)

# Install bash completion
install_data('../../completion/bash/unitlogctl', install_dir: bash_comp_path)
