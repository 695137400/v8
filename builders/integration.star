# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/lib.star", "GOMA", "RECLIENT", "in_console", "v8_builder")

def integration_builder(**kwargs):
    return v8_builder(disable_resultdb_exports = True, use_goma = GOMA.NO, **kwargs)

in_category = in_console("integration")

in_category(
    "Layout",
    integration_builder(
        name = "V8 Blink Win",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        execution_timeout = 10800,
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["sheriffs"],
    ),
    integration_builder(
        name = "V8 Blink Mac",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        execution_timeout = 10800,
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["sheriffs"],
    ),
    integration_builder(
        name = "V8 Blink Linux",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_concurrent_invocations = 3,
            max_batch_size = 1,
        ),
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["sheriffs"],
        close_tree = True,
    ),
    integration_builder(
        name = "V8 Blink Linux Debug",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.LOGARITHMIC_BATCHING_KIND,
            log_base = 2.0,
        ),
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["sheriffs"],
    ),
    integration_builder(
        name = "V8 Blink Linux Future",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["sheriffs"],
    ),
)

in_category(
    "Nonlayout",
    integration_builder(
        name = "Linux Debug Builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium",
        dimensions = {"host_class": "large_disk", "os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["v8-infra-cc"],
    ),
    integration_builder(
        name = "V8 Linux GN",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["sheriffs"],
    ),
    integration_builder(
        name = "V8 Android GN (dbg)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["sheriffs"],
    ),
    integration_builder(
        name = "Linux ASAN Builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium",
        dimensions = {"host_class": "large_disk", "os": "Ubuntu-18.04", "cpu": "x86-64"},
        execution_timeout = 18000,
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["v8-infra-cc"],
    ),
)

in_category(
    "GPU",
    integration_builder(
        name = "Win V8 FYI Release (NVIDIA)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        execution_timeout = 10800,
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["v8-infra-cc"],
    ),
    integration_builder(
        name = "Mac V8 FYI Release (Intel)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        execution_timeout = 10800,
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["v8-infra-cc"],
    ),
    integration_builder(
        name = "Linux V8 FYI Release (NVIDIA)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        execution_timeout = 10800,
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["v8-infra-cc"],
    ),
    integration_builder(
        name = "Linux V8 FYI Release - pointer compression (NVIDIA)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        execution_timeout = 10800,
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["v8-infra-cc"],
    ),
    integration_builder(
        name = "Android V8 FYI Release (Nexus 5X)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        execution_timeout = 10800,
        properties = {"builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["v8-infra-cc"],
    ),
)

in_category(
    "Node.js",
    integration_builder(
        name = "V8 Linux64 - node.js integration ng",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:v8/node_integration_ng",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"v8_tot": True, "builder_group": "client.v8.fyi"},
        use_remoteexec = RECLIENT.DEFAULT,
        notifies = ["sheriffs"],
    ),
)
