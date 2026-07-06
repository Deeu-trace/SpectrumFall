const { execSync } = require('child_process');
const fs = require('fs');

const pandoc = 'C:\\Users\\fanzh\\AppData\\Local\\Pandoc\\pandoc.exe';
const outputPath = 'e:\\SpectrumFall\\report_assets\\reference.docx';

// Use Buffer to capture binary stdout directly
const buf = execSync(`"${pandoc}" --print-default-data-file=reference.docx`);
fs.writeFileSync(outputPath, buf);
console.log(`Saved reference.docx: ${buf.length} bytes`);
