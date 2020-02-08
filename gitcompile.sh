#! /bin/sh

# Script to compile mhWaveEdit from a Git master branch commit
# Configure options can be given on command line
# Requires autoconf, automake and some other stuff
#
# Users building from a release tarball do not need to use this script
#  The standard ./configure followed by make flow can be used there instead.
#

libtoolize --copy --force || exit 1
aclocal --force -I m4
autoheader --force
touch config.rpath
automake --add-missing --copy
autoconf --force
./configure $*
make -C po mhwaveedit.pot-update
make
