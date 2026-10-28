#!/usr/bin/env node
// 与 GitHubDesktop2Chinese.exe --invalidcheck 同语义的独立校验脚本。
//
// 用途：修改 json/localization.json 后，在本地快速确认是否存在失效替换项，
// 无需等 CI、也无需运行完整汉化流程。
//
// 用法：
//   node check_localization.js <localization.json路径> <基准目录>
//     <基准目录> 为含 main.js / renderer.js 的目录（GitHub Desktop 的 resources/app），
//     若存在 .bak 备份则优先使用（与 --invalidcheck 行为一致）。
//
// 退出码：失效项总数（0 表示全部有效）。
const fs = require('fs');

const jsonPath = process.argv[2];
const baseDir = process.argv[3];

if (!jsonPath || !baseDir) {
  console.error('用法: node check_localization.js <localization.json路径> <基准目录>');
  process.exit(2);
}

let d;
try {
  d = JSON.parse(fs.readFileSync(jsonPath, 'utf8'));
} catch (e) {
  console.error(`[invalid_check] 解析 localization.json 失败: ${e.message}`);
  process.exit(2);
}

const readJs = (name) => {
  for (const cand of [name + '.bak', name]) {
    const full = baseDir.replace(/[\\/]$/, '') + '/' + cand;
    if (fs.existsSync(full)) return fs.readFileSync(full, 'utf8');
  }
  throw new Error(`找不到文件: ${baseDir}/${name}(.bak)`);
};

let mainStr, rendererStr;
try {
  mainStr = readJs('main.js');
  rendererStr = readJs('renderer.js');
} catch (e) {
  console.error(`[invalid_check] ${e.message}`);
  process.exit(2);
}

let total = 0;

// cpp 中空字符串或字面量 '""' 都会跳过
const skip = (s) => !s || s === '""';

// JS 正则与 std::regex(ECMAScript 语法) 在本场景下语义一致
const checkSection = (section, text, tag) => {
  if (!d[section]) return;
  for (const it of d[section]) {
    if (!Array.isArray(it) || skip(it[0])) continue;
    try {
      if (new RegExp(it[0]).test(text)) continue;
      console.log(`[${tag}] ${it[0]}`);
      total++;
    } catch (e) {
      console.log(`[${tag}] 无效正则(${e.message}): ${it[0]}`);
      total++;
    }
    if (it.length >= 3 && !skip(it[2])) {
      try {
        if (new RegExp(it[2]).test(text)) continue;
        console.log(`[${tag} third] ${it[2]}`);
        total++;
      } catch (e) {
        console.log(`[${tag} third] 无效正则(${e.message}): ${it[2]}`);
        total++;
      }
    }
  }
};

const checkSelect = (text, file, tag) => {
  if (!Array.isArray(d.select)) return;
  for (const s of d.select) {
    if (s.replaceFile !== file || !s.enable) continue;
    for (const v of s.replace) {
      if (skip(v[0])) continue;
      try {
        if (new RegExp(v[0]).test(text)) continue;
        console.log(`[${tag}] ${v[0]}`);
        total++;
      } catch (e) {
        console.log(`[${tag}] 无效正则(${e.message}): ${v[0]}`);
        total++;
      }
      if (v.length >= 3 && !skip(v[2])) {
        try {
          if (new RegExp(v[2]).test(text)) continue;
          console.log(`[${tag} third] ${v[2]}`);
          total++;
        } catch (e) {
          console.log(`[${tag} third] 无效正则(${e.message}): ${v[2]}`);
          total++;
        }
      }
    }
  }
};

checkSection('main', mainStr, 'main');
checkSelect(mainStr, 'main.js', 'select main');
checkSection('renderer', rendererStr, 'renderer');
checkSelect(rendererStr, 'renderer.js', 'select renderer');

console.log(`失效项总数: ${total}`);
process.exit(total > 0 ? 1 : 0);
