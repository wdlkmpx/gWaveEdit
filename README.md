# How to compile

Run:
- `./autogen.sh` to generate configure
- ./configure --prefix=/usr
- make
- make install

# Requirements:
- gtk >= 2.24
- glib >= 2.32
- lame 3.93+

# Known bugs in gWaveEdit:

* When stopping, the cursor doesn't stop exactly where the playback stopped.

* Not highly tested on big-endian systems. Feedback on this is welcome. 

* The ALSA driver sometimes underrun shortly after playback started or 
  cursor jump, causing a click in the playback. 
