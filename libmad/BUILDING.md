# Building libmad

Building libmad is extremely simple. All you need is CMake, a build tool of
your choice (i.e., `make` on macOS or Linux, Visual Studio on Windows, `ninja`,
etc), and a working C compiler.

Once you have the above setup, perform the following steps in a terminal or
command prompt:

1. At the root of the source tree, create a new directory called `build/`.
2. Run `cd build` to enter the build directory.
3. Run `cmake ..`
4. Run `cmake --build . -j ncpus`

For the last step, replace `ncpus` with the number of CPU threads your system
has. Also for the last step, you can invoke the build tool yourself directly.
CMake generates build files for `make` by default on macOS and Linux, and files
for Visual Studio on Windows. If you want to use an alternative generator, such
as Ninja for example, just add `-G <generator name>` to step 3 above. For
example, if you wanted to use Ninja instead of the default generator, you would
add `-G Ninja`.

# Packaging the Source Code
First, follow the instructions above, until you reach step 4. Do not run any
commands after that command and do not follow the rest of the normal build
instructions, only return to this section when done. Then, run the following
command to package the source code:

```sh
$ cmake --build . --target package_source
```

Alternatively, you can use `cpack` instead in the following way:

```sh
$ cpack --config CPackSourceConfig.cmake
```

Either of the commands will perform the same task. After running either of the
above commands, you will get a variety of tarballs you can use, including a
`.tar.gz` tarball, a `.tar.bz2` tarball, a `.tar.xz` tarball, and even a
`.tar.Z` tarball. Whichever tarball you choose is solely your preference.

# Build Options

* `-DASO` (ON [default, if supported] | OFF): Enables architecture-specific
  optimizations. This onlys works for x86(_64), ARM, and MIPS targets.

* `-DBUILD_SHARED_LIBS` (ON [default] | OFF): Builds shared libraries. If
  disabled, builds static libraries.

* `-DCMAKE_BUILD_TYPE` (Debug, Release, RelWithDebInfo, MinSizeRel): Sets the
  build type. This is a standard CMake option.

* `-DEXAMPLE` (ON [default] | OFF): Builds the example `minimad`.

* `-DOPTIMIZE` (SPEED [default] | ACCURACY): Optimizes the build for either
  speed or accuracy.

* `-DPAD_VERSION_STRING` (ON | OFF [default]): Pads the version string with
  extra spaces to improve ABI compatibility with 0.15.1b.

# Supported Tool Versions

- **CMake**: libmad requires at least CMake 3.5 or later, but we strongly
  recommend version 3.10 or later.

- **Compiler**: Any C99 conformant compiler should work. All major compilers
  (GCC, Clang, MSVC) are supported, but other C99-conformant compilers should
  work as well.
