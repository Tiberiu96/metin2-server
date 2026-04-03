// decode_missing.js — decode EUC-KR missing LC_TEXT strings and output JSON
const {execFileSync} = require('child_process');
const fs   = require('fs');
const path = require('path');

const ROOT        = path.resolve(__dirname, '..');
const LOCALE_FILE = path.join(ROOT, 'server/share/locale/english/locale_string.txt');

function buildLocaleMap(f) {
    const lines = fs.readFileSync(f).toString('latin1').split('\n');
    const map   = new Map();
    let i = 0;
    while (i < lines.length) {
        const l1 = lines[i].trim();
        if (!l1) { i++; continue; }
        if (l1[0] !== '"') { i++; continue; }
        const korean  = l1.slice(1, l1.lastIndexOf('";'));
        let j = i + 1;
        while (j < lines.length && !lines[j].trim()) j++;
        if (j < lines.length) {
            const l2 = lines[j].trim();
            if (l2[0] === '"') {
                const english = l2.slice(1, l2.lastIndexOf('";'));
                if (/[\x80-\xff]/.test(korean) && !/[\x80-\xff]/.test(english)) {
                    map.set(korean, english);
                    i = j + 1; continue;
                }
            }
        }
        i++;
    }
    return map;
}

const localeMap = buildLocaleMap(LOCALE_FILE);
const files = execFileSync('git', ['ls-files', 'src/server/game/src', 'src/server/db/src'], { cwd: ROOT, maxBuffer: 10*1024*1024 })
    .toString('latin1').trim().split('\n')
    .filter(f => /\.(cpp|h)$/.test(f));

const missing = new Map(); // latin1 key -> first file
for (const f of files) {
    const text = fs.readFileSync(path.join(ROOT, f.trim())).toString('latin1');
    // Match LC_TEXT("...") and LC_TEXT_LANG(LC_TEXT("..."), ...)
    const re = /LC_TEXT\("([^"]*)"\)/g;
    let m;
    while ((m = re.exec(text)) !== null) {
        const k = m[1];
        if (/[\x80-\xff]/.test(k) && !localeMap.has(k) && !missing.has(k))
            missing.set(k, f.trim());
    }
}

const result = [];
for (const [latin1, file] of missing) {
    const buf = Buffer.from(latin1, 'latin1');
    let decoded = '';
    try { decoded = new TextDecoder('euc-kr').decode(buf); } catch(e) { decoded = '?'; }
    result.push({ latin1, decoded, file: file.replace(/\\/g, '/') });
}

console.log(JSON.stringify(result, null, 2));
