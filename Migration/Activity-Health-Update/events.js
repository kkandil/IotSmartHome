'use strict';
const { randomUUID } = require('node:crypto');
const { validateClock, clockMatches, localTime } = require('./event-clock');

const key = t => JSON.stringify([t.homeName, t.deviceID, t.varName]);
function home(input) {
  if (!input || typeof input.homeName !== 'string' || !/^[A-Za-z0-9_-]+$/.test(input.homeName)) throw new Error('Select a home');
}
function equal(actual, expected, type) {
  if (actual === undefined || actual === null) return false;
  if (['int', 'float'].includes(type)) return String(actual).trim() !== '' && Number.isFinite(Number(actual)) && Number(actual) === Number(expected);
  if (['bool', 'boolean'].includes(type)) {
    const normalize = v => ['1', 'true'].includes(String(v)) ? true : ['0', 'false'].includes(String(v)) ? false : null;
    return normalize(actual) !== null && normalize(actual) === normalize(expected);
  }
  return String(actual) === expected;
}
function compare(actual, expected, type, operator = '=') {
  if (operator === '=') return equal(actual, expected, type);
  if (!['int', 'float'].includes(type) || actual == null || String(actual).trim() === '' || !Number.isFinite(Number(actual))) return false;
  const a = Number(actual), b = Number(expected);
  return operator === '>' ? a > b : operator === '>=' ? a >= b : operator === '<' ? a < b : operator === '<=' ? a <= b : false;
}
function validateHold(item) {
  const seconds = item.holdSeconds === undefined ? 0 : item.holdSeconds;
  if (!Number.isSafeInteger(seconds) || seconds < 0 || seconds > 359999)
    throw Error('Trigger duration must be between 00:00:01 and 99:59:59, or Immediately');
  return seconds;
}
function rawMatch(c, rule, values, now) {
  return c.kind === 'time' ? clockMatches(c, now)
    : compare(values.get(key({ ...c, homeName: rule.homeName })), c.varValue, c.varType, c.operator);
}
function evaluateConditions(rule, values, now, claimed = new Set(), ready = null) {
  const groups = [[]];
  rule.conditions.forEach((c, index) => {
    if (index && c.join === 'OR') groups.push([]);
    groups[groups.length - 1].push({ c, index });
  });
  let matched = false;
  const slots = [];
  for (const group of groups) {
    const exact = [];
    const ok = group.every(({ c, index }) => {
      if (ready && !ready[index]) return false;
      if (c.kind !== 'time') return rawMatch(c, rule, values, now);
      if (!clockMatches(c, now)) return false;
      if (c.mode === 'exact') {
        const slot = `${index}:${c.timeZone}:${localTime(now, c.timeZone).date}T${c.time}`;
        if (claimed.has(slot)) return false;
        exact.push(slot);
      }
      return true;
    });
    if (ok) { matched = true; slots.push(...exact); }
  }
  return { matched, slots };
}
function matches(rule, values, now = new Date()) {
  return evaluateConditions(rule, values, now).matched;
}

class EventEngine {
  constructor(sql, { resolveTarget, dispatch, catalog, activity = null, location = () => null, onChange = () => {}, clock = () => new Date(), available = () => true }) {
    Object.assign(this, { sql, resolveTarget, dispatch, catalog, activity, location, onChange, clock, available });
    this.holds = new Map();
    this.values = new Map();
    this.queue = Promise.resolve();
    sql.exec(`CREATE TABLE IF NOT EXISTS conditional_events (
      id TEXT PRIMARY KEY, home TEXT NOT NULL, body TEXT NOT NULL,
      matched INTEGER NOT NULL DEFAULT 0, last_run TEXT);
      CREATE INDEX IF NOT EXISTS event_home ON conditional_events(home);
      CREATE TABLE IF NOT EXISTS event_clock_claims (
        event_id TEXT NOT NULL, slot TEXT NOT NULL, at INTEGER NOT NULL,
        PRIMARY KEY(event_id, slot));`);
  }
  serial(work) {
    const result = this.queue.then(work);
    this.queue = result.catch(error => console.error('Events:', error.message));
    return result;
  }
  rows(homeName) {
    return homeName === undefined ? this.sql.prepare('SELECT * FROM conditional_events ORDER BY rowid').all()
      : this.sql.prepare('SELECT * FROM conditional_events WHERE home=? ORDER BY rowid').all(homeName);
  }
  list(input) {
    home(input);
    return this.rows(input.homeName).map(row => ({ ...JSON.parse(row.body), lastRun: row.last_run ? JSON.parse(row.last_run) : null }));
  }
  async seed(rule) {
    for (const c of rule.conditions) {
      if (c.kind === 'time') continue;
      const target = { ...c, homeName: rule.homeName };
      const variable = await this.resolveTarget(target);
      this.values.set(key(target), variable?.Value);
    }
  }
  baseline(rule) {
    const now = this.clock();
    this.holds.delete(rule.id);
    const ready = this.conditionReadiness(rule, now);
    // Do not execute a current minute when saving, enabling or restarting.
    rule.conditions.forEach((c, i) => {
      if (c.kind === 'time' && c.mode === 'exact' && clockMatches(c, now)) {
        this.claim(rule.id, [`${i}:${c.timeZone}:${localTime(now, c.timeZone).date}T${c.time}`], now);
      }
    });
    return +evaluateConditions(rule, this.values, now, new Set(), ready).matched;
  }
  claim(id, slots, now) {
    for (const slot of slots) this.sql.prepare('INSERT OR IGNORE INTO event_clock_claims VALUES(?,?,?)').run(id, slot, now.getTime());
  }
  async start() {
    for (const row of this.rows()) {
      const rule = JSON.parse(row.body);
      await this.seed(rule);
      // Restarting never replays actions for an already matching condition.
      this.sql.prepare('UPDATE conditional_events SET matched=? WHERE id=?').run(this.baseline(rule), row.id);
    }
  }
  startClock() {
    if (this.timer) return;
    this.timer = setInterval(() => {
      if (this.tickPending) return;
      this.tickPending = true;
      this.tick().catch(error => console.error('Event clock:', error.message))
        .finally(() => { this.tickPending = false; });
    }, 1000);
    this.timer.unref();
  }
  stop() { clearInterval(this.timer); this.timer = null; }
  async validateItem(item, homeName, condition, index) {
    const holdSeconds = condition ? validateHold(item || {}) : 0;
    if (condition && item?.kind === 'time') return { ...validateClock(item, index), holdSeconds };
    if (!item || !Number.isSafeInteger(item.deviceID) || item.deviceID <= 0
        || typeof item.varName !== 'string' || !item.varName || item.varName.length > 128) throw new Error('Choose a device and variable');
    const target = await this.resolveTarget({ ...item, homeName });
    if (!target) throw new Error('A selected device or variable no longer exists');
    if (!condition && target.readOnly) throw Error('This measurement is read-only');
    const type = target.Type;
    if (!['int', 'float', 'string', 'bool', 'boolean'].includes(type)) throw new Error('Unsupported variable type');
    const value = item.varValue;
    if (typeof value !== 'string' || value.length > 1024) throw new Error('Enter a value of at most 1024 characters');
    if (type === 'int' && !/^[+-]?\d+$/.test(value)) throw new Error('Enter a whole number');
    if (['int', 'float'].includes(type) && (!value.trim() || !Number.isFinite(Number(value)))) throw new Error('Enter a valid number');
    if (['bool', 'boolean'].includes(type) && !['0', '1', 'true', 'false'].includes(value)) throw new Error('Select ON or OFF');
    if (condition && index > 0 && !['AND', 'OR'].includes(item.join)) throw new Error('Choose AND or OR');
    const operator = item.operator || '=';
    if (condition && (!['=', '>', '>=', '<', '<='].includes(operator) || (operator !== '=' && !['int','float'].includes(type)))) throw new Error('Ordering comparisons require a numeric variable');
    return { deviceID: item.deviceID, deviceName: target.deviceName || String(item.deviceID), varName: item.varName,
      varType: type, varValue: value, ...(condition ? { join: index ? item.join : 'AND', operator, holdSeconds } : {}) };
  }
  checkCycles(rule) {
    if (!rule.enabled) return;
    const rules = this.list(rule).filter(r => r.enabled && r.id !== rule.id).concat(rule);
    const graph = new Map();
    for (const r of rules) for (const c of r.conditions) {
      if (c.kind === 'time') continue;
      const source = key({ ...c, homeName: r.homeName });
      if (!graph.has(source)) graph.set(source, new Set());
      for (const a of r.actions) graph.get(source).add(key({ ...a, homeName: r.homeName }));
    }
    const visiting = new Set(), visited = new Set();
    const visit = node => {
      if (visiting.has(node)) throw new Error('These enabled events form a loop. Change an action or disable a conflicting event.');
      if (visited.has(node)) return;
      visiting.add(node);
      for (const next of graph.get(node) || []) visit(next);
      visiting.delete(node); visited.add(node);
    };
    for (const node of graph.keys()) visit(node);
  }
  save(input) { return this.serial(async () => {
    home(input);
    if (typeof input.name !== 'string' || !input.name.trim() || input.name.trim().length > 80) throw new Error('Enter an event name (up to 80 characters)');
    if (typeof input.enabled !== 'boolean') throw new Error('Choose whether the event is enabled');
    for (const field of ['conditions', 'actions']) if (!Array.isArray(input[field]) || !input[field].length || input[field].length > 32)
      throw new Error('Add at least one condition and one action (maximum 32 each)');
    if (input.id && (typeof input.id !== 'string' || !this.sql.prepare('SELECT id FROM conditional_events WHERE id=? AND home=?').get(input.id, input.homeName))) throw new Error('Event not found');
    const rule = { id: input.id || randomUUID(), homeName: input.homeName, name: input.name.trim(), enabled: input.enabled, conditions: [], actions: [] };
    for (let i = 0; i < input.conditions.length; i++) rule.conditions.push(await this.validateItem(input.conditions[i], rule.homeName, true, i));
    for (const a of input.actions) rule.actions.push(await this.validateItem(a, rule.homeName, false, 0));
    this.checkCycles(rule);
    await this.seed(rule);
    this.sql.prepare(`INSERT INTO conditional_events(id,home,body,matched) VALUES(?,?,?,?)
      ON CONFLICT(id) DO UPDATE SET body=excluded.body, matched=excluded.matched`)
      .run(rule.id, rule.homeName, JSON.stringify(rule), this.baseline(rule));
    this.onChange(rule);
    return rule;
  }); }
  toggle(input) { return this.serial(async () => {
    home(input);
    if (typeof input.enabled !== 'boolean' || typeof input.id !== 'string') throw new Error('Invalid event');
    const rule = this.list(input).find(r => r.id === input.id);
    if (!rule) throw new Error('Event not found');
    delete rule.lastRun;
    rule.enabled = input.enabled;
    this.checkCycles(rule);
    await this.seed(rule);
    this.sql.prepare('UPDATE conditional_events SET body=?,matched=? WHERE id=?').run(JSON.stringify(rule), this.baseline(rule), rule.id);
    this.onChange(rule);
    return rule;
  }); }
  remove(input) { return this.serial(() => {
    home(input);
    if (typeof input.id !== 'string' || !this.sql.prepare('DELETE FROM conditional_events WHERE id=? AND home=?').run(input.id, input.homeName).changes) throw new Error('Event not found');
    this.holds.delete(input.id);
    this.onChange(input);
  }); }
  removeTarget(homeName, deviceID, varName) { return this.serial(() => {
    for (const row of this.rows(homeName)) {
      const rule = JSON.parse(row.body);
      if (deviceID === undefined || [...rule.conditions, ...rule.actions].some(t => t.deviceID === deviceID && (varName === undefined || t.varName === varName))) {
        this.sql.prepare('DELETE FROM conditional_events WHERE id=?').run(row.id);
        this.holds.delete(rule.id);
        this.onChange(rule);
      }
    }
  }); }
  conditionReadiness(rule, now, mutate = true, values = this.values) {
    const stored = this.holds.get(rule.id) || new Map();
    const timers = mutate ? stored : new Map(stored);
    if (mutate) this.holds.set(rule.id, timers);
    // Evaluate every condition even when an earlier AND/OR branch fails.
    return rule.conditions.map((c, i) => {
      const raw = rawMatch(c, rule, values, now);
      if (!c.holdSeconds) return raw;
      const available = c.kind === 'time' || this.available({ ...c, homeName: rule.homeName });
      if (!rule.enabled || !raw || !available) {
        timers.delete(i);
        if (mutate && !available && c.kind !== 'time') values.delete(key({ ...c, homeName: rule.homeName }));
        return false;
      }
      const at = now.getTime();
      if (!timers.has(i) || at < timers.get(i)) timers.set(i, at);
      return at - timers.get(i) >= c.holdSeconds * 1000;
    });
  }
  disconnected(homeName, deviceID) { return this.serial(() => {
    for (const row of this.rows(homeName)) {
      const rule = JSON.parse(row.body);
      rule.conditions.forEach((c, i) => {
        if (c.kind !== 'time' && c.deviceID === deviceID && c.holdSeconds) {
          this.holds.get(rule.id)?.delete(i);
          this.values.delete(key({ ...c, homeName }));
        }
      });
    }
    return this.evaluate();
  }); }
  changed(target) { return this.serial(async () => {
    const previous=this.values.get(key(target));
    this.values.set(key(target), target.varValue);
    await this.evaluate(target,previous);
  }); }
  tick() { return this.serial(() => this.evaluate()); }
  async evaluate(target,previous) {
    const now = this.clock();
    if (!this.nextClaimCleanup || now.getTime() >= this.nextClaimCleanup) {
      this.sql.prepare('DELETE FROM event_clock_claims WHERE at<?').run(now.getTime() - 8 * 86400000);
      this.nextClaimCleanup = now.getTime() + 86400000;
    }
    for (const row of this.rows(target?.homeName)) {
      const rule = JSON.parse(row.body);
      if (!rule.enabled || (process.env.HUB_HOME && rule.homeName !== process.env.HUB_HOME)) continue;
      if (!rule.conditions.some(c => target ? c.deviceID === target.deviceID && c.varName === target.varName : c.kind === 'time' || c.holdSeconds > 0)) continue;
      const claimed = new Set(this.sql.prepare('SELECT slot FROM event_clock_claims WHERE event_id=?').all(rule.id).map(r => r.slot));
      const { matched, slots } = evaluateConditions(rule, this.values, now, claimed, this.conditionReadiness(rule, now));
      // Claim the transition durably before sending commands (no repeat on echo).
      if (+matched !== row.matched) this.sql.prepare('UPDATE conditional_events SET matched=? WHERE id=?').run(+matched, row.id);
      if (!matched) {
        const crossed=target && rule.conditions.some(c=>c.kind!=='time'&&c.deviceID===target.deviceID&&c.varName===target.varName&&!compare(previous,c.varValue,c.varType,c.operator)&&compare(target.varValue,c.varValue,c.varType,c.operator));
        if(crossed&&this.activity)this.activity.put(rule.homeName,'automation',{name:rule.name,eventId:rule.id,status:'skipped',conditions:this.conditionSnapshot(rule,now),actions:[],trigger:{deviceID:target.deviceID,varName:target.varName,value:target.varValue}});
        continue;
      }
      if(row.matched && !slots.length)continue;
      this.claim(rule.id, slots, now);
      const at = this.clock().toISOString();
      const results = [];
      const conditions=this.conditionSnapshot(rule,now);
      const run=this.activity?.begin(rule,conditions,target?{deviceID:target.deviceID,varName:target.varName,value:target.varValue}:{kind:'clock'});
      // Every false-to-true transition is significant, even during a rapid toggle.
      // Duplicate reports are suppressed by matched; configured cycles are
      // rejected on save. A time-based cooldown would silently consume edges.
      this.sql.prepare('UPDATE conditional_events SET last_run=? WHERE id=?').run(JSON.stringify({ at, status: 'running', results: [] }), row.id);
      for (const action of rule.actions) {
        let status;
        const commandId=run?this.activity.startAction(run,action):undefined;
        try { status = await this.dispatch({ ...action, homeName: rule.homeName, ...(commandId?{commandId}:{}) }); }
        // A transport/confirmation error can occur after the device applied the write.
        catch (error) { status = 'outcome_unknown'; console.error('Event action:', error.message); }
        if(run)this.activity.finishAction(run,commandId,status);
        results.push({ deviceID: action.deviceID, varName: action.varName, status });
      }
      const status = results.every(r => r.status === 'confirmed') ? 'confirmed'
        : results.every(r => ['sent','confirmed'].includes(r.status)) ? 'sent'
        : results.every(r => r.status === 'saved_offline') ? 'saved_offline'
        : results.some(r => ['sent','confirmed','saved_offline'].includes(r.status)) ? 'partial'
        : results.some(r => r.status === 'outcome_unknown') ? 'outcome_unknown' : 'not_sent';
      this.sql.prepare('UPDATE conditional_events SET last_run=? WHERE id=?').run(JSON.stringify({ at, status, results }), row.id);
      console.log(`Event ${rule.id}: ${status}`);
      this.onChange(rule);
    }
  }
  conditionSnapshot(rule,now,values=this.values) {
    const ready = this.conditionReadiness(rule, now, false, values);
    return rule.conditions.map((c,i)=>({...c,rawMatched:rawMatch(c,rule,values,now),remainingSeconds:c.holdSeconds ? Math.max(0,c.holdSeconds-Math.floor((now.getTime()-(this.holds.get(rule.id)?.get(i) ?? now.getTime()))/1000)) : 0,actual:c.kind==='time'?now.toISOString():values.get(key({...c,homeName:rule.homeName})),matched:ready[i]}));
  }
  async preview(input) {
    home(input);const rule=this.list(input).find(r=>r.id===input.eventId);if(!rule)throw Error('Event not found');
    const values=new Map(this.values);
    for(const c of rule.conditions)if(c.kind!=='time'){const target={...c,homeName:rule.homeName};values.set(key(target),(await this.resolveTarget(target))?.Value);}
    const now=this.clock();return {name:rule.name,at:now.getTime(),conditions:this.conditionSnapshot(rule,now,values),actions:[],enabled:rule.enabled,matched:evaluateConditions(rule,values,now,new Set(),this.conditionReadiness(rule,now,false,values)).matched,preview:true};
  }
  attach(socket) {
    for (const event of ['GetAutomationConditions', 'GetEvents', 'SaveEvent', 'DeleteEvent', 'SetEventEnabled', 'GetEventVariables']) socket.on(event, async input => {
      const requestId = typeof input?.requestId === 'string' ? input.requestId : null;
      try {
        home(input);
        let result = {};
        if(event==='GetAutomationConditions')result={preview:await this.preview(input)};
        if (event === 'GetEvents') result = { events: this.list(input) };
        if (event === 'SaveEvent') result = { event: await this.save(input) };
        if (event === 'DeleteEvent') await this.remove(input);
        if (event === 'SetEventEnabled') result = { event: await this.toggle(input) };
        if (event === 'GetEventVariables') result = { devices: await this.catalog(input.homeName), homeLocation: this.location() };
        socket.emit(event, { requestId, status: 'OK', ...result });
      } catch (error) { socket.emit(event, { requestId, status: 'Error', message: error.message }); }
    });
  }
}
module.exports = { EventEngine, equal, compare, matches, key };
