// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function inherits(childCtor, parentCtor) {
  childCtor.prototype.__proto__ = parentCtor.prototype;
};

/**
 * A thin wrapper around shell's 'read' function showing a file name on error.
 */
function readFile(fileName) {
  try {
    return read(fileName);
  } catch (e) {
    console.log(fileName + ': ' + (e.message || e));
    throw e;
  }
}

// ===========================================================================

numberFormat = new Intl.NumberFormat('de-CH', {
  maximumFractionDigits: 2,
  minimumFractionDigits: 2,
});

function BYTES(bytes, total) {
  let units = ['B ', 'KB', 'MB', 'GB'];
  let unitIndex = 0;
  let value = bytes;
  while (value > 1024 && unitIndex < units.length) {
    value /= 1024;
    unitIndex++;
  }
  let result = numberFormat.format(value).padStart(10) + ' ' + units[unitIndex];
  if (total !== undefined && total != 0) {
    result += PERCENT(bytes, total).padStart(5);
  }
  return result;
}

function PERCENT(value, total) {
  return Math.round(value / total * 100) + "%"
}

function timestampMin(list) {
  let result = -1;
  list.forEach(timestamp => {
    if (result == -1) {
      result = timestamp;
    } else if (timestamp != -1) {
      result = Math.min(result, timestamp);
    }
  });
  return result;
}

// ===========================================================================
class Script {
  constructor(file, id) {
    this.file = file;
    this.id = id;
    if (id === undefined || id <= 0) {
      throw new Error(`Invalid id=${id} for script with file='${file}'`);
    }
    this.isEval = false;
    this.funktions = [];
    this.metrics = new Map();
    this.maxNestingLevel = 0;
    this.firstParseEvent = -1;
    this.lastParseEvent = -1;
    this.width = 0;
    this.bytesTotal = 0;
    this.finalized = false;
    this.summary = '';
  }

  isEmpty() {
    return this.funktions.length == 0
  }

  funktionAtPosition(start) {
    if (this.finalized) throw new Error('script is finalized!');
    return this.funktions[start];
  }

  addMissingFunktions(list) {
    if (this.finalized) throw new Error('script is finalized!');
    list.forEach(fn => {
      if (this.funktions[fn.start] === undefined) {
        this.addFunktion(fn);
      }
    });
  }

  addFunktion(fn) {
    if (this.finalized) throw new Error('script is finalized!');
    if (fn.start == undefined) {
      throw new Error("Funktion has no start position");
    }
    if (this.funktions[fn.start] !== undefined) {
      fn.print();
      throw new Error("adding same function twice to script");
    }
    this.funktions[fn.start] = fn;
  }

  finalize() {
    this.finalized = true;
    // Compact funktions as we no longer need access via start byte position.
    this.funktions = this.funktions.filter(each => true);
    let parent;
    let maxNesting = 0;
    // Iterate over the Funktions in byte position order.
    this.funktions.forEach(fn => {
      fn.fromEval = this.isEval;
      if (parent === undefined) {
        parent = fn;
      } else {
        // Walk up the nested chain of Funktions to find the parent.
        while (parent !== undefined && !fn.isNestedIn(parent)) {
          parent = parent.parent;
        }
        fn.parent = parent;
        if (parent) {
          maxNesting = Math.max(maxNesting, parent.addNestedFunktion(fn));
        }
        parent = fn;

        if (this.firstParseEvent == -1) {
          this.firstParseEvent = fn.getFirstParseEvent();
        } else {
          this.firstParseEvent =
            Math.min(this.firstParseEvent, fn.getFirstParseEvent());
        }

        this.lastParseEvent =
          Math.max(this.lastParseEvent, fn.getLastParseEvent());
      }
    });
    this.maxNestingLevel = maxNesting;
  }

  print() {
    console.log(this.toString());
  }

  toString() {
    let str = `SCRIPT id=${this.id} file=${this.file}\n` +
      `functions[${this.funktions.length}]:`;
    this.funktions.forEach(fn => str += fn.toString());
    return str;
  }

  calculateMetrics(printSummary) {
    let log = (str) => this.summary += str + '\n';
    log("SCRIPT: " + this.id);
    let all = this.funktions;
    if (all.length == 0) return;

    let nofFunktions = all.length;
    let ownBytesSum = list => {
      return list.reduce((bytes, each) => bytes + each.getOwnBytes(), 0)
    };
    this.bytesTotal = all.reduce(
      (bytes, each) => bytes + (each.isToplevel() ? each.getBytes() : 0), 0);

    let info = (name, funktions) => {
      let ownBytes = ownBytesSum(funktions);
      let nofPercent = Math.round(funktions.length / nofFunktions * 100);
      let value = (funktions.length + "").padStart(6) +
        (nofPercent + "%").padStart(5) +
        BYTES(ownBytes, this.bytesTotal).padStart(10);
      log(("  - " + name).padEnd(20) + value);
      this.metrics.set(name + "-bytes", ownBytes);
      this.metrics.set(name + "-count", funktions.length);
      this.metrics.set(name + "-count-percent", nofPercent);
      this.metrics.set(name + "-bytes-percent",
        Math.round(ownBytes / this.bytesTotal * 100));
    };

    log("  - file:         " + this.file);
    info("functions", all);
    info("toplevel", all.filter(each => each.isToplevel()));
    info("preparsed", all.filter(each => each.preparseTime > 0));


    info("fully parsed", all.filter(each => each.parseTime > 0));
    info("fn parsed", all.filter(each => each.parse2Time > 0));
    info("resolved", all.filter(each => each.resolutionTime > 0));
    info("executed", all.filter(each => each.executionTimestamp > 0));
    info("forEval", all.filter(each => each.fromEval));

    let parsingCost = new ExecutionCost('parse', all,
      each => each.parseTime);
    parsingCost.setMetrics(this.metrics);
    log(parsingCost.toString())

    let preParsingCost = new ExecutionCost('preparse', all,
      each => each.preparseTime);
    preParsingCost.setMetrics(this.metrics);
    log(preParsingCost.toString())

    let resolutionCost = new ExecutionCost('resolution', all,
      each => each.resolutionTime);
    resolutionCost.setMetrics(this.metrics);
    log(resolutionCost.toString())

    let nesting = new NestingDistribution(all);
    nesting.setMetrics(this.metrics);
    log(nesting.toString())

    if (printSummary) console.log(this.summary);
  }

  getAccumulatedMetric(name, cutoff) {
    let property = name + 'Timestamp';
    return this.funktions.reduce((sum, funktion) => {
      let value = funktion[property];
      return -1 < value && value < cutoff ?
        sum + funktion.getOwnBytes() : sum;
    }, 0);
  }
}


class TotalScript extends Script {
  constructor() {
    super('all files', 'all files');
  }

  addAllFunktions(script) {
    // funktions is indexed by byte offset and as such not packed. Add every
    // Funktion one by one to keep this.funktions packed.
    script.funktions.forEach(fn => this.funktions.push(fn));
  }
}


// ===========================================================================

class NestingDistribution {
  constructor(funktions) {
    // Stores the nof bytes per function nesting level.
    this.accumulator = [0, 0, 0, 0, 0];
    // Max nof bytes encountered at any nesting level.
    this.max = 0;
    // avg bytes per nesting level.
    this.avg = 0;
    this.totalBytes = 0;

    funktions.forEach(each => each.accumulateNestingLevel(this.accumulator));
    this.max = this.accumulator.reduce((max, each) => Math.max(max, each), 0);
    this.totalBytes = this.accumulator.reduce((sum, each) => sum + each, 0);
    for (let i = 0; i < this.accumulator.length; i++) {
      this.avg += this.accumulator[i] * i;
    }
    this.avg /= this.totalBytes;
  }

  print() {
    console.log(this.toString())
  }

  toString() {
    let ticks = " ▁▂▃▄▅▆▇█";
    let accString = this.accumulator.reduce((str, each) => {
      let index = Math.round(each / this.max * (ticks.length - 1));
      return str + ticks[index];
    }, '');
    let percent0 = this.accumulator[0]
    let percent1 = this.accumulator[1];
    let percent2plus = this.accumulator.slice(2)
      .reduce((sum, each) => sum + each, 0);
    return "  - nesting level:      " +
      ' avg=' + numberFormat.format(this.avg) +
      ' l0=' + PERCENT(percent0, this.totalBytes) +
      ' l1=' + PERCENT(percent1, this.totalBytes) +
      ' l2+=' + PERCENT(percent2plus, this.totalBytes) +
      ' distribution=[' + accString + ']';

  }

  setMetrics(dict) {}
}

class ExecutionCost {
  constructor(prefix, funktions, time_fn) {
    this.prefix = prefix;
    // Time spent on executed functions.
    this.executedCost = 0
    // Time spent on not executed functions.
    this.nonExecutedCost = 0;

    this.executedCost = funktions.reduce((sum, each) => {
      return sum + (each.hasBeenExecuted() ? time_fn(each) : 0)
    }, 0);
    this.nonExecutedCost = funktions.reduce((sum, each) => {
      return sum + (each.hasBeenExecuted() ? 0 : time_fn(each))
    }, 0);

  }

  print() {
    console.log(this.toString())
  }

  toString() {
    return ('  - ' + this.prefix + '-time:').padEnd(24) +
      (" executed=" + numberFormat.format(this.executedCost) + 'ms').padEnd(20) +
      " non-executed=" + numberFormat.format(this.nonExecutedCost) + 'ms';
  }

  setMetrics(dict) {
    dict.set('parseMetric', this.executionCost);
    dict.set('parseMetricNegative', this.nonExecutionCost);
  }
}

// ===========================================================================
class Funktion {
  constructor(name, start, end, script) {
    if (start < 0) throw new Error("invalid start position: " + start);
    if (end <= 0) throw new Error("invalid end position: " + end);
    this.name = name;
    this.start = start;
    this.end = end;
    this.script = script;
    this.parent = undefined;
    this.fromEval = false;
    this.nested = [];
    this.nestingLevel = 0;

    this.preparseTimestamp = -1;
    this.parseTimestamp = -1;
    this.parse2Timestamp = -1;
    this.resolutionTimestamp = -1;
    this.executionTimestamp = -1;
    this.firstParseTimestamp = -1;

    this.preparseTime = 0;
    this.parseTime = 0;
    this.parse2Time;
    this.resolutionTime = 0;

    this.scopeResolutionTime = 0;
    if (script) this.script.addFunktion(this);
  }

  getFirstParseEvent() {
    if (this.firstParseTimestamp == -1) {
      this.firstParseTimestamp = timestampMin(
        [this.parseTimestamp, this.preparseTimestamp,
          this.resolutionTimestamp
        ])
    }
    return this.firstParseTimestamp;
  }

  getLastParseEvent() {
    return Math.max(
      this.preparseTimestamp + this.preparseTime,
      this.parseTimestamp + this.parseTime,
      this.resolutionTimestamp + this.resolutionTime);
  }

  isNestedIn(funktion) {
    if (this.script != funktion.script) throw new Error("Incompatible script");
    return funktion.start < this.start && this.end < funktion.end;
  }

  isToplevel() {
    return this.parent === undefined
  }

  hasBeenExecuted() {
    return this.executionTimestamp > 0
  }

  accumulateNestingLevel(accumulator) {
    let value = accumulator[this.nestingLevel] || 0;
    accumulator[this.nestingLevel] = value + this.getOwnBytes();
  }

  addNestedFunktion(funktion) {
    if (this.script != funktion.script) throw new Error("Incompatible script");
    if (funktion == undefined) throw new Error("Nesting non funktion");
    this.nested.push(funktion);
    funktion.nestingLevel = this.nestingLevel + 1;
    return funktion.nestingLevel;
  }

  getBytes() {
    return this.end - this.start;
  }

  getOwnBytes() {
    return this.nested.reduce(
      (bytes, each) => bytes - each.getBytes(),
      this.getBytes());
  }

  print() {
    console.log(this.toString());
  }

  toString() {
    return `function ${this.name}() range=${this.start}-${this.end}`,
      `script=${this.script ? this.script.id : 'X'}`;
  }
}




// ===========================================================================
function functionEventProcessor(processor) {
  // {script file},{script id},{start position},{end position},
  // {time},{timestamp},{function name}
  return {
    parsers: [null, parseInt, parseInt, parseInt, parseFloat, parseInt, null],
    processor: processor
  };
}

const kTimestampfactor = 1000;

function ParseProcessor() {
  LogReader.call(this, {
    'parse-full': functionEventProcessor(this.processFull),
    'parse-function': functionEventProcessor(this.processFunction),
    'parse-script': functionEventProcessor(this.processScript),
    'parse-eval': functionEventProcessor(this.processEval),
    'preparse-no-resolution': functionEventProcessor(this.processPreparseNoResolution),
    'preparse-resolution': functionEventProcessor(this.processPreparseResolution),
    'first-execution': functionEventProcessor(this.processFirstExecution)
  });
  this.idToScript = new Map();
  this.fileToScript = new Map();
  this.nameToFunction = new Map();
  this.scripts = [];
  this.totalScript = new TotalScript();
  this.firstParseEvent = -1;
  this.lastParseEvent = -1;
}
inherits(ParseProcessor, LogReader);

ParseProcessor.prototype.print = function() {
  console.log("scripts:");
  this.idToScript.forEach(script => script.print());
}
/**
 * @override
 */
ParseProcessor.prototype.printError = function(str) {
  console.log(str);
};

ParseProcessor.prototype.processString = function(string) {
  var end = string.length;
  var current = 0;
  var next = 0;
  var line;
  var i = 0;
  var entry;
  while (current < end) {
    next = string.indexOf("\n", current);
    if (next === -1) break;
    i++;
    line = string.substring(current, next);
    current = next + 1;
    this.processLogLine(line);
  }
  this.postProcess();
}

ParseProcessor.prototype.processLogFile = function(fileName) {
  this.collectEntries = true
  this.lastLogFileName_ = fileName;
  var line;
  while (line = readline()) {
    this.processLogLine(line);
  }
  this.postProcess();
}

ParseProcessor.prototype.postProcess = function() {
  this.scripts = [...this.idToScript.values()];
  this.scripts.forEach(script => script.finalize());
  this.scripts.forEach(script => script.calculateMetrics(false));

  this.firstParseEvent =
    timestampMin(this.scripts.map(each => each.firstParseEvent));
  this.lastParseEvent = this.scripts.reduce(
    (max, script) => Math.max(max, script.lastParseEvent), -1);

  this.idToScript.forEach(script => this.totalScript.addAllFunktions(script));
  this.totalScript.calculateMetrics(true);
};

ParseProcessor.prototype.addEntry = function(entry) {
  this.entries.push(entry);
}

ParseProcessor.prototype.lookupScript = function(file, id) {
  // During preparsing we only have the temporary ranges and no script yet.
  let script;
  if (this.idToScript.has(id)) {
    script = this.idToScript.get(id);
  } else {
    script = new Script(file, id);
    this.idToScript.set(id, script);
  }
  if (file.length > 0 && script.file.length == 0) {
    script.file = file;
    this.fileToScript.set(file, script);
  }
  return script;
}

ParseProcessor.prototype.addScriptAndFunction = function(file, scriptId,
  startPosition, endPosition, time, timestamp, functionName) {
  let script = this.lookupScript(file, scriptId);
  let funktion = script.funktionAtPosition(startPosition);
  if (funktion == undefined) {
    funktion = new Funktion(functionName, startPosition, endPosition, script);
  }
  return funktion;
}

ParseProcessor.prototype.processEval = function(file, scriptId, startPosition,
  endPosition, time, timestamp, functionName) {
  let script = this.lookupScript(file, scriptId);
  script.isEval = true;
}

ParseProcessor.prototype.processFull = function(file, scriptId, startPosition,
  endPosition, time, timestamp, functionName) {
  let funktion = this.addScriptAndFunction(...arguments);
  funktion.parseTimestamp = timestamp / kTimestampfactor - time;
  funktion.parseTime = time;
}

ParseProcessor.prototype.processFunction = function(file, scriptId, startPosition,
  endPosition, time, timestamp, functionName) {
  let funktion = this.addScriptAndFunction(...arguments);
  funktion.parse2Timestamp = timestamp / kTimestampfactor - time;
  funktion.parse2Time = time;
}

ParseProcessor.prototype.processScript = function(file, scriptId, startPosition,
  endPosition, time, timestamp, functionName) {
  // TODO timestamp and time
  let script = this.lookupScript(file, scriptId);
}

ParseProcessor.prototype.processPreparseResolution = function(file, scriptId,
  startPosition, endPosition, time, timestamp, functionName) {
  let funktion = this.addScriptAndFunction(...arguments);
  funktion.resolutionTimestamp = timestamp / kTimestampfactor - time;
  funktion.resolutionTime = time;
}

ParseProcessor.prototype.processPreparseNoResolution = function(file, scriptId,
  startPosition, endPosition, time, timestamp, functionName) {
  let funktion = this.addScriptAndFunction(...arguments);
  funktion.preparseTimestamp = timestamp / kTimestampfactor - time;
  funktion.preparseTime = time;
}

ParseProcessor.prototype.processFirstExecution = function(file, scriptId,
  startPosition, endPosition, time, timestamp, functionName) {
  let script = this.lookupScript(file, scriptId);
  if (startPosition == 0) {
    // undefined = eval fn execution
    if (script) {
      script.executionTimestamp = timestamp / kTimestampfactor;
    }
  } else {
    let funktion = script.funktionAtPosition(startPosition);
    if (funktion) {
      funktion.executionTimestamp = timestamp / kTimestampfactor;
    } else if (functionName.length > 0) {
      // throw new Error("Could not find function: " + functionName);
    }
  }
}


function ArgumentsProcessor(args) {
  this.args_ = args;
  this.result_ = ArgumentsProcessor.DEFAULTS;

  this.argsDispatch_ = {};
};


ArgumentsProcessor.DEFAULTS = {
  logFileName: 'v8.log',
  range: 'auto,auto',
};


ArgumentsProcessor.prototype.parse = function() {
  while (this.args_.length) {
    var arg = this.args_.shift();
    if (arg.charAt(0) != '-') {
      this.result_.logFileName = arg;
      continue;
    }
    var userValue = null;
    var eqPos = arg.indexOf('=');
    if (eqPos != -1) {
      userValue = arg.substr(eqPos + 1);
      arg = arg.substr(0, eqPos);
    }
    if (arg in this.argsDispatch_) {
      var dispatch = this.argsDispatch_[arg];
      this.result_[dispatch[0]] = userValue == null ? dispatch[1] : userValue;
    } else {
      return false;
    }
  }
  return true;
};


ArgumentsProcessor.prototype.result = function() {
  return this.result_;
};


ArgumentsProcessor.prototype.printUsageAndExit = function() {

  function padRight(s, len) {
    s = s.toString();
    if (s.length < len) {
      s = s + (new Array(len - s.length + 1).join(' '));
    }
    return s;
  }

  console.log('Cmdline args: [options] [log-file-name]\n' +
    'Default log file name is "' +
    ArgumentsProcessor.DEFAULTS.logFileName + '".\n');
  console.log('Options:');
  for (var arg in this.argsDispatch_) {
    var synonyms = [arg];
    var dispatch = this.argsDispatch_[arg];
    for (var synArg in this.argsDispatch_) {
      if (arg !== synArg && dispatch === this.argsDispatch_[synArg]) {
        synonyms.push(synArg);
        delete this.argsDispatch_[synArg];
      }
    }
    console.log('  ' + padRight(synonyms.join(', '), 20) + " " + dispatch[2]);
  }
  quit(2);
};
