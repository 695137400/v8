# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/lib.star", "GCLIENT_VARS", "RECLIENT", "ci_pair_factory", "in_console", "v8_builder", "v8_failure_notifier")

def experiment_builder(**kwargs):
    notify_owners = kwargs.pop("notify_owners", None)
    if notify_owners:
        builder_name = kwargs["name"]
        v8_failure_notifier(
            name = "notification for %s" % builder_name,
            notify_emails = notify_owners,
            notified_by = [builder_name],
        )
    bucket = kwargs.pop("bucket", "ci")
    properties = kwargs.pop("properties", {})
    if "builder_group" not in properties:
        properties["builder_group"] = "client.v8"
    experiments = kwargs.pop("experiments", {})
    experiments["v8.resultdb"] = 100
    return v8_builder(
        bucket = bucket,
        properties = properties,
        experiments = experiments,
        **kwargs
    )

in_category = in_console("experiments")
experiment_builder_pair = ci_pair_factory(experiment_builder)

in_category(
    "Features",
    experiment_builder_pair(
        name = "V8 Linux64 - cppgc-non-default - debug",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        use_remoteexec = RECLIENT.DEFAULT,
        notify_owners = ["mlippautz@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - debug - perfetto",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        use_remoteexec = RECLIENT.DEFAULT,
        notify_owners = ["skyostil@google.com"],
        notifies = ["blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - disable runtime call stats",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        use_remoteexec = RECLIENT.DEFAULT,
        notify_owners = ["cbruni@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - external code space - debug",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        use_remoteexec = RECLIENT.DEFAULT,
        notify_owners = ["ishell@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder(
        name = "V8 Linux64 - Fuzzilli - builder",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        use_remoteexec = RECLIENT.DEFAULT,
        notify_owners = ["saelo@google.com", "msarm@google.com"],
        notifies = ["blamelist"],
    ),
)

in_category(
    "Fuchsia",
    experiment_builder(
        name = "V8 Fuchsia",
        parent_builder = "V8 Fuchsia - builder",
        execution_timeout = 19800,
        notifies = ["v8-infra-cc"],
        # TODO(crbug.com/v8/12785): Enable again when it is green.
        work_in_progress = True,
    ),
)

in_category(
    "Coverage",
    experiment_builder(
        name = "V8 Linux64 - coverage",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {
            "enable_swarming": False,
            "gclient_vars": {"checkout_clang_coverage_tools": "True"},
            "coverage": "llvm",
        },
        use_remoteexec = RECLIENT.DEFAULT,
        notify_owners = ["machenbach@chromium.org"],
    ),
    experiment_builder(
        name = "V8 Linux64 - coverage - debug",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {
            "enable_swarming": False,
            "gclient_vars": {"checkout_clang_coverage_tools": "True"},
            "coverage": "llvm",
        },
        use_remoteexec = RECLIENT.DEFAULT,
        execution_timeout = 7200,
        notify_owners = ["machenbach@chromium.org"],
    ),
)

in_category(
    "FYI",
    experiment_builder_pair(
        name = "V8 Linux64 - arm64",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"target_arch": "arm", "target_bits": 64},
        triggered_by = ["v8-trigger"],
        use_remoteexec = RECLIENT.DEFAULT,
        notify_owners = ["clemensb@chromium.org"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - arm64 - debug",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"target_arch": "arm", "target_bits": 64},
        triggered_by = ["v8-trigger"],
        use_remoteexec = RECLIENT.DEFAULT,
        notify_owners = ["clemensb@chromium.org"],
    ),
    experiment_builder(
        name = "V8 Linux64 - debug - fyi",
        parent_builder = "V8 Linux64 - debug builder",
        execution_timeout = 19800,
        notify_owners = ["jgruber@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - no shared cage - debug",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        execution_timeout = 19800,
        use_remoteexec = RECLIENT.DEFAULT,
        notify_owners = ["jgruber@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder(
        name = "V8 Linux64 - fyi",
        parent_builder = "V8 Linux64 - builder",
        execution_timeout = 19800,
        notify_owners = ["jgruber@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder(
        name = "V8 Linux64 - PKU",
        parent_builder = "V8 Linux64 - builder",
        execution_timeout = 19800,
        notify_owners = ["sroettger@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 gcc",
        triggered_by = ["v8-trigger"],
        dimensions = {"host_class": "strong", "os": "Ubuntu-20.04|Ubuntu-22.04", "cpu": "x86-64"},
        use_remoteexec = RECLIENT.NO,
        notifies = ["sheriffs on new failure", "blamelist"],
    ),
    experiment_builder(
        name = "V8 Linux64 gcc - debug builder",
        triggered_by = ["v8-trigger"],
        dimensions = {"host_class": "strong", "os": "Ubuntu-20.04|Ubuntu-22.04", "cpu": "x86-64"},
        use_remoteexec = RECLIENT.NO,
        notifies = ["sheriffs on new failure", "blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - predictable",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["sheriffs on new failure", "blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - official",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["sheriffs on new failure", "blamelist"],
    ),
)

in_category(
    "Linux64 no sandbox",
    experiment_builder_pair(
        name = "V8 Linux64 - no sandbox",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        use_remoteexec = RECLIENT.DEFAULT,
        close_tree = False,
        triggered_by = ["v8-trigger"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - no sandbox - debug",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        use_remoteexec = RECLIENT.DEFAULT,
        close_tree = False,
        triggered_by = ["v8-trigger"],
    ),
)

in_category(
    "Mac",
    experiment_builder(
        name = "V8 iOS - sim - builder",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        properties = {"$depot_tools/osx_sdk": {"sdk_version": "14c18"}, "target_platform": "ios"},
        caches = [
            swarming.cache(
                path = "osx_sdk",
                name = "osx_sdk",
            ),
        ],
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["v8-infra-cc"],
    ),
    experiment_builder(
        name = "V8 Mac - arm64 - sim - debug",
        parent_builder = "V8 Mac - arm64 - sim - debug builder",
        execution_timeout = 19800,
        notifies = ["sheriffs on new failure", "blamelist"],
    ),
    experiment_builder(
        name = "V8 Mac - arm64 - sim - release",
        parent_builder = "V8 Mac - arm64 - sim - release builder",
        execution_timeout = 19800,
        notifies = ["sheriffs on new failure", "blamelist"],
    ),
    experiment_builder(
        name = "V8 Mac64 - full debug builder",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["sheriffs on new failure", "blamelist"],
    ),
)
