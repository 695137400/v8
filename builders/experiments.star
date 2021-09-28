# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/lib.star", "GCLIENT_VARS", "GOMA", "v8_builder", "v8_notifier")

RECLIENT = struct(
    DEFAULT = {
        "instance": "rbe-chromium-trusted",
        "metrics_project": "chromium-reclient-metrics",
    },
    CACHE_SILO = {
        "instance": "rbe-chromium-trusted",
        "metrics_project": "chromium-reclient-metrics",
        "cache_silo": True,
    },
    COMPARE = {
        "instance": "rbe-chromium-trusted",
        "metrics_project": "chromium-reclient-metrics",
        "compare": True,
    },
)

def experiment_builder(**kwargs):
    to_notify = kwargs.pop("to_notify", None)
    if to_notify:
        builder_name = kwargs["name"]
        v8_notifier(
            name = "notification for %s" % builder_name,
            notify_emails = to_notify,
            notified_by = [builder_name],
        )

    v8_builder(
        in_console = "experiments/V8",
        **kwargs
    )

def experiment_builder_pair(name, **kwargs):
    properties = kwargs.pop("properties", None)
    dimensions = kwargs.pop("dimensions", None)
    triggered_by = kwargs.pop("triggered_by", None)
    use_goma = kwargs.pop("use_goma", None)
    use_rbe = kwargs.pop("use_rbe", None)

    to_notify = kwargs.pop("to_notify", None)
    if to_notify:
        builder_name = name
        v8_notifier(
            name = "notification for %s" % builder_name,
            notify_emails = to_notify,
            notified_by = [builder_name, builder_name + " - builder"],
        )

    v8_builder(
        in_console = "experiments/V8",
        name = name + " - builder",
        properties = properties,
        dimensions = dimensions,
        triggered_by = triggered_by,
        use_goma = use_goma,
        use_rbe = use_rbe,
        **kwargs
    )

    v8_builder(
        in_console = "experiments/V8",
        name = name,
        parent_builder = name + " - builder",
        properties = properties,
        **kwargs
    )

experiment_builder(
    name = "V8 iOS - sim",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Mac-10.15", "cpu": "x86-64"},
    properties = {"$depot_tools/osx_sdk": {"sdk_version": "12d4e"}, "target_platform": "ios", "builder_group": "client.v8"},
    caches = [
        swarming.cache(
            path = "osx_sdk",
            name = "osx_sdk",
        ),
    ],
    use_goma = GOMA.DEFAULT,
)

experiment_builder(
    name = "V8 Linux64 - bazel",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    executable = "recipe:v8/bazel",
    to_notify = [
        "victorgomes@chromium.org",
    ],
)

experiment_builder_pair(
    name = "V8 Linux64 - cppgc-non-default - debug",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.DEFAULT,
    to_notify = ["mlippautz@chromium.org"],
)

experiment_builder_pair(
    name = "V8 Linux64 - debug - perfetto",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.DEFAULT,
    to_notify = ["skyostil@google.com"],
)

experiment_builder(
    name = "V8 Linux64 - disable runtime call stats",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.DEFAULT,
    to_notify = ["cbruni@chromium.org"],
)

experiment_builder_pair(
    name = "V8 Linux64 - external code space - debug",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.DEFAULT,
    to_notify = ["ishell@chromium.org"],
)

experiment_builder(
    name = "V8 Linux64 gcc - debug",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    gclient_vars = [GCLIENT_VARS.V8_HEADER_INCLUDES],
    use_goma = GOMA.NO,
    to_notify = [
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
    ],
)

experiment_builder(
    name = "V8 Linux64 - gcov coverage",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"enable_swarming": False, "builder_group": "client.v8", "clobber": True, "coverage": "gcov"},
    use_goma = GOMA.NO,
    to_notify = [
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
    ],
)

experiment_builder(
    name = "V8 Linux64 - Fuzzilli",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.DEFAULT,
    to_notify = ["mvstanton@google.com", "msarm@google.com"],
)

experiment_builder(
    name = "V8 Linux64 - fyi",
    parent_builder = "V8 Linux64 - builder",
    bucket = "ci",
    dimensions = {"host_class": "multibot"},
    execution_timeout = 19800,
    properties = {"builder_group": "client.v8"},
    to_notify = ["jgruber@chromium.org"],
)

experiment_builder(
    name = "V8 Linux64 - debug - fyi",
    parent_builder = "V8 Linux64 - debug builder",
    bucket = "ci",
    dimensions = {"host_class": "multibot"},
    execution_timeout = 19800,
    properties = {"builder_group": "client.v8"},
    to_notify = ["jgruber@chromium.org"],
)

experiment_builder_pair(
    name = "V8 Linux64 - dict tracking - debug",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.DEFAULT,
    to_notify = ["ishell@chromium.org"],
)

experiment_builder_pair(
    name = "V8 Linux64 - arm64 - sim - heap sandbox - debug",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.DEFAULT,
    to_notify = [
        "saelo@chromium.org",
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
    ],
)

experiment_builder_pair(
    name = "V8 Linux64 - heap sandbox - debug",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.DEFAULT,
    to_notify = [
        "saelo@chromium.org",
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
    ],
)

experiment_builder_pair(
    name = "V8 Linux64 - debug - single generation",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.DEFAULT,
    to_notify = [
        "dinfuehr@chromium.org",
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
    ],
)

experiment_builder_pair(
    name = "V8 Linux64 - python3",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.DEFAULT,
    experiments = {"luci.recipes.use_python3": 100},
    to_notify = ["machenbach@chromium.org"],
)

experiment_builder(
    name = "V8 Linux64 - builder (reclient)",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.NO,
    use_rbe = RECLIENT.CACHE_SILO,
    to_notify = ["yyanagisawa@google.com"],
)

experiment_builder(
    name = "V8 Linux64 - builder (goma cache silo)",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.CACHE_SILO,
    to_notify = ["yyanagisawa@google.com"],
)

experiment_builder(
    name = "V8 Linux64 - builder (reclient compare)",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.NO,
    use_rbe = RECLIENT.COMPARE,
    to_notify = ["yyanagisawa@google.com"],
)

experiment_builder(
    name = "V8 Linux64 - node.js integration ng (reclient)",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    executable = "recipe:v8/node_integration_ng",
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"v8_tot": True, "builder_group": "client.v8"},
    use_rbe = RECLIENT.CACHE_SILO,
    to_notify = ["yyanagisawa@google.com"],
)

experiment_builder(
    name = "V8 Linux64 - node.js integration ng (goma cache silo)",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    executable = "recipe:v8/node_integration_ng",
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"v8_tot": True, "builder_group": "client.v8"},
    use_goma = GOMA.CACHE_SILO,
    to_notify = ["yyanagisawa@google.com"],
)

experiment_builder(
    name = "V8 Linux64 - node.js integration ng (reclient compare)",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    executable = "recipe:v8/node_integration_ng",
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"v8_tot": True, "builder_group": "client.v8"},
    use_goma = GOMA.NO,
    use_rbe = RECLIENT.COMPARE,
    to_notify = ["yyanagisawa@google.com"],
)

experiment_builder(
    name = "V8 Win32 - builder (reclient)",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Windows-10", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.NO,
    use_rbe = RECLIENT.CACHE_SILO,
    to_notify = ["yyanagisawa@google.com"],
)

experiment_builder(
    name = "V8 Win32 - builder (goma cache silo)",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Windows-10", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.CACHE_SILO,
    to_notify = ["yyanagisawa@google.com"],
)

experiment_builder(
    name = "V8 Win32 - builder (reclient compare)",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Windows-10", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.NO,
    use_rbe = RECLIENT.COMPARE,
    to_notify = ["yyanagisawa@google.com"],
)

experiment_builder(
    name = "V8 Linux gcc",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    gclient_vars = [GCLIENT_VARS.V8_HEADER_INCLUDES],
    use_goma = GOMA.NO,
    to_notify = [
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
    ],
)

experiment_builder(
    name = "V8 Linux - predictable",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Ubuntu-18.04", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.DEFAULT,
    to_notify = [
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
    ],
)

experiment_builder(
    name = "V8 Mac64 - full debug",
    bucket = "ci",
    triggered_by = ["v8-trigger"],
    dimensions = {"os": "Mac-10.15", "cpu": "x86-64"},
    properties = {"builder_group": "client.v8"},
    use_goma = GOMA.DEFAULT,
    # infra will be notified until this builder is not broken anymore
    notifies = ["infra"],
)

experiment_builder(
    name = "V8 Mac - arm64 - release",
    parent_builder = "V8 Mac - arm64 - release builder",
    bucket = "ci",
    dimensions = {"host_class": "multibot"},
    execution_timeout = 19800,
    properties = {"builder_group": "client.v8"},
    to_notify = [
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
    ],
)

experiment_builder(
    name = "V8 Mac - arm64 - debug",
    parent_builder = "V8 Mac - arm64 - debug builder",
    bucket = "ci",
    dimensions = {"host_class": "multibot"},
    execution_timeout = 19800,
    properties = {"builder_group": "client.v8"},
    to_notify = [
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
    ],
)

experiment_builder(
    name = "V8 Mac - arm64 - sim - release",
    parent_builder = "V8 Mac - arm64 - sim - release builder",
    bucket = "ci",
    dimensions = {"host_class": "multibot"},
    execution_timeout = 19800,
    properties = {"builder_group": "client.v8"},
    to_notify = [
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
    ],
)

experiment_builder(
    name = "V8 Mac - arm64 - sim - debug",
    parent_builder = "V8 Mac - arm64 - sim - debug builder",
    bucket = "ci",
    dimensions = {"host_class": "multibot"},
    execution_timeout = 19800,
    properties = {"builder_group": "client.v8"},
    to_notify = [
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
    ],
)
