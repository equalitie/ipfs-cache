# IPFS Cache

A C++ wrapper over go-ipfs to store key/value pairs in the IPFS network.

## Requirements

To be able to use the IPFS Cache in platforms like Android, where running IPFS as an
independent daemon is not a possibility, the wrapper needs to embed IPFS by linking
directly with its Go code.  Thus the source of go-ipfs is needed to build the main
glue between C++ and IPFS.  Building that source requires a recent version of Go.  To
avoid extra system dependencies, the build process automatically downloads the Go
system and builds IPFS itself.

In summary, the minimum build dependencies are:

* `cmake` 3.5+
* `g++` capable of C++14
* The [Boost library](http://www.boost.org/)

For Debian, this translates to the following packages:

* `build-essential`
* `cmake`
* `curl`
* `libboost-system-dev`
* `libboost-coroutine-dev`
* `libboost-program-options-dev`

The build process is able to compile the IPFS Cache to different platforms with the
help of a properly configured cross-compilation environment.  If you actually intend
to cross-compile you will need proper C/C++ cross-compiler packages, Boost libraries
for the target system and a toolchain file for CMake to use them.

To the date, the build process has only been tested on 64-bit GNU/Linux platforms.

### Linux cross-compilation example

For building binaries in a Debian Strech machine which are able to run on _Raspbian
Stretch_ on the Raspberry Pi:

  - Install the `gcc-6-arm-linux-gnueabihf` and `g++-6-arm-linux-gnueabihf` packages.
  - As indicated in <https://wiki.debian.org/Multiarch/HOWTO>, add the new
    architecture with `dpkg --add-architecture armhf` and update your package list.
  - Install the Boost libraries matching the target distribution, with the proper
    architecture suffix:

      - `libboost-system1.62-dev:armhf`
      - `libboost-coroutine1.62-dev:armhf`
      - `libboost-program-options1.62-dev:armhf`

  - Create a toolchain file (e.g. `toolchain-linux-armhf-gcc6.cmake`) containing:

        set(CMAKE_SYSTEM_NAME Linux)
        set(CMAKE_SYSTEM_PROCESSOR armv6l)

        set(CMAKE_C_COMPILER /usr/bin/arm-linux-gnueabihf-gcc-6)
        set(CMAKE_CXX_COMPILER /usr/bin/arm-linux-gnueabihf-g++-6)

### Android cross-compilation example

For building binaries able to run in _Android KitKat and above on ARM_ processors you
will need a Clang/LLVM standalone toolchain created with the Android NDK.  Assuming
that the NDK is under `~/opt/android-ndk-r15c`, you may run:

    $ ~/opt/android-ndk-r15c/build/tools/make-standalone-toolchain.sh \
      --platform=android-19 --arch=arm --stl=libc++ \
      --install-dir=$HOME/opt/ndk-android19-arm-libcpp

You will also need to build the Boost libraries for this platform.  You may use
[Boost for Android](https://github.com/dec1/Boost-for-Android).  Assuming that Boost
source is in `~/src/boost/<BOOST_VERSION>`, edit `doIt.sh` and:

  - set `BOOST_SRC_DIR` to `$HOME/src/boost`
  - set `BOOST_VERSION` to the `<BOOST_VERSION>` above
  - set `GOOGLE_DIR` to `$HOME/opt/android-ndk-r15c`
  - modify `build-boost.sh` arguments, set `--version=$BOOST_VERSION`,
    `--stdlibs="llvm-3.5"`, `--linkage="shared"` and `--abis` to the desired
    architectures (`armeabi-v7a` in our example)

Create the `llvm-3.5` link as indicated in Boost for Android's readme and run
`./doIt.sh` to build the Boost libraries.  This will create the directory
`build/boost/<BOOST_VERSION>`.

After the previous steps you can use a CMake toolchain file like the following one:

    set(CMAKE_SYSTEM_NAME Android)
    set(CMAKE_SYSTEM_VERSION 19)
    set(CMAKE_ANDROID_ARCH_ABI armeabi-v7a)
    set(CMAKE_ANDROID_STANDALONE_TOOLCHAIN $ENV{HOME}/opt/ndk-android19-arm-libcpp)

    set(BOOST_INCLUDEDIR /path/to/Boost-for-Android/build/boost/<BOOST_VERSION>/include)
    set(BOOST_LIBRARYDIR /path/to/Boost-for-Android/build/boost/<BOOST_VERSION>/libs/${CMAKE_ANDROID_ARCH_ABI}/llvm-3.5)

## Building

```
$ cd <PROJECT ROOT>
$ mkdir build
$ cd build
$ cmake ..
$ make
```

On success, the _build_ directory shall contain the _libipfs-bindings--<SYS>--<PROC>.so_
and _libipfs-cache--<SYS>--<PROC>.so_ shared libraries and two example programs:
_injector--<SYS>--<PROC>_ and _client--<SYS>--<PROC>_.  `<SYS>` is CMake's
`CMAKE_SYSTEM_NAME` (e.g. `Linux`, `Android`...) while `<PROC>` is CMake's
`CMAKE_SYSTEM_PROCESSOR` (e.g. `x86_64`, `armv6l`...).

To cross-compile to another system, you may either create a different `build`
directory, or reuse the same directory and just remove the `CMakeCache.txt` file (thus
you can reuse some downloads and build tools).  Just remember to point CMake to the
proper toolchain file.  For the previous Raspbian example:

```
cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain-linux-armhf-gcc6.cmake ..
make
```

This will create the _libipfs-cache--Linux--armv6l.so_ library and similarly named
versions of the example programs.

## Using the examples

The _injector_ is a program which manipulates the IPFS key/value database. It does so
by running a very simplistic HTTP server which listens to requests for adding
new _(key, value)_ pairs into the database.

To start the injector listening on the TCP port 8080 start it as so:

```
$ ./injector --repo <PATH TO IPFS REPOSITORY> -p 8080
Swarm listening on /ip4/127.0.0.1/tcp/4002
Swarm listening on /ip4/<LAN-IP>/tcp/4002
Swarm listening on /ip4/<WAN-IP>/tcp/35038
Swarm listening on /ip4/<WAN-IP>/tcp/4002
Swarm listening on /ip6/::1/tcp/4002
Serving on port 0.0.0.0:8080
IPNS of this database is <DATABASE IPNS>
Starting event loop, press Ctrl-C to exit.
```

Make a note of the `<DATABASE IPNS>` string, it is the IPNS address which our
client will use to find the database in the IPFS network.

Each IPFS application requires a repository which stores a _config_ file and
data being shared on the IPFS network. If the path to the repository doesn't
exist, the application will try to create one.

To insert a new _(key, value)_ entry into the database, run the _curl_ command
with _key_ and _value_ variables set:

```
$ curl -d key=my_key -d value=my_value localhost:8080
```

When this command succeeds, we can have a look at the database by pointing our
browser to:

```
https://ipfs.io/ipns/<DATABASE IPNS>
```

Which may look something like this:

```
{"my_key":"<IPFS CONTENT ID>"}
```

Note that the _value_ is not stored in the database directly, instead, it can be found
in the IPFS network under `<IPFS CONTENT ID>`. We can again look it up with our
browser by following the link

```
https://ipfs.io/ipfs/<IPFS CONTENT ID>
```

Finally, to find the _value_ of a _key_ using the _client_ example program, one would
run it as so:

```
$ ./client --repo <PATH TO IPFS REPOSITORY> --ipns <DATABASE IPNS> --key my_key
```

**NOTE**: If the _client_ and the _injector_ programs are being executed on the
same PC, they need to start their IPFS services on different ports. Otherwise,
the program that executes later will not work properly. To make sure this
holds, modify the `Addresses => Swarm` JSON entries in either _client's_ or
_injector's_ `<PATH TO IPFS REPOSITORY>/config` file.


