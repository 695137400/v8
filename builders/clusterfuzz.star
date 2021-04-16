# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/lib.star", "GCLIENT_VARS", "GOMA", "in_console", "v8_builder")

def clusterfuzz_builder(close_tree=True, **kwargs):
    kwargs["close_tree"] = close_tree
    return v8_builder(**kwargs)

in_category = in_console("clusterfuzz")

in_category(
    "Windows",
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Win64 ASAN - release builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.clusterfuzz", "clobber": True, "clusterfuzz_archive": {"bitness": "64", "bucket": "v8-asan", "name": "d8-asan"}, "default_targets": ["v8_clusterfuzz"]},
        use_goma = GOMA.ATS,
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Win64 ASAN - debug builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.clusterfuzz", "clobber": True, "clusterfuzz_archive": {"bitness": "64", "bucket": "v8-asan", "name": "d8-asan"}, "default_targets": ["v8_clusterfuzz"]},
        use_goma = GOMA.ATS,
    ),
)

in_category(
    "Mac",
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Mac64 ASAN - release builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Mac-10.15", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.clusterfuzz", "clobber": True, "clusterfuzz_archive": {"bucket": "v8-asan", "name": "d8-asan"}, "default_targets": ["v8_clusterfuzz"]},
        use_goma = GOMA.DEFAULT,
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Mac64 ASAN - debug builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Mac-10.15", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.clusterfuzz", "clobber": True, "clusterfuzz_archive": {"bucket": "v8-asan", "name": "d8-asan"}, "default_targets": ["v8_clusterfuzz"]},
        use_goma = GOMA.DEFAULT,
    ),
)

in_category(
    "Linux",
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 - release builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Ubuntu-16.04", "cpu": "x86-64"},
        properties = {"triggers": ["V8 NumFuzz"], "builder_group": "client.v8.clusterfuzz", "clusterfuzz_archive": {"bucket": "v8-asan", "name": "d8"}, "default_targets": ["v8_clusterfuzz"]},
        use_goma = GOMA.DEFAULT,
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 - debug builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Ubuntu-16.04", "cpu": "x86-64"},
        properties = {"triggers": ["V8 NumFuzz - debug"], "builder_group": "client.v8.clusterfuzz", "clusterfuzz_archive": {"bucket": "v8-asan", "name": "d8"}, "default_targets": ["v8_clusterfuzz"]},
        use_goma = GOMA.DEFAULT,
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 ASAN no inline - release builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Ubuntu-16.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.clusterfuzz", "clobber": True, "clusterfuzz_archive": {"bucket": "v8-asan", "name": "d8-asan-no-inline"}, "default_targets": ["v8_clusterfuzz"]},
        use_goma = GOMA.DEFAULT,
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 ASAN - debug builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Ubuntu-16.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.clusterfuzz", "clobber": True, "clusterfuzz_archive": {"bucket": "v8-asan", "name": "d8-asan"}, "default_targets": ["v8_clusterfuzz"]},
        use_goma = GOMA.DEFAULT,
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 ASAN arm64 - debug builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Ubuntu-16.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.clusterfuzz", "clobber": True, "clusterfuzz_archive": {"bucket": "v8-asan", "name": "d8-arm64-asan"}, "default_targets": ["v8_clusterfuzz"]},
        use_goma = GOMA.DEFAULT,
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux - debug builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Ubuntu-16.04", "cpu": "x86-64"},
        properties = {"triggers": ["V8 NumFuzz - debug"], "builder_group": "client.v8.clusterfuzz", "clusterfuzz_archive": {"bucket": "v8-asan", "name": "d8"}, "default_targets": ["v8_clusterfuzz"], "target_bits": 32},
        use_goma = GOMA.DEFAULT,
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux ASAN arm - debug builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Ubuntu-16.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.clusterfuzz", "clobber": True, "clusterfuzz_archive": {"bucket": "v8-asan", "name": "d8-arm-asan"}, "default_targets": ["v8_clusterfuzz"]},
        use_goma = GOMA.DEFAULT,
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux MSAN no origins",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Ubuntu-16.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.clusterfuzz", "clobber": True, "default_targets": ["v8_clusterfuzz"], "clusterfuzz_archive": {"bucket": "v8-msan", "name": "d8-msan-no-origins"}},
        gclient_vars = [GCLIENT_VARS.INSTRUMENTED_LIBRARIES],
        use_goma = GOMA.DEFAULT,
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux MSAN chained origins",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Ubuntu-16.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.clusterfuzz", "clobber": True, "default_targets": ["v8_clusterfuzz"], "clusterfuzz_archive": {"bucket": "v8-msan", "name": "d8-msan-chained-origins"}},
        gclient_vars = [GCLIENT_VARS.INSTRUMENTED_LIBRARIES],
        use_goma = GOMA.DEFAULT,
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 CFI - release builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Ubuntu-16.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.clusterfuzz", "clobber": True, "clusterfuzz_archive": {"bucket": "v8-cfi", "name": "d8-cfi"}, "default_targets": ["v8_clusterfuzz"]},
        use_goma = GOMA.DEFAULT,
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 TSAN - release builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Ubuntu-16.04", "cpu": "x86-64"},
        properties = {"default_targets": ["v8_clusterfuzz"], "builder_group": "client.v8.clusterfuzz", "triggers": ["V8 NumFuzz - TSAN"]},
        use_goma = GOMA.DEFAULT,
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 UBSan - release builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.GREEDY_BATCHING_KIND,
            max_batch_size = 1,
        ),
        dimensions = {"os": "Ubuntu-16.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.clusterfuzz", "clobber": True, "clusterfuzz_archive": {"bucket": "v8-ubsan", "name": "d8-ubsan"}, "default_targets": ["v8_clusterfuzz"]},
        use_goma = GOMA.DEFAULT,
    ),
)

in_category(
    "Fuzzers",
    v8_builder(
        name = "V8 NumFuzz",
        bucket = "ci",
        dimensions = {"host_class": "multibot"},
        execution_timeout = 19800,
        properties = {"builder_group": "client.v8.clusterfuzz"},
        close_tree = False,
        notifies = ["memory sheriffs"],
    ),
    v8_builder(
        name = "V8 NumFuzz - debug",
        bucket = "ci",
        dimensions = {"host_class": "multibot"},
        execution_timeout = 19800,
        properties = {"builder_group": "client.v8.clusterfuzz"},
        close_tree = False,
        notifies = ["memory sheriffs"],
    ),
    v8_builder(
        name = "V8 NumFuzz - TSAN",
        bucket = "ci",
        dimensions = {"host_class": "multibot"},
        execution_timeout = 19800,
        properties = {"builder_group": "client.v8.clusterfuzz"},
        close_tree = False,
        notifies = ["memory sheriffs"],
    ),
)
