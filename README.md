# Stream Balance Analyzer



## Dependencies
llvm-10: https://releases.llvm.org/download.html#10.0.0
(Download the monorepo llvm-project)

```
mkdir build
cmake ../llvm -DLLVM_ENABLE_PROJECTS=clang
make -jN
```