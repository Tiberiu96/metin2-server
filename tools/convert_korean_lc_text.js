#!/usr/bin/env node
// convert_korean_lc_text.js
// Converts LC_TEXT("Korean-EUC-KR") → English in all C++ source files.
// Reads locale_string.txt to build Korean→English map.
// Writes files using latin1 (byte-safe, no encoding conversion).

const fs   = require('fs');
const path = require('path');
const {execFileSync} = require('child_process');

// ── Config ────────────────────────────────────────────────────────────────
const ROOT        = path.resolve(__dirname, '..');
const LOCALE_FILE = path.join(ROOT, 'server/share/locale/english/locale_string.txt');
const SRC_DIRS    = ['src/server/game/src', 'src/server/db/src'];
const DRY_RUN     = process.argv.includes('--dry');

// ── Build Korean→English map from locale_string.txt ───────────────────────
function buildLocaleMap(filePath) {
    const text = fs.readFileSync(filePath).toString('latin1');
    const map  = new Map();

    // Format: "Korean string";\n"English string";\n (blank line between pairs)
    const lineRe = /^"((?:[^"\\]|\\.)*)";$/;
    const lines  = text.split('\n');
    let i = 0;
    while (i < lines.length) {
        const l1 = lines[i].trim();
        if (!l1) { i++; continue; }
        const m1 = l1.match(lineRe);
        if (!m1) { i++; continue; }
        const korean = m1[1];
        // Peek next non-empty line
        let j = i + 1;
        while (j < lines.length && !lines[j].trim()) j++;
        if (j < lines.length) {
            const m2 = lines[j].trim().match(lineRe);
            if (m2 && /[\x80-\xff]/.test(korean) && !/[\x80-\xff]/.test(m2[1])) {
                map.set(korean, m2[1]);
                i = j + 1;
                continue;
            }
        }
        i++;
    }
    return map;
}

// ── Process a single source file ──────────────────────────────────────────
function processFile(filePath, localeMap, stats) {
    const original = fs.readFileSync(filePath).toString('latin1');
    let result     = original;
    let changed    = 0;

    // Pattern 1: LC_TEXT_LANG(LC_TEXT("Korean"), expr)
    // → LC_TEXT_LANG("English", expr)
    result = result.replace(
        /LC_TEXT_LANG\(LC_TEXT\("((?:[^"\\]|\\.)*)"\)\s*,\s*([^)]+)\)/g,
        (full, korean, langExpr) => {
            if (!/[\x80-\xff]/.test(korean)) return full; // already ASCII
            const english = localeMap.get(korean);
            if (english !== undefined) {
                changed++;
                stats.converted++;
                return `LC_TEXT_LANG("${english}", ${langExpr.trim()})`;
            }
            stats.missing++;
            stats.missingList.push({ file: filePath, korean });
            return full; // leave as-is
        }
    );

    // Pattern 2: standalone LC_TEXT("Korean")  (not wrapped in LC_TEXT_LANG above)
    result = result.replace(
        /LC_TEXT\("((?:[^"\\]|\\.)*)"\)/g,
        (full, korean) => {
            if (!/[\x80-\xff]/.test(korean)) return full; // already ASCII
            const english = localeMap.get(korean);
            if (english !== undefined) {
                changed++;
                stats.converted++;
                return `LC_TEXT("${english}")`;
            }
            stats.missing++;
            stats.missingList.push({ file: filePath, korean });
            return full;
        }
    );

    if (changed > 0 && !DRY_RUN) {
        fs.writeFileSync(filePath, Buffer.from(result, 'latin1'));
    }
    return changed;
}

// ── Main ──────────────────────────────────────────────────────────────────
console.log(DRY_RUN ? '[DRY RUN]' : '[LIVE]', 'Converting Korean LC_TEXT strings...\n');

const localeMap = buildLocaleMap(LOCALE_FILE);

// Merge extra translations
const extraFile = path.join(__dirname, 'extra_translations.json');
if (fs.existsSync(extraFile)) {
    const extra = JSON.parse(fs.readFileSync(extraFile, 'utf8'));
    for (const [k, v] of Object.entries(extra)) localeMap.set(k, v);
}

console.log(`Locale map: ${localeMap.size} Korean→English entries\n`);

const stats = { converted: 0, missing: 0, filesChanged: 0, missingList: [] };

// Get all tracked cpp/h files in SRC_DIRS
const allFiles = execFileSync('git', ['ls-files', ...SRC_DIRS], { cwd: ROOT, maxBuffer: 10*1024*1024 })
    .toString('latin1').trim().split('\n')
    .filter(f => /\.(cpp|h)$/.test(f))
    .map(f => path.join(ROOT, f.trim()));

for (const f of allFiles) {
    const n = processFile(f, localeMap, stats);
    if (n > 0) {
        stats.filesChanged++;
        console.log(`  ${n.toString().padStart(4)}  ${path.relative(ROOT, f)}`);
    }
}

console.log(`
══════════════════════════════════════
 Converted : ${stats.converted}
 Not found : ${stats.missing}
 Files     : ${stats.filesChanged}
══════════════════════════════════════`);

if (stats.missing > 0) {
    console.log('\nNot found in locale_string.txt:');
    // Deduplicate by korean string
    const seen = new Set();
    for (const { file, korean } of stats.missingList) {
        const key = korean.substring(0, 40);
        if (!seen.has(key)) {
            seen.add(key);
            console.log('  ', path.relative(ROOT, file), '|', JSON.stringify(korean.substring(0, 50)));
        }
    }
}
