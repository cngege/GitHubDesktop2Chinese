#!/usr/bin/env node
// 与 GitHubDesktop2Chinese.exe --invalidcheck 同语义的独立校验脚本。
//
// 用途：修改 json/localization.json 后，在本地快速确认是否存在失效替换项，
// 无需等 CI、也无须运行完整汉化流程。
//
// 用法：
//   node check_localization.js <localization.json路径> <基准目录> [--dev]
//     <基准目录> 为含 main.js / renderer.js 的目录（GitHub Desktop 的 resources/app），
//     若存在 .bak 备份则优先使用（与 --invalidcheck 行为一致）。
//     --dev 额外校验 main_dev / renderer_dev；默认不校验开发映射，保持 CI 行为不变。
//
// 退出码：
//   0 = 全部有效；1 = 存在失效项或结构问题；2 = 参数/读文件/JSON 解析错误。
//
// 除失效检测外，还会做两项静态校验（只打印，不改变失效项计数）：
//   - 结构校验：段/条目必须是数组、条目至少两项且均为字符串、select 必备字段齐全；
//   - '$' 转义提示：替换串会作为 std::regex_replace 的格式串，字面量 '$' 必须写成 '$$'，
//     否则该条规则会把 '$1'/'$&' 当成反向引用，静默吞掉内容。
const fs = require('fs');

const jsonPath = process.argv[2];
const baseDir = process.argv[3];
const devMode = process.argv.slice(4).includes('--dev');

if (!jsonPath || !baseDir) {
  console.error('用法: node check_localization.js <localization.json路径> <基准目录> [--dev]');
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
let structureErrors = 0;
let warnings = 0;

// ---------- 结构校验 ----------
const structureError = (msg) => {
  console.log(`[structure] ${msg}`);
  structureErrors++;
};

if (!d || typeof d !== 'object' || Array.isArray(d)) {
  structureError('顶层必须是 JSON 对象');
} else {
  for (const sec of ['main', 'main_dev', 'renderer', 'renderer_dev']) {
    if (!(sec in d)) {
      structureError(`缺少 ${sec} 段`);
      continue;
    }
    if (!Array.isArray(d[sec])) {
      structureError(`${sec} 必须是数组`);
      continue;
    }
    d[sec].forEach((it, i) => {
      if (!Array.isArray(it) || it.length < 2) {
        structureError(`${sec}[${i}] 必须是至少含 2 项的数组`);
        return;
      }
      it.forEach((v, j) => {
        if (typeof v !== 'string') structureError(`${sec}[${i}][${j}] 必须是字符串`);
      });
    });
  }
  if ('select' in d) {
    if (!Array.isArray(d.select)) {
      structureError('select 必须是数组');
    } else {
      d.select.forEach((s, i) => {
        if (!s || typeof s !== 'object' || Array.isArray(s)) {
          structureError(`select[${i}] 必须是对象`);
          return;
        }
        for (const k of ['replaceFile', 'enable', 'replace']) {
          if (!(k in s)) structureError(`select[${i}] 缺少 ${k}`);
        }
        if (s.replace !== undefined && !Array.isArray(s.replace)) {
          structureError(`select[${i}].replace 必须是数组`);
        }
      });
    }
  }
}

// ---------- 替换串 '$' 转义提示 ----------
const lintReplacement = (where, s) => {
  if (typeof s !== 'string') return;
  for (let i = 0; i < s.length; i++) {
    if (s[i] !== '$') continue;
    const next = s[i + 1] || '';
    // 合法写法：$$（字面量 $）、$1..$9、$&、$`、$'
    if (next === '$' || next === '&' || next === '`' || next === "'" || /[0-9]/.test(next)) {
      if (next === '$') i++;
      continue;
    }
    console.log(`[warn] ${where} 替换串含未转义的 $（应写作 $$）: ${s.slice(Math.max(0, i - 20), i + 20)}`);
    warnings++;
    return; // 同一条规则只提示一次
  }
};

// cpp 中空字符串或字面量 '""' 都会跳过
const skip = (s) => !s || s === '""';

// 规则的第 4 项是平台限定（"windows" / "macos" / "linux"）。
// 同一段文案在 Windows 与 macOS 上措辞不同，限定平台后，
// 另一个平台既不会替换，也不会被算成失效项（与 loader 的处理保持一致）。
//
// 平台默认按当前机器推断；可用环境变量 GD2C_PLATFORM 覆盖，
// 这样在 Windows 上也能校验 macOS/Linux 的映射：
//   $env:GD2C_PLATFORM = "macos"; node check_localization.js ...
const detectPlatform = () => {
  const forced = (process.env.GD2C_PLATFORM || '').trim().toLowerCase();
  if (forced) return forced;
  if (process.platform === 'win32') return 'windows';
  if (process.platform === 'darwin') return 'macos';
  if (process.platform === 'linux') return 'linux';
  return 'any';
};

const currentPlatform = detectPlatform();

const platformMatches = (tag) => {
  if (!tag || tag === 'any') return true;
  // 可用逗号分隔多个平台（如 "windows,linux"）
  return tag
    .split(',')
    .map((s) => s.trim().toLowerCase())
    .includes(currentPlatform);
};

// 第 3 项是关键字定位串，留空表示没有关键字
const hasKeyword = (item) =>
  Array.isArray(item) && item.length >= 3 && typeof item[2] === 'string' && item[2] !== '';

// JS 正则与 std::regex(ECMAScript 语法) 在本场景下语义一致，
// 但 std::regex 不支持 lookbehind(?<=) 等新增语法，新增规则请避免使用。
const checkSection = (section, text, tag) => {
  if (!Array.isArray(d[section])) return;
  for (const it of d[section]) {
    if (!Array.isArray(it) || skip(it[0])) continue;
    if (!platformMatches(it[3])) continue; // 平台限定：非当前平台的规则跳过
    lintReplacement(`${tag} ${it[0]}`, it[1]);
    try {
      if (new RegExp(it[0]).test(text)) continue;
      console.log(`[${tag}] ${it[0]}`);
      total++;
    } catch (e) {
      console.log(`[${tag}] 无效正则(${e.message}): ${it[0]}`);
      total++;
    }
    if (hasKeyword(it)) {
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
    if (!s || typeof s !== 'object') continue;
    if (s.replaceFile !== file || !s.enable) continue;
    if (!Array.isArray(s.replace)) continue;
    for (const v of s.replace) {
      if (!Array.isArray(v) || skip(v[0])) continue;
      lintReplacement(`${tag} ${v[0]}`, v[1]);
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

// 开发映射默认不参与 CI 校验（其中常含只针对本地环境的实验规则），
// 需要时显式加 --dev，与加载器 dev --devreplace 的 *_dev 段语义对齐。
if (devMode) {
  checkSection('main_dev', mainStr, 'main_dev');
  checkSection('renderer_dev', rendererStr, 'renderer_dev');
  console.log('校验范围: 含 main_dev / renderer_dev（--dev）');
}

console.log(`失效项总数: ${total}`);
if (structureErrors > 0) console.log(`结构问题: ${structureErrors}`);
if (warnings > 0) console.log(`转义提示: ${warnings}（不影响退出码）`);
process.exit(total > 0 || structureErrors > 0 ? 1 : 0);
