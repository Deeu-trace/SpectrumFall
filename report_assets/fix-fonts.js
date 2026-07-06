const fs = require('fs');
const path = require('path');

const baseDir = 'e:\\SpectrumFall\\report_assets\\reference_extracted\\word';

// ===== 1. Fix theme1.xml =====
const themePath = path.join(baseDir, 'theme', 'theme1.xml');
let theme = fs.readFileSync(themePath, 'utf8');
console.log('Original theme1.xml length:', theme.length);

// Replace Aptos Display (majorFont latin) with Calibri
theme = theme.replace('<a:latin typeface="Aptos Display" panose="02110004020202020204"/>',
                     '<a:latin typeface="Calibri" panose="02020503020202020204"/>');
// Replace Aptos (minorFont latin) with Calibri
theme = theme.replace('<a:latin typeface="Aptos" panose="02110004020202020204"/>',
                     '<a:latin typeface="Calibri" panose="02020503020202020204"/>');

// Set majorFont ea typeface to SimSun (was empty)
// The pattern: <a:majorFont> ... <a:ea typeface=""/> -> need to target the majorFont block
// Since both major and minor have <a:ea typeface=""/>, we replace all
let eaCount = (theme.match(/<a:ea typeface=""/g) || []).length;
console.log('Empty <a:ea typeface=""/> count:', eaCount);
theme = theme.replace(/<a:ea typeface=""/g, '<a:ea typeface="SimSun"/>');

// Replace Hans font (等线 Light / 等线) with SimSun for consistency
theme = theme.replace('<a:font script="Hans" typeface="等线 Light"/>', '<a:font script="Hans" typeface="SimSun"/>');
theme = theme.replace('<a:font script="Hans" typeface="等线"/>', '<a:font script="Hans" typeface="SimSun"/>');

fs.writeFileSync(themePath, theme, 'utf8');
console.log('Written modified theme1.xml, length:', theme.length);

// ===== 2. Fix styles.xml =====
const stylesPath = path.join(baseDir, 'styles.xml');
let styles = fs.readFileSync(stylesPath, 'utf8');
console.log('\nOriginal styles.xml length:', styles.length);

// Replace theme-based font references in docDefaults with explicit fonts
// old: w:asciiTheme="minorHAnsi" w:eastAsiaTheme="minorEastAsia" w:hAnsiTheme="minorHAnsi" w:cstheme="minorBidi"
// new: explicit Calibri + SimSun
const oldMinor = 'w:asciiTheme="minorHAnsi" w:eastAsiaTheme="minorEastAsia" w:hAnsiTheme="minorHAnsi" w:cstheme="minorBidi"';
const newMinor = 'w:ascii="Calibri" w:eastAsia="SimSun" w:hAnsi="Calibri" w:cs="Times New Roman"';
if (styles.includes(oldMinor)) {
  styles = styles.replace(oldMinor, newMinor);
  console.log('Replaced minorFont theme refs in docDefaults');
}

const oldMajor = 'w:asciiTheme="majorHAnsi" w:eastAsiaTheme="majorEastAsia" w:hAnsiTheme="majorHAnsi" w:cstheme="majorBidi"';
const newMajor = 'w:ascii="Calibri" w:eastAsia="SimSun" w:hAnsi="Calibri" w:cs="Times New Roman"';
if (styles.includes(oldMajor)) {
  styles = styles.replace(oldMajor, newMajor);
  console.log('Replaced majorFont theme refs in styles');
}

// Replace any remaining eastAsiaTheme references
const remainingEastAsia = (styles.match(/eastAsiaTheme/g) || []).length;
if (remainingEastAsia > 0) {
  styles = styles.replace(/w:eastAsiaTheme="[^"]*"/g, 'w:eastAsia="SimSun"');
  console.log('Replaced', remainingEastAsia, 'remaining eastAsiaTheme refs');
}

const remainingAscii = (styles.match(/asciiTheme/g) || []).length;
if (remainingAscii > 0) {
  styles = styles.replace(/w:asciiTheme="[^"]*"/g, 'w:ascii="Calibri"');
  styles = styles.replace(/w:hAnsiTheme="[^"]*"/g, 'w:hAnsi="Calibri"');
  styles = styles.replace(/w:cstheme="[^"]*"/g, 'w:cs="Times New Roman"');
  console.log('Replaced', remainingAscii, 'remaining asciiTheme refs');
}

fs.writeFileSync(stylesPath, styles, 'utf8');
console.log('Written modified styles.xml, length:', styles.length);

console.log('\nAll done. Ready to repackage.');
