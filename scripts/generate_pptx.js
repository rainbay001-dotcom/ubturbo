#!/usr/bin/env node
/**
 * Generate SMAP openclaw scenario diagram as a native .pptx file.
 * Pure Node.js — no external packages, no shell zip needed.
 * Run: node scripts/generate_pptx.js
 * Output: diagrams/smap_openclaw_scenario.pptx
 */

'use strict';

const fs   = require('fs');
const path = require('path');
const zlib = require('zlib');

// ── Minimal ZIP writer (STORED, no compression) ──────────────────────────────
const CRC32_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let i = 0; i < 256; i++) {
    let c = i;
    for (let j = 0; j < 8; j++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[i] = c;
  }
  return t;
})();

function crc32(buf) {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) c = CRC32_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

function u16le(n) { const b = Buffer.alloc(2); b.writeUInt16LE(n, 0); return b; }
function u32le(n) { const b = Buffer.alloc(4); b.writeUInt32LE(n >>> 0, 0); return b; }

function createZip(files) {
  const localParts = [];
  const centralParts = [];
  let offset = 0;

  for (const { name, content } of files) {
    const nameBuf  = Buffer.from(name, 'utf8');
    const dataBuf  = Buffer.isBuffer(content) ? content : Buffer.from(content, 'utf8');
    const crc      = crc32(dataBuf);
    const size     = dataBuf.length;

    // Local file header
    const local = Buffer.concat([
      Buffer.from([0x50, 0x4b, 0x03, 0x04]), // signature
      u16le(20),          // version needed
      u16le(0),           // flags
      u16le(0),           // compression: STORED
      u16le(0),           // mod time
      u16le(0),           // mod date
      u32le(crc),
      u32le(size),        // compressed size
      u32le(size),        // uncompressed size
      u16le(nameBuf.length),
      u16le(0),           // extra field length
      nameBuf,
      dataBuf,
    ]);
    localParts.push(local);

    // Central directory header
    const central = Buffer.concat([
      Buffer.from([0x50, 0x4b, 0x01, 0x02]), // signature
      u16le(20),          // version made by
      u16le(20),          // version needed
      u16le(0),
      u16le(0),           // STORED
      u16le(0),
      u16le(0),
      u32le(crc),
      u32le(size),
      u32le(size),
      u16le(nameBuf.length),
      u16le(0),           // extra
      u16le(0),           // comment
      u16le(0),           // disk start
      u16le(0),           // int attr
      u32le(0),           // ext attr
      u32le(offset),
      nameBuf,
    ]);
    centralParts.push(central);
    offset += local.length;
  }

  const centralBuf  = Buffer.concat(centralParts);
  const centralSize = centralBuf.length;
  const eocd = Buffer.concat([
    Buffer.from([0x50, 0x4b, 0x05, 0x06]), // signature
    u16le(0),                   // disk number
    u16le(0),                   // disk with central dir
    u16le(files.length),
    u16le(files.length),
    u32le(centralSize),
    u32le(offset),
    u16le(0),                   // comment length
  ]);

  return Buffer.concat([...localParts, centralBuf, eocd]);
}

// ── PPTX XML content ─────────────────────────────────────────────────────────

const SW = 12192000; // slide width EMU (13.33")
const SH = 6858000;  // slide height EMU (7.5")

const contentTypes = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/>
  <Override PartName="/ppt/slides/slide1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>
  <Override PartName="/ppt/slideLayouts/slideLayout1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml"/>
  <Override PartName="/ppt/slideMasters/slideMaster1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml"/>
  <Override PartName="/ppt/theme/theme1.xml" ContentType="application/vnd.openxmlformats-officedocument.theme+xml"/>
</Types>`;

const relsRoot = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="ppt/presentation.xml"/>
</Relationships>`;

const presentation = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:presentation xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"
  xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"
  xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"
  saveSubsetFonts="1">
  <p:sldMasterIdLst><p:sldMasterId id="2147483648" r:id="rId1"/></p:sldMasterIdLst>
  <p:sldIdLst><p:sldId id="256" r:id="rId2"/></p:sldIdLst>
  <p:sldSz cx="${SW}" cy="${SH}" type="custom"/>
  <p:notesSz cx="6858000" cy="9144000"/>
  <p:defaultTextStyle><a:defPPr><a:defRPr lang="zh-CN"/></a:defPPr></p:defaultTextStyle>
</p:presentation>`;

const presRels = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster" Target="slideMasters/slideMaster1.xml"/>
  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide1.xml"/>
  <Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme" Target="theme/theme1.xml"/>
</Relationships>`;

const theme = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<a:theme xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" name="SlateTheme">
  <a:themeElements>
    <a:clrScheme name="Slate">
      <a:dk1><a:srgbClr val="2E3D4A"/></a:dk1><a:lt1><a:srgbClr val="FFFFFF"/></a:lt1>
      <a:dk2><a:srgbClr val="3E5264"/></a:dk2><a:lt2><a:srgbClr val="F0F2F4"/></a:lt2>
      <a:accent1><a:srgbClr val="C86010"/></a:accent1>
      <a:accent2><a:srgbClr val="2E528A"/></a:accent2>
      <a:accent3><a:srgbClr val="144E6A"/></a:accent3>
      <a:accent4><a:srgbClr val="1A6A30"/></a:accent4>
      <a:accent5><a:srgbClr val="888888"/></a:accent5>
      <a:accent6><a:srgbClr val="444444"/></a:accent6>
      <a:hlink><a:srgbClr val="C86010"/></a:hlink>
      <a:folHlink><a:srgbClr val="2E528A"/></a:folHlink>
    </a:clrScheme>
    <a:fontScheme name="Slate">
      <a:majorFont><a:latin typeface="Calibri"/><a:ea typeface="Microsoft YaHei"/><a:cs typeface=""/></a:majorFont>
      <a:minorFont><a:latin typeface="Calibri"/><a:ea typeface="Microsoft YaHei"/><a:cs typeface=""/></a:minorFont>
    </a:fontScheme>
    <a:fmtScheme name="Slate">
      <a:fillStyleLst>
        <a:solidFill><a:schemeClr val="phClr"/></a:solidFill>
        <a:solidFill><a:schemeClr val="phClr"/></a:solidFill>
        <a:solidFill><a:schemeClr val="phClr"/></a:solidFill>
      </a:fillStyleLst>
      <a:lnStyleLst>
        <a:ln w="9525"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln>
        <a:ln w="25400"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln>
        <a:ln w="38100"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln>
      </a:lnStyleLst>
      <a:effectStyleLst>
        <a:effectStyle><a:effectLst/></a:effectStyle>
        <a:effectStyle><a:effectLst/></a:effectStyle>
        <a:effectStyle><a:effectLst/></a:effectStyle>
      </a:effectStyleLst>
      <a:bgFillStyleLst>
        <a:solidFill><a:schemeClr val="phClr"/></a:solidFill>
        <a:solidFill><a:schemeClr val="phClr"/></a:solidFill>
        <a:solidFill><a:schemeClr val="phClr"/></a:solidFill>
      </a:bgFillStyleLst>
    </a:fmtScheme>
  </a:themeElements>
</a:theme>`;

const slideMaster = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:sldMaster xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"
  xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"
  xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
  <p:cSld><p:bg><p:bgRef idx="1001"><a:schemeClr val="bg1"/></p:bgRef></p:bg>
    <p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>
      <p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>
    </p:spTree></p:cSld>
  <p:clrMap bg1="lt1" tx1="dk1" bg2="lt2" tx2="dk2" accent1="accent1" accent2="accent2"
    accent3="accent3" accent4="accent4" accent5="accent5" accent6="accent6"
    hlink="hlink" folHlink="folHlink"/>
  <p:sldLayoutIdLst><p:sldLayoutId id="2147483649" r:id="rId1"/></p:sldLayoutIdLst>
  <p:txStyles>
    <p:titleStyle><a:lvl1pPr><a:defRPr lang="zh-CN" sz="3600" b="1"><a:solidFill><a:schemeClr val="dk1"/></a:solidFill><a:latin typeface="Calibri"/><a:ea typeface="Microsoft YaHei"/></a:defRPr></a:lvl1pPr></p:titleStyle>
    <p:bodyStyle><a:lvl1pPr><a:defRPr lang="zh-CN" sz="1800"><a:solidFill><a:schemeClr val="dk1"/></a:solidFill><a:latin typeface="Calibri"/><a:ea typeface="Microsoft YaHei"/></a:defRPr></a:lvl1pPr></p:bodyStyle>
    <p:otherStyle><a:lvl1pPr><a:defRPr lang="zh-CN"/></a:lvl1pPr></p:otherStyle>
  </p:txStyles>
</p:sldMaster>`;

const smRels = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout" Target="../slideLayouts/slideLayout1.xml"/>
  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme" Target="../theme/theme1.xml"/>
</Relationships>`;

const slideLayout = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:sldLayout xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"
  xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"
  xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"
  type="blank" preserve="1">
  <p:cSld name="Blank"><p:spTree>
    <p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>
    <p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>
  </p:spTree></p:cSld>
  <p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>
</p:sldLayout>`;

const slRels = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster" Target="../slideMasters/slideMaster1.xml"/>
</Relationships>`;

const slideRels = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout" Target="../slideLayouts/slideLayout1.xml"/>
</Relationships>`;

// Read slide1.xml from the pptx_src directory (built from the committed source)
const slide1Path = path.resolve(__dirname, '../pptx_src/ppt/slides/slide1.xml');
let slide1;
if (fs.existsSync(slide1Path)) {
  slide1 = fs.readFileSync(slide1Path, 'utf8');
} else {
  // Fallback: minimal slide placeholder
  slide1 = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:sld xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"
  xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"
  xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
  <p:cSld><p:spTree>
    <p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>
    <p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="${SW}" cy="${SH}"/><a:chOff x="0" y="0"/><a:chExt cx="${SW}" cy="${SH}"/></a:xfrm></p:grpSpPr>
    <p:sp><p:nvSpPr><p:cNvPr id="2" name="t"/><p:cNvSpPr txBox="1"/><p:nvPr/></p:nvSpPr>
      <p:spPr><a:xfrm><a:off x="457200" y="274638"/><a:ext cx="8229600" cy="1143000"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom><a:noFill/></p:spPr>
      <p:txBody><a:bodyPr wrap="square"/><a:lstStyle/>
        <a:p><a:r><a:rPr lang="zh-CN" sz="2800" b="1"><a:latin typeface="Calibri"/></a:rPr><a:t>SMAP openclaw Scenario Diagram</a:t></a:r></a:p>
        <a:p><a:r><a:rPr lang="zh-CN" sz="1800"><a:latin typeface="Calibri"/></a:rPr><a:t>Run: node scripts/generate_pptx.js</a:t></a:r></a:p>
      </p:txBody></p:sp>
  </p:spTree></p:cSld>
  <p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>
</p:sld>`;
}

// ── Assemble PPTX ─────────────────────────────────────────────────────────────
const files = [
  { name: '[Content_Types].xml',                          content: contentTypes },
  { name: '_rels/.rels',                                   content: relsRoot },
  { name: 'ppt/presentation.xml',                          content: presentation },
  { name: 'ppt/_rels/presentation.xml.rels',               content: presRels },
  { name: 'ppt/theme/theme1.xml',                          content: theme },
  { name: 'ppt/slideMasters/slideMaster1.xml',             content: slideMaster },
  { name: 'ppt/slideMasters/_rels/slideMaster1.xml.rels',  content: smRels },
  { name: 'ppt/slideLayouts/slideLayout1.xml',             content: slideLayout },
  { name: 'ppt/slideLayouts/_rels/slideLayout1.xml.rels',  content: slRels },
  { name: 'ppt/slides/slide1.xml',                         content: slide1 },
  { name: 'ppt/slides/_rels/slide1.xml.rels',              content: slideRels },
];

const pptxBuf = createZip(files);
const outDir  = path.resolve(__dirname, '../diagrams');
if (!fs.existsSync(outDir)) fs.mkdirSync(outDir, { recursive: true });
const outPath = path.join(outDir, 'smap_openclaw_scenario.pptx');
fs.writeFileSync(outPath, pptxBuf);
console.log(`Created: ${outPath}  (${(pptxBuf.length / 1024).toFixed(1)} KB)`);
