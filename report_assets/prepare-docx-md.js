const fs = require('fs');
const path = require('path');

const sourceMd = 'e:\\SpectrumFall\\课程设计报告.md';
const targetMd = 'e:\\SpectrumFall\\report_docx_source.md';

console.log('Reading source markdown...');
let content = fs.readFileSync(sourceMd, 'utf8');

// Image references to insert (in order of mermaid blocks)
const images = [
  '![图 3-1 系统整体类图](report_assets/images/class-diagram-overall.png)\n\n**图 3-1 系统整体类图**',
  '![图 3-2 可视化模块类图](report_assets/images/class-diagram-visualizer.png)\n\n**图 3-2 可视化模块类图**',
  '![图 3-3 数据持久化类图](report_assets/images/class-diagram-persistence.png)\n\n**图 3-3 数据持久化类图**'
];

// Match all ```mermaid ... ``` blocks (non-greedy, multiline)
const pattern = /```mermaid[\s\S]*?```/g;
const matches = [];
let m;
while ((m = pattern.exec(content)) !== null) {
  matches.push({ index: m.index, length: m[0].length });
}
console.log(`Found ${matches.length} mermaid blocks`);

// Replace from end to start to preserve indices
for (let i = matches.length - 1; i >= 0; i--) {
  const match = matches[i];
  const imgIdx = Math.min(i, images.length - 1);
  const replacement = images[imgIdx];
  content = content.substring(0, match.index) + replacement + content.substring(match.index + match.length);
  console.log(`  [OK] Replaced block #${i + 1}`);
}

console.log(`Writing target markdown: ${targetMd}`);
fs.writeFileSync(targetMd, content, 'utf8');

const stats = fs.statSync(targetMd);
console.log(`Done. File size: ${stats.size} bytes`);
