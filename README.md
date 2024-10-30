# bazel-clangd-helper

Utilities for making clangd nicer to use with Bazel.

## Installation

Add the following to `MODULE.bazel`:

```python
bazel_dep(name = "bazel_clangd_helper", dev_dependency = True)
git_override(
    module_name = "bazel_clangd_helper",
    commit = "<LATEST-COMMIT-ID>",  # Replace this.
    remote = "https://github.com/figurerobotics/bazel-clangd-helper.git",
)
```

## Compile commands generation

This generates a [JSON compilation database](https://clang.llvm.org/docs/JSONCompilationDatabase.html) from Bazel C/C++
targets.

```
bazel run @bazel_clangd_helper//:generate_compile_commands -- <YOUR-BAZEL-TARGETS>
```

e.g. to generate for all targets, run: `bazel run @bazel_clangd_helper//:generate_compile_commands -- //...`

Multiple targets can be specified. Internally, this will generate a compile command JSON fragment for each source file,
and then concatenate them into a `compile_commands.json` at the repository root. Incremental builds will be faster as
Bazel can leverage its cache to avoid regenerating unchanged parts of the build graph.
