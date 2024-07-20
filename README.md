## FSV

[![travis ci](https://travis-ci.org/mcuelenaere/fsv.svg?branch=master)](https://travis-ci.org/mcuelenaere/fsv)

NOTE: this project is **not** actively maintained.

This repo is a fork of [fsv](http://fsv.sourceforge.net/), with some patches on top to align it more to the current state of affairs.
The original author is [Daniel Richard G.](http://fox.mit.edu/skunk/), a former student of Computer Science at the MIT.

**About fsv**

> fsv (pronounced eff-ess-vee) is a file system visualizer in cyberspace. It lays out files and directories in three dimensions, geometrically representing the file system hierarchy to allow visual overview and analysis. fsv can visualize a modest home directory, a workstation's hard drive, or any arbitrarily large collection of files, limited only by the host computer's memory and graphics hardware.

Its ancestor, SGI's `fsn` (pronounced "fusion") originated on IRIX and was prominently featured in Jurassic Park: ["It's a Unix system!"](https://www.youtube.com/watch?v=3HjOjvu6oKA). 

[Screenshots](http://fsv.sourceforge.net/screenshots/) of the original clone are still available.

Useful info and screenshots of the original SGI IRIX implementation are available on [siliconbunny](http://www.siliconbunny.com/fsn-the-irix-3d-file-system-tool-from-jurassic-park/).

**Install**
1. Install dependencies (Ubuntu/Debian): `sudo apt-get install libgtkgl2.0-dev libgl1-mesa-dev libglu1-mesa-dev`
2. Clone the repository
3. Run the configure script: `./autogen.sh`
4. Do the install dance:
    - `./configure`
    - `make`
    - `sudo make install`
