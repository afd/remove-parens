# remove-parens

Download a built release of Clang/LLVM version 13.0.1 from GitHub and put it into third_party/clang+llvm-13.0.1

After this, you should find:

```
ls third_party/clang+llvm-13.0.1/
```

yields:

```
bin  include  lib  libexec  share
```

Then:

```
mkdir build
cd build
cmake -G Ninja ..
ninja
```

Then you can do:

```
./remove-parens /path/to/source/file --
```

If your source file will include standard headers, then you will need to copy the `remove-parens` executable into:

```
third_party/clang+llvm-13.0.1/bin/
```

and run it from there.
