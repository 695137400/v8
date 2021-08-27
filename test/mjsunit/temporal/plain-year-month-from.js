// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --harmony-temporal

let d1 = new Temporal.PlainYearMonth(2021, 8);
// 1. Set options to ? GetOptionsObject(options).
[true, false, "string is invalid", Symbol(),
    123, 456n, Infinity, NaN, null].forEach(function(invalidOptions) {

  assertThrows(() => Temporal.PlainYearMonth.from(
      d1, invalidOptions),
      TypeError,
      "invalid_argument");
    });

// a. Perform ? ToTemporalOverflow(options).
assertThrows(() => Temporal.PlainYearMonth.from(
  d1, {overflow: "invalid overflow"}),
    RangeError,
    "Value invalid overflow out of range for Temporal.PlainYearMonth.from options property overflow");

[undefined, {}, {overflow: "constrain"}, {overflow: "reject"}].forEach(
    function(validOptions) {
  let d = new Temporal.PlainYearMonth(1, 2);
  let d2 = Temporal.PlainYearMonth.from(d, validOptions);
  assertEquals(1, d2.year);
  assertEquals(2, d2.month);
  assertEquals("iso8601", d2.calendar.id);
});

[undefined, {}, {overflow: "constrain"}, {overflow: "reject"}].forEach(
    function(validOptions) {
  let d3 = Temporal.PlainYearMonth.from(
      {year:9, month: 8},
      validOptions);
  assertEquals(9, d3.year);
  assertEquals(8, d3.month);
  assertEquals("M08", d3.monthCode);
  assertEquals("iso8601", d3.calendar.id);
});

[undefined, {}, {overflow: "constrain"}].forEach(
    function(validOptions) {
  let d4 = Temporal.PlainYearMonth.from(
      {year:9, month: 14},
      validOptions);
  assertEquals(9, d4.year);
  assertEquals(12, d4.month);
  assertEquals("M12", d4.monthCode);
  assertEquals("iso8601", d4.calendar.id);
});

assertThrows(() => Temporal.PlainYearMonth.from(
      {year:9, month: 14},
    {overflow: "reject"}),
    RangeError,
    "Invalid time value");
