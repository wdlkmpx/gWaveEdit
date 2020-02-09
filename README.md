# How to compile

Run:
- `./autogen.sh` to generate configure
- ./configure --prefix=/usr
- make
- make install

# Abandoned GTK+ 3.0 port

This was an effort to port gwaveedit to GTK+ 3.0, abandoned in the middle of nowhere. A lot of work done, learned a few lessons about the history of GTK+ but it's still a long way to go ...

Basically I tried to remove deprecated functions in GTK+ 2.0. It compiles, but not with `-Wall -DGTK_DISABLE_DEPRECATED` yet. `-DGDK_DISABLE_DEPRECATED` is also waiting.
