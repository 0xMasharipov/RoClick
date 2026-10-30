const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const Module = require('node:module');
const ts = require('typescript');
const filename = path.resolve(__dirname, '../src/i18n.ts');
const compiled = ts.transpileModule(fs.readFileSync(filename, 'utf8'), {
  compilerOptions: {module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020},
}).outputText;
const translations = new Module(filename, module);
translations._compile(compiled, filename);
const {en, tr, translate, formatNumber, formatRate} = translations.exports;
assert.deepEqual(Object.keys(tr).sort(), Object.keys(en).sort());
for (const key of Object.keys(en)) {
  assert.ok(en[key].trim() && tr[key].trim(), key);
  const placeholders = text => [...text.matchAll(/\{(\w+)\}/g)].map(match => match[1]).sort();
  assert.deepEqual(placeholders(en[key]), placeholders(tr[key]), key);
}
const engine = fs.readFileSync(path.resolve(__dirname, '../native/core/Engine.h'), 'utf8');
const errorKeys = [...engine.matchAll(/case ErrorKind::\w+: return "([^"]+)";/g)].map(match => match[1]);
assert.equal(errorKeys.length, 9);
for (const key of errorKeys) assert.ok(en[`error_${key}`] && tr[`error_${key}`], key);
assert.equal(formatNumber(1234567.5, 'en', 1), '1,234,567.5');
assert.equal(formatNumber(1234567.5, 'tr', 1), '1.234.567,5');
assert.equal(formatNumber(0, 'tr', 1), '0,0');
assert.equal(formatNumber(1000, 'tr'), '1.000');
assert.equal(formatRate(10, 'tr'), '100');
assert.equal(formatRate(3, 'tr'), '333,3');
assert.equal(formatRate(1, 'en'), '1,000');
assert.equal(translate('tr', 'invalidPress', {max: 9}), '0–9 ms arasında tam sayı bir basılı tutma süresi gir.');
assert.match(translate('tr', 'right'), /ğ/);
assert.match(translate('tr', 'running'), /ı/);
console.log('Localization passed: key parity, placeholders, native errors, Turkish glyphs and number formatting.');
