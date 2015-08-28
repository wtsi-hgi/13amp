# 13 Amp

A userspace CRAM-to-BAM translation virtual filesystem, which trades the
cost of disk space for CPU, memory and time (*so much* time!).

## Quick Build (with pkg-config)

1. Set your `PKG_CONFIG_PATH` appropriately (e.g.
   `export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$(echo ~/local/lib/pkgconfig)`)

2. If the source tree was cloned from the git repository, run:
   `./autogen.sh` (not required if you unpacked from a tar distribution)

3. Make and change to a `build` subdirectory: `mkdir build && cd build`

4. Run `configure` (setting `--prefix` appropriately; in this case,
   installing into `~/local`):

   ```sh
   ../configure HTSLIB_LDFLAGS="$(pkg-config --libs htslib)" \
                 HTSLIB_CFLAGS="$(pkg-config --cflags htslib)" \
                 FUSE_LDFLAGS="$(pkg-config --libs fuse)" \
                 FUSE_CFLAGS="$(pkg-config --cflags fuse)" \
                 --prefix=$(echo ~/local)
   ```

5. `make install`

## Etymology

13 Amp uses FUSE (Filesystem in USErspace) to provide the filesystem
layer; therein lies the obvious connection. Moreover, 13 is the ASCII
and Unicode character code for a carriage return, or `CR`. Hence
"cramp", an excruciating pain caused by involuntary contraction, which
happens to contain the word "CRAM".

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
