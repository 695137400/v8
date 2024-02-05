# Copyright 2021 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def consoles_map(ctx):
    """
    Returns a mapping of console ID to console config.
    """
    consoles = dict()
    milo = ctx.output["luci-milo.cfg"]
    for console in milo.consoles:
        consoles[console.id] = console
    return consoles

def aggregate_builder_tester_console(ctx):
    """
    This callback collects and groups all builders by parent name in separate
    categories containing both the parent and the children to later add this
    categories to the `builder/tester` console. The purpose of the console is
    to make the parent/child relationship evident.

    Warning! The callback needs to run before the `parent_builder` property
    gets removed, before `ensure_forward_triggering_properties` callback.
    """
    build_bucket = ctx.output["cr-buildbucket.cfg"]
    categories = {}
    for bucket in build_bucket.buckets:
        if bucket.name == "ci":
            for builder in bucket.swarming.builders:
                if builder.properties:
                    properties = json.decode(builder.properties)
                    parent = properties.pop("parent_builder", None)
                    if parent:
                        contents = categories.get(parent, [])
                        if contents:
                            contents.append(builder.name)
                        else:
                            categories[parent] = [parent, builder.name]
    consoles = consoles_map(ctx)
    for category, contents in categories.items():
        for builder in contents:
            cbuilder = dict(
                name = "buildbucket/luci.v8.ci/" + builder,
                category = category,
            )
            consoles["builder-tester"].builders.append(cbuilder)

def separate_builder_tester_console(ctx):
    """
    Two console duplicates, one with all artifact builders and one with all
    testers separated.
    """
    consoles = consoles_map(ctx)
    for name in consoles:
        builders_name = name + "-builders"
        testers_name = name + "-testers"
        if builders_name in consoles and testers_name in consoles:
            consoles[builders_name].builders = [
                builder
                for builder in consoles[name].builders
                if is_artifact_builder(builder)
            ]
            consoles[testers_name].builders = [
                tester
                for tester in consoles[name].builders
                if not is_artifact_builder(tester)
            ]

def headless_consoles(ctx):
    """
    Console duplicates without headers for being included in an iFrame-based
    dashboard.
    """
    consoles = consoles_map(ctx)
    for name in consoles:
        headless_name = name + "-headless"
        if headless_name in consoles:
            consoles[headless_name].builders = list(consoles[name].builders)

def is_artifact_builder(builder):
    return builder.name.endswith("builder")

def is_release_builder(builder):
    return "debug" not in builder.name

BUILDER_TESTER_SPLICER = struct(
    decision_func = is_artifact_builder,
    left_category = "builder",
    right_category = "tester",
)

RELEASE_DEBUG_SPLICER = struct(
    decision_func = is_release_builder,
    left_category = "rel",
    right_category = "dbg",
)

def builder_with_category(builder, add_sub_cat, splicer):
    category = builder.category
    if splicer.decision_func(builder):
        subcat = splicer.left_category
    else:
        subcat = splicer.right_category
    if add_sub_cat:
        category += ("|" + subcat)
    return dict(
        name = builder.name,
        category = category,
    )

def splice_by_categories(builders, splicer):
    # Key builder lists by categories.
    categories = dict()
    for builder in builders:
        categories.setdefault(builder.category, []).append(builder)

    # Analyze each category separately for subcategories.
    result = []
    for category, contents in categories.items():
        left_builders = [b for b in contents if splicer.decision_func(b)]
        right_builders = [b for b in contents if not splicer.decision_func(b)]

        # Create subcategories only with enough builders of each kind.
        add_sub_cat = len(left_builders) > 1 and len(right_builders) > 1
        for builder in left_builders + right_builders:
            result.append(builder_with_category(builder, add_sub_cat, splicer))
    return result

def mirror_console(original_console, dev_console):
    dev_console.builders = splice_by_categories(
        original_console.builders,
        BUILDER_TESTER_SPLICER,
    )
    dev_console.builders = splice_by_categories(
        dev_console.builders,
        RELEASE_DEBUG_SPLICER,
    )

def mirror_dev_consoles(ctx):
    """
    Mirror the three main consoles as dev consoles with additional
    subcategories.
    """
    consoles = consoles_map(ctx)
    mirror_console(consoles["main"], consoles["main-dev"])
    mirror_console(consoles["memory"], consoles["memory-dev"])
    mirror_console(consoles["ports"], consoles["ports-dev"])

def ensure_forward_triggering_properties(ctx):
    """
    This callback collects (and removes) `parent_builder` properties from the
    builders, then reverse the relationship and set `triggers` property on the
    corresponding builders.
    """
    build_bucket = ctx.output["cr-buildbucket.cfg"]
    for bucket in build_bucket.buckets:
        triggers = dict()
        for builder in bucket.swarming.builders:
            if builder.properties:
                properties = json.decode(builder.properties)
                parent = properties.pop("parent_builder", None)
                if parent:
                    triggers.setdefault(parent, []).append(builder.name)
                builder.properties = json.encode(properties)
        for builder in bucket.swarming.builders:
            tlist = triggers.get(builder.name, [])
            if tlist:
                properties = json.decode(builder.properties)
                properties["triggers"] = tlist
                builder.properties = json.encode(properties)

def extract_property(builder, key, default):
    value = default
    if builder.properties:
        properties = json.decode(builder.properties)
        value = properties.pop(key, default)
        builder.properties = json.encode(properties)
    return value

def builder_suffix(builder):
    return builder.name.split("/")[-1]

def builder_suffixes(console):
    return [builder_suffix(builder) for builder in console.builders]

def hide_wip_builders(ctx):
    """
    Move builders marked as WIP to a separate console.
    """
    wip_console = consoles_map(ctx)["wip"]
    wip_builders = []
    build_bucket = ctx.output["cr-buildbucket.cfg"]
    for bucket in build_bucket.buckets:
        for builder in bucket.swarming.builders:
            wip = extract_property(builder, "__wip__", False)
            if wip:
                wip_builders.append(builder.name)

    milo = ctx.output["luci-milo.cfg"]
    for console in milo.consoles:
        if console == wip_console:
            continue
        filtered_builders = []
        for console_builder in console.builders:
            if builder_suffix(console_builder) in builder_suffixes(wip_console):
                continue
            if (builder_suffix(console_builder) in wip_builders and
                "/luci.v8.ci/" in console_builder.name):
                wip_console.builders.append(console_builder)
            else:
                filtered_builders.append(console_builder)
        console.builders = filtered_builders

def scheduled_builder_cleanup(ctx):
    """
    Remove repository information from builders that are scheduled. This is
    necessary for luci-notify to ignore notifications for builders with no
    revision information and cron scheduled builders will always have no
    revision information.
    """
    schedule = ctx.output["luci-scheduler.cfg"]
    scheduled_builders = [job.id for job in schedule.job if job.schedule]

    notify = ctx.output["luci-notify.cfg"]
    for notifier in notify.notifiers:
        for builder in notifier.builders:
            if builder.name in scheduled_builders:
                builder.repository = None

def collect_sherriffed_non_tree_closer_builders(ctx):
    """
    This callback checks that no sheriff emails are sent on non-tree closers.
    """
    notify = ctx.output["luci-notify.cfg"]

    tree_closers = [
        builder
        for notifier in notify.notifiers
        for builder in notifier.builders
        if notifier.tree_closers
    ]

    def sheriff_in_recipients(notifier):
        return any([
            "sheriff" in recipient
            for notification in notifier.notifications
            for recipient in notification.email.recipients
        ])

    notifies_sheriff = [
        builder
        for notifier in notify.notifiers
        for builder in notifier.builders
        if (not notifier.tree_closers) and sheriff_in_recipients(notifier)
    ]

    sherriffed_builders = sorted(
        [
            struct(name = builder.name, bucket = builder.bucket)
            for builder in notifies_sheriff
            if builder not in tree_closers
        ],
        key = lambda b: (b.name, b.bucket),
    )

    ctx.output["sherrifed-builders.cfg"] = json.indent(
        json.encode(struct(builders = sherriffed_builders)),
        indent = "  ",
    )

lucicfg.generator(collect_sherriffed_non_tree_closer_builders)

lucicfg.generator(aggregate_builder_tester_console)

lucicfg.generator(separate_builder_tester_console)

lucicfg.generator(headless_consoles)

lucicfg.generator(mirror_dev_consoles)

lucicfg.generator(ensure_forward_triggering_properties)

lucicfg.generator(hide_wip_builders)

lucicfg.generator(scheduled_builder_cleanup)
