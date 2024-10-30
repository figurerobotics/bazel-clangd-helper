load(
    "@bazel_tools//tools/build_defs/cc:action_names.bzl",
    "CPP_COMPILE_ACTION_NAME",
    "C_COMPILE_ACTION_NAME",
)
load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain")

_CPP_EXTENSIONS = ["cc", "cpp", "cxx", "hh", "hpp", "hxx", "ipp"]

def _is_cpp_srcs(srcs):
    if any([src.extension in _CPP_EXTENSIONS for src in srcs]):
        return True
    if all([src.extension == "h" for src in srcs]):  # Header-only.
        return True
    return False

def _get_toolchain_flags(ctx, is_cpp):
    cc_toolchain = find_cpp_toolchain(ctx)
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = ctx.disabled_features,
    )
    if is_cpp:
        return cc_common.get_memory_inefficient_command_line(
            feature_configuration = feature_configuration,
            action_name = CPP_COMPILE_ACTION_NAME,
            variables = cc_common.create_compile_variables(
                feature_configuration = feature_configuration,
                cc_toolchain = cc_toolchain,
                user_compile_flags = ctx.fragments.cpp.cxxopts + ctx.fragments.cpp.copts,
                add_legacy_cxx_options = True,
            ),
        )
    return cc_common.get_memory_inefficient_command_line(
        feature_configuration = feature_configuration,
        action_name = C_COMPILE_ACTION_NAME,
        variables = cc_common.create_compile_variables(
            feature_configuration = feature_configuration,
            cc_toolchain = cc_toolchain,
            user_compile_flags = ctx.fragments.cpp.copts,
        ),
    )

def _compile_commands_aspect_impl(target, ctx):
    if CcInfo not in target:
        return []

    srcs = []
    if hasattr(ctx.rule.attr, "srcs"):
        for src in ctx.rule.attr.srcs:
            # Ignore generated files.
            srcs += [f for f in src.files.to_list() if f.is_source]

    is_cpp = _is_cpp_srcs(srcs)

    common_args = ctx.actions.args()

    # Set a placeholder for the workspace directory which will be replaced later.
    # We avoid passing in the workspace directory via flag or environment variable to avoid
    # discarding the analysis cache.
    # TODO(michaelahn): Consider a better way to get the workspace directory from bazel.
    common_args.add("--directory", "__BAZEL_WORKSPACE_DIR__")

    # Positional args.
    common_args.add("--")
    common_args.add_all(_get_toolchain_flags(ctx, is_cpp))

    compile_context = target[CcInfo].compilation_context
    for define in compile_context.defines.to_list() + compile_context.local_defines.to_list():
        common_args.add("-D" + define)
    common_args.add_all(compile_context.quote_includes.to_list(), before_each = "-iquote")
    common_args.add_all(compile_context.system_includes.to_list(), before_each = "-isystem")
    common_args.add_all(compile_context.external_includes.to_list(), before_each = "-isystem")
    for include in compile_context.includes.to_list():
        common_args.add("-I" + include)
    for include in compile_context.framework_includes.to_list():
        common_args.add("-F" + include)

    output_files = []
    for src in srcs:
        args = ctx.actions.args()
        args.add("--source_path", src.short_path)
        output_file = ctx.actions.declare_file(
            "{}.{}.compile_commands.json".format(target.label.name, src.basename),
        )
        args.add("--output_path", output_file)

        ctx.actions.run(
            outputs = [output_file],
            arguments = [args, common_args],
            executable = ctx.executable._generate_executable,
            mnemonic = "GenerateCompileCommands",
            progress_message = "Generating compile commands for {}".format(src.short_path),
        )
        output_files.append(output_file)

    return [OutputGroupInfo(report = depset(direct = output_files))]

compile_commands_aspect = aspect(
    implementation = _compile_commands_aspect_impl,
    fragments = ["cpp"],
    attrs = {
        "_cc_toolchain": attr.label(default = Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
        "_generate_executable": attr.label(
            default = Label("//:generate_compile_command"),
            cfg = "exec",
            executable = True,
            allow_files = True,
        ),
    },
    toolchains = ["@bazel_tools//tools/cpp:toolchain_type"],
)
