This repo is an update of [fsv](http://fsv.sourceforge.net/) for modern systems (GTK2).

> fsv (pronounced eff-ess-vee) is a file system visualizer in cyberspace. It lays out files and directories in three dimensions, geometrically representing the file system hierarchy to allow visual overview and analysis. fsv can visualize a modest home directory, a workstation's hard drive, or any arbitrarily large collection of files, limited only by the host computer's memory and graphics hardware.

`fsv` was featured in Jurassic Park: ["It's a Unix system!"](https://www.youtube.com/watch?v=3HjOjvu6oKA). Original site has [screenshots](http://fsv.sourceforge.net/screenshots/).

**Install**

1. Clone the repository
2. Make a configure script: `./autogen.sh`
3. Install dependencies (Ubuntu): `sudo apt-get install libgtk2.0-dev libgl1-mesa-dev gtkgl-2.0-dev libgtkgl2.0-dev libglu1-mesa-dev`
4. Do the install dance:

    - ./configure
	- make
	- sudo make install
