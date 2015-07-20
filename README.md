# 13 Amp

A userspace CRAM-to-BAM translation virtual filesystem.

# Quick compile / install (for those with pkg-config)

1. Set `PKG_CONFIG_PATH` appropriately (e.g. `export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$(echo ~/local/lib/pkgconfig)`)

2. If source tree was cloned from git repository, run: `./autogen.sh` (not required if you unpacked from a tar distribution)

3. Make and change to `build` subdirectory: `mkdir build && cd build`

4. Run configure (setting `--prefix` appropraitely - in this case installing into `~/local`): `../configure HTSLIB_LDFLAGS="$(pkg-config --libs htslib)" HTSLIB_CFLAGS="$(pkg-config --cflags htslib)" FUSE_LDFLAGS="$(pkg-config --libs fuse)" FUSE_CFLAGS="$(pkg-config --cflags fuse)" --prefix=$(echo ~/local)`

5. Run make install: `make install`

# License

Copyright (c) 2015 Genome Research Limited

This program is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <http://www.gnu.org/licenses/>.
