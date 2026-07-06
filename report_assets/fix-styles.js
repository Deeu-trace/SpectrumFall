const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

// Use PowerShell to manipulate the docx zip since Node lacks built-in zip
// Strategy: extract -> modify -> repackage using PowerShell System.IO.Compression

const referencePath = 'e:\\SpectrumFall\\report_assets\\reference.docx';
const extractDir = 'e:\\SpectrumFall\\report_assets\\reference_extracted';
const stylesPath = path.join(extractDir, 'word', 'styles.xml');

// Step 1: Read current styles.xml content (already extracted to reference-styles.xml)
const stylesFile = 'e:\\SpectrumFall\\report_assets\\reference-styles.xml';
let content = fs.readFileSync(stylesFile, 'utf8');

console.log('Original styles.xml length:', content.length);

// Step 2: Replace theme-based fonts with explicit font names
// Change: w:asciiTheme="minorHAnsi" w:eastAsiaTheme="minorEastAsia" w:hAnsiTheme="minorHAnsi" w:cstheme="minorBidi"
// To: w:ascii="Calibri" w:eastAsia="宋体" w:hAnsi="Calibri" w:cs="Times New Roman"
const oldFonts1 = 'w:asciiTheme="minorHAnsi" w:eastAsiaTheme="minorEastAsia" w:hAnsiTheme="minorHAnsi" w:cstheme="minorBidi"';
const newFonts1 = 'w:ascii="Calibri" w:eastAsia="宋体" w:hAnsi="Calibri" w:cs="Times New Roman"';

if (content.includes(oldFonts1)) {
  content = content.replace(oldFonts1, newFonts1);
  console.log('Replaced docDefaults rFonts');
} else {
  console.log('WARN: docDefaults rFonts pattern not found');
}

// Also handle majorHAnsi / majorEastAsia theme references (for headings)
const oldFonts2 = 'w:asciiTheme="majorHAnsi" w:eastAsiaTheme="majorEastAsia" w:hAnsiTheme="majorHAnsi" w:cstheme="majorBidi"';
const newFonts2 = 'w:ascii="Calibri" w:eastAsia="宋体" w:hAnsi="Calibri" w:cs="Times New Roman"';

if (content.includes(oldFonts2)) {
  content = content.replace(oldFonts2, newFonts2);
  console.log('Replaced heading rFonts');
}

// Also handle any remaining eastAsiaTheme references
const remaining = (content.match(/eastAsiaTheme/g) || []).length;
console.log('Remaining eastAsiaTheme refs:', remaining);
if (remaining > 0) {
  content = content.replace(/w:eastAsiaTheme="[^"]*"/g, 'w:eastAsia="宋体"');
  console.log('Replaced all remaining eastAsiaTheme');
}

const remaining2 = (content.match(/asciiTheme/g) || []).length;
if (remaining2 > 0) {
  content = content.replace(/w:asciiTheme="[^"]*"/g, 'w:ascii="Calibri"');
  content = content.replace(/w:hAnsiTheme="[^"]*"/g, 'w:hAnsi="Calibri"');
  content = content.replace(/w:cstheme="[^"]*"/g, 'w:cs="Times New Roman"');
  console.log('Replaced all remaining theme refs');
}

// Step 3: Write modified styles.xml
fs.writeFileSync(stylesFile, content, 'utf8');
console.log('Written modified styles.xml, length:', content.length);

console.log('Done. styles.xml is ready at:', stylesFile);
