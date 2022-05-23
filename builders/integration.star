# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/lib.star", "GOMA", "in_console", "v8_builder")

in_category = in_console("integration")

in_category(
    "Layout",
    v8_builder(
        name = "V8 Blink Win",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        execution_timeout = 10800,
        properties = {"builder_group": "client.v8.fyi"},
        use_goma = GOMA.ATS,
        notifies = ["sheriffs with blamelist"],
    ),
    v8_builder(
        name = "V8 Blink Mac",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Mac-10.15", "cpu": "x86-64"},
        execution_timeout = 10800,
        properties = {"builder_group": "client.v8.fyi"},
        use_goma = GOMA.DEFAULT,
        notifies = ["sheriffs with blamelist"],
    ),
    v8_builder(
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
        use_goma = GOMA.DEFAULT,
        notifies = ["sheriffs with blamelist"],
    ),
    v8_builder(
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
        use_goma = GOMA.DEFAULT,
        notifies = ["sheriffs with blamelist"],
    ),
    v8_builder(
        name = "V8 Blink Linux Future",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        use_goma = GOMA.DEFAULT,
        notifies = ["sheriffs with blamelist"],
    ),
)

in_category(
    "Nonlayout",
    v8_builder(
        name = "Linux Debug Builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium",
        dimensions = {"host_class": "large_disk", "os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        use_goma = GOMA.DEFAULT,
    ),
    v8_builder(
        name = "V8 Linux GN",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        use_goma = GOMA.DEFAULT,
        notifies = ["sheriffs with blamelist"],
    ),
    v8_builder(
        name = "V8 Android GN (dbg)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        use_goma = GOMA.DEFAULT,
        notifies = ["sheriffs with blamelist"],
    ),
    v8_builder(
        name = "Linux ASAN Builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium",
        dimensions = {"host_class": "large_disk", "os": "Ubuntu-18.04", "cpu": "x86-64"},
        execution_timeout = 18000,
        properties = {"builder_group": "client.v8.fyi"},
        use_goma = GOMA.DEFAULT,
    ),
)

in_category(
    "GPU",
    v8_builder(
        name = "Win V8 FYI Release (NVIDIA)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        execution_timeout = 10800,
        properties = {"builder_group": "client.v8.fyi"},
        use_goma = GOMA.ATS,
    ),
    v8_builder(
        name = "Mac V8 FYI Release (Intel)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Mac-10.15", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        use_goma = GOMA.DEFAULT,
    ),
    v8_builder(
        name = "Linux V8 FYI Release (NVIDIA)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        use_goma = GOMA.DEFAULT,
    ),
    v8_builder(
        name = "Linux V8 FYI Release - pointer compression (NVIDIA)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        use_goma = GOMA.DEFAULT,
    ),
    v8_builder(
        name = "Android V8 FYI Release (Nexus 5X)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        execution_timeout = 10800,
        properties = {"builder_group": "client.v8.fyi"},
        use_goma = GOMA.DEFAULT,
    ),
)

in_category(
    "Node.js",
    v8_builder(
        name = "V8 Linux64 - node.js integration ng",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:v8/node_integration_ng",
        dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
        properties = {"v8_tot": True, "builder_group": "client.v8.fyi"},
        use_goma = GOMA.DEFAULT,
        notifies = ["sheriffs with blamelist"],
    ),
)
