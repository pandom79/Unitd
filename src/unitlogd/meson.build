#(C) 2021 by Domenico Panella <pandom79@gmail.com>

#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License version 3.
#See http://www.gnu.org/licenses/gpl-3.0.html for full license text.
#

sources = files(
                'unitlogd_impl.h',
                'common/common.c',
                'common/common.h',
                'signals/signals.c',
                'signals/signals.h',
                'socket/socket.c',
                'socket/socket.h',
                )

deps = [dependency('unitd'), dependency('wrapper')]

libunitlogd = library(unitlogd_name, sources, dependencies: deps, install: false)

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
