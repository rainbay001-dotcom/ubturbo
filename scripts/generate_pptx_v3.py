#!/usr/bin/env python3
"""
Generate 10 PPTX slides (SMAP / openclaw scenario) using only Python built-ins.
Each file is a single-slide 16:9 presentation in a distinct visual style.
"""

import zipfile, io, os, textwrap

# ──────────────────────────── helpers ────────────────────────────
def xe(s):
    return str(s).replace('&','&amp;').replace('<','&lt;').replace('>','&gt;').replace('"','&quot;')

# Slide dimensions (16:9, 13.33" × 7.5")
SW, SH = 12192000, 6858000

# ──────────────────────── static PPTX XML ────────────────────────
CONTENT_TYPES = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n' \
'<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">\n' \
'  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>\n' \
'  <Default Extension="xml" ContentType="application/xml"/>\n' \
'  <Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/>\n' \
'  <Override PartName="/ppt/slides/slide1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>\n' \
'  <Override PartName="/ppt/slideLayouts/slideLayout1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml"/>\n' \
'  <Override PartName="/ppt/slideMasters/slideMaster1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml"/>\n' \
'  <Override PartName="/ppt/theme/theme1.xml" ContentType="application/vnd.openxmlformats-officedocument.theme+xml"/>\n' \
'</Types>'

ROOT_RELS = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n' \
'<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n' \
'  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="ppt/presentation.xml"/>\n' \
'</Relationships>'

PRESENTATION = f'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n' \
f'<p:presentation xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"\n' \
f'                xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"\n' \
f'                xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"\n' \
f'                saveSubsetFonts="1">\n' \
f'  <p:sldMasterIdLst><p:sldMasterId id="2147483648" r:id="rId1"/></p:sldMasterIdLst>\n' \
f'  <p:sldIdLst><p:sldId id="256" r:id="rId2"/></p:sldIdLst>\n' \
f'  <p:sldSz cx="{SW}" cy="{SH}"/>\n' \
f'  <p:notesSz cx="6858000" cy="9144000"/>\n' \
f'  <p:defaultTextStyle><a:defPPr><a:defRPr lang="zh-CN"/></a:defPPr></p:defaultTextStyle>\n' \
f'</p:presentation>'

PRES_RELS = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n' \
'<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n' \
'  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster" Target="slideMasters/slideMaster1.xml"/>\n' \
'  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide1.xml"/>\n' \
'</Relationships>'

SLIDE_RELS = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n' \
'<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n' \
'  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout" Target="../slideLayouts/slideLayout1.xml"/>\n' \
'</Relationships>'

LAYOUT_RELS = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n' \
'<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n' \
'  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster" Target="../slideMasters/slideMaster1.xml"/>\n' \
'</Relationships>'

MASTER_RELS = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n' \
'<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">\n' \
'  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme" Target="../theme/theme1.xml"/>\n' \
'  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout" Target="../slideLayouts/slideLayout1.xml"/>\n' \
'</Relationships>'

THEME = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n' \
'<a:theme xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" name="Office Theme">\n' \
'  <a:themeElements>\n' \
'    <a:clrScheme name="Office">\n' \
'      <a:dk1><a:sysClr lastClr="000000" val="windowText"/></a:dk1>\n' \
'      <a:lt1><a:sysClr lastClr="FFFFFF" val="window"/></a:lt1>\n' \
'      <a:dk2><a:srgbClr val="44546A"/></a:dk2>\n' \
'      <a:lt2><a:srgbClr val="E7E6E6"/></a:lt2>\n' \
'      <a:accent1><a:srgbClr val="4472C4"/></a:accent1>\n' \
'      <a:accent2><a:srgbClr val="ED7D31"/></a:accent2>\n' \
'      <a:accent3><a:srgbClr val="A9D18E"/></a:accent3>\n' \
'      <a:accent4><a:srgbClr val="FF0000"/></a:accent4>\n' \
'      <a:accent5><a:srgbClr val="FFC000"/></a:accent5>\n' \
'      <a:accent6><a:srgbClr val="70AD47"/></a:accent6>\n' \
'      <a:hlink><a:srgbClr val="0563C1"/></a:hlink>\n' \
'      <a:folHlink><a:srgbClr val="954F72"/></a:folHlink>\n' \
'    </a:clrScheme>\n' \
'    <a:fontScheme name="Office">\n' \
'      <a:majorFont><a:latin typeface="Calibri Light"/><a:ea typeface="Microsoft YaHei Light"/><a:cs typeface=""/></a:majorFont>\n' \
'      <a:minorFont><a:latin typeface="Calibri"/><a:ea typeface="Microsoft YaHei"/><a:cs typeface=""/></a:minorFont>\n' \
'    </a:fontScheme>\n' \
'    <a:fmtScheme name="Office">\n' \
'      <a:fillStyleLst>\n' \
'        <a:solidFill><a:schemeClr val="phClr"/></a:solidFill>\n' \
'        <a:solidFill><a:schemeClr val="phClr"/></a:solidFill>\n' \
'        <a:solidFill><a:schemeClr val="phClr"/></a:solidFill>\n' \
'      </a:fillStyleLst>\n' \
'      <a:lnStyleLst>\n' \
'        <a:ln w="6350"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln>\n' \
'        <a:ln w="12700"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln>\n' \
'        <a:ln w="19050"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln>\n' \
'      </a:lnStyleLst>\n' \
'      <a:effectStyleLst>\n' \
'        <a:effectStyle><a:effectLst/></a:effectStyle>\n' \
'        <a:effectStyle><a:effectLst/></a:effectStyle>\n' \
'        <a:effectStyle><a:effectLst/></a:effectStyle>\n' \
'      </a:effectStyleLst>\n' \
'      <a:bgFillStyleLst>\n' \
'        <a:solidFill><a:schemeClr val="phClr"/></a:solidFill>\n' \
'        <a:solidFill><a:schemeClr val="phClr"/></a:solidFill>\n' \
'        <a:solidFill><a:schemeClr val="phClr"/></a:solidFill>\n' \
'      </a:bgFillStyleLst>\n' \
'    </a:fmtScheme>\n' \
'  </a:themeElements>\n' \
'</a:theme>'

SLIDE_MASTER = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n' \
'<p:sldMaster xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"\n' \
'             xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"\n' \
'             xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">\n' \
'  <p:cSld><p:spTree>\n' \
'    <p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>\n' \
'    <p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>\n' \
'  </p:spTree></p:cSld>\n' \
'  <p:clrMap bg1="lt1" tx1="dk1" bg2="lt2" tx2="dk2" accent1="accent1" accent2="accent2" accent3="accent3" accent4="accent4" accent5="accent5" accent6="accent6" hlink="hlink" folHlink="folHlink"/>\n' \
'  <p:sldLayoutIdLst><p:sldLayoutId id="2147483649" r:id="rId2"/></p:sldLayoutIdLst>\n' \
'  <p:txStyles>\n' \
'    <p:titleStyle><a:lvl1pPr><a:defRPr lang="zh-CN" sz="4400" dirty="0"><a:solidFill><a:schemeClr val="tx1"/></a:solidFill></a:defRPr></a:lvl1pPr></p:titleStyle>\n' \
'    <p:bodyStyle><a:lvl1pPr><a:defRPr lang="zh-CN" sz="2000" dirty="0"/></a:lvl1pPr></p:bodyStyle>\n' \
'    <p:otherStyle><a:defPPr><a:defRPr lang="zh-CN"/></a:defPPr></p:otherStyle>\n' \
'  </p:txStyles>\n' \
'</p:sldMaster>'

SLIDE_LAYOUT = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n' \
'<p:sldLayout xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"\n' \
'             xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"\n' \
'             xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"\n' \
'             type="blank" preserve="1">\n' \
'  <p:cSld name="Blank"><p:spTree>\n' \
'    <p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>\n' \
'    <p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>\n' \
'  </p:spTree></p:cSld>\n' \
'  <p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>\n' \
'</p:sldLayout>'


# ──────────────────────── shape builder ──────────────────────────
class S:
    """Slide shape collector."""
    def __init__(self):
        self._id = 2
        self._shapes = []

    def _nid(self):
        i = self._id; self._id += 1; return i

    def rect(self, x, y, w, h, fill=None, stroke=None, sw=12700,
             radius=0, paras=None, anchor='ctr', name=None, no_fill=False):
        nid = self._nid()
        nm = xe(name or f'Sp{nid}')
        fill_x = '<a:noFill/>' if (fill is None or no_fill) else f'<a:solidFill><a:srgbClr val="{fill}"/></a:solidFill>'
        line_x = ('<a:ln><a:noFill/></a:ln>' if stroke is None
                  else f'<a:ln w="{sw}"><a:solidFill><a:srgbClr val="{stroke}"/></a:solidFill></a:ln>')
        geom = (f'<a:prstGeom prst="roundRect"><a:avLst><a:gd name="adj" fmla="val {radius}"/></a:avLst></a:prstGeom>'
                if radius > 0 else '<a:prstGeom prst="rect"><a:avLst/></a:prstGeom>')
        tx = ''
        if paras is not None:
            tx = f'<p:txBody><a:bodyPr wrap="square" anchor="{anchor}"><a:normAutofit/></a:bodyPr><a:lstStyle/>{"".join(paras)}</p:txBody>'
        self._shapes.append(
            f'<p:sp><p:nvSpPr><p:cNvPr id="{nid}" name="{nm}"/>'
            f'<p:cNvSpPr><a:spLocks noGrp="1"/></p:cNvSpPr><p:nvPr/></p:nvSpPr>'
            f'<p:spPr><a:xfrm><a:off x="{x}" y="{y}"/><a:ext cx="{w}" cy="{h}"/></a:xfrm>'
            f'{geom}{fill_x}{line_x}</p:spPr>{tx}</p:sp>'
        )
        return self

    def arrow_h(self, x, y, w, h, fill='888888'):
        """Horizontal right-pointing arrow shape."""
        nid = self._nid()
        self._shapes.append(
            f'<p:sp><p:nvSpPr><p:cNvPr id="{nid}" name="Arr{nid}"/>'
            f'<p:cNvSpPr><a:spLocks noGrp="1"/></p:cNvSpPr><p:nvPr/></p:nvSpPr>'
            f'<p:spPr><a:xfrm><a:off x="{x}" y="{y}"/><a:ext cx="{w}" cy="{h}"/></a:xfrm>'
            f'<a:prstGeom prst="rightArrow"><a:avLst>'
            f'<a:gd name="adj1" fmla="val 50000"/><a:gd name="adj2" fmla="val 50000"/>'
            f'</a:avLst></a:prstGeom>'
            f'<a:solidFill><a:srgbClr val="{fill}"/></a:solidFill>'
            f'<a:ln><a:noFill/></a:ln></p:spPr></p:sp>'
        )
        return self

    def arrow_d(self, x, y, w, h, fill='888888'):
        """Downward arrow shape."""
        nid = self._nid()
        self._shapes.append(
            f'<p:sp><p:nvSpPr><p:cNvPr id="{nid}" name="Arr{nid}"/>'
            f'<p:cNvSpPr><a:spLocks noGrp="1"/></p:cNvSpPr><p:nvPr/></p:nvSpPr>'
            f'<p:spPr><a:xfrm><a:off x="{x}" y="{y}"/><a:ext cx="{w}" cy="{h}"/></a:xfrm>'
            f'<a:prstGeom prst="downArrow"><a:avLst>'
            f'<a:gd name="adj1" fmla="val 50000"/><a:gd name="adj2" fmla="val 50000"/>'
            f'</a:avLst></a:prstGeom>'
            f'<a:solidFill><a:srgbClr val="{fill}"/></a:solidFill>'
            f'<a:ln><a:noFill/></a:ln></p:spPr></p:sp>'
        )
        return self

    def xml(self):
        return ''.join(self._shapes)


def p(text, sz=1200, bold=False, color='333333', align='ctr', italic=False):
    b = '1' if bold else '0'
    it = '1' if italic else '0'
    return (f'<a:p><a:pPr algn="{align}"/><a:r>'
            f'<a:rPr lang="zh-CN" altLang="en-US" sz="{sz}" b="{b}" i="{it}" dirty="0">'
            f'<a:solidFill><a:srgbClr val="{color}"/></a:solidFill>'
            f'<a:latin typeface="Calibri"/><a:ea typeface="Microsoft YaHei"/>'
            f'</a:rPr><a:t>{xe(text)}</a:t></a:r></a:p>')

def blank_p():
    return '<a:p><a:endParaRPr lang="zh-CN" dirty="0"/></a:p>'


def make_slide(bg, shapes_xml):
    bg_xml = f'<p:bg><p:bgPr><a:solidFill><a:srgbClr val="{bg}"/></a:solidFill><a:effectLst/></p:bgPr></p:bg>'
    return (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
        '<p:sld xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"\n'
        '       xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"\n'
        '       xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">\n'
        '  <p:cSld>\n'
        f'    {bg_xml}\n'
        '    <p:spTree>\n'
        '      <p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>\n'
        '      <p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/>'
        '<a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>\n'
        f'      {shapes_xml}\n'
        '    </p:spTree>\n'
        '  </p:cSld>\n'
        '</p:sld>'
    )


def save_pptx(path, slide_xml):
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml',                   CONTENT_TYPES)
        z.writestr('_rels/.rels',                           ROOT_RELS)
        z.writestr('ppt/presentation.xml',                  PRESENTATION)
        z.writestr('ppt/_rels/presentation.xml.rels',       PRES_RELS)
        z.writestr('ppt/slides/slide1.xml',                 slide_xml)
        z.writestr('ppt/slides/_rels/slide1.xml.rels',      SLIDE_RELS)
        z.writestr('ppt/slideLayouts/slideLayout1.xml',     SLIDE_LAYOUT)
        z.writestr('ppt/slideLayouts/_rels/slideLayout1.xml.rels', LAYOUT_RELS)
        z.writestr('ppt/slideMasters/slideMaster1.xml',     SLIDE_MASTER)
        z.writestr('ppt/slideMasters/_rels/slideMaster1.xml.rels', MASTER_RELS)
        z.writestr('ppt/theme/theme1.xml',                  THEME)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(buf.getvalue())
    print(f'  Saved: {path}')


# ─────────────── common layout constants ───────────────
# Header
HDR_Y, HDR_H = 0, 580000
# Footer
FTR_Y, FTR_H = 6500000, 358000
# Content area
CTY = 630000
CTH = FTR_Y - CTY   # 5870000

# Three columns
C1X, C1W = 150000, 3500000
C2X, C2W = 3900000, 4200000
C3X, C3W = 8350000, 3700000

# Column title row
COL_TH = 220000      # col header height
COL_TY = CTY + 50000

# Caller column sub-sections
CALLER_ICON_Y  = COL_TY + COL_TH + 100000
CALLER_ICON_H  = 900000
CALLER_CHIPS_Y = CALLER_ICON_Y + CALLER_ICON_H + 100000

# VM column sub-sections
VM_LABEL_Y   = COL_TY + COL_TH + 60000
VM_LABEL_H   = 260000
VM_GRID_Y    = VM_LABEL_Y + VM_LABEL_H + 60000
# 2 rows of 3 VMs each
VM_ROW1_Y    = VM_GRID_Y
VM_ROW2_Y    = VM_GRID_Y + 820000
VM_H         = 740000
# VM x positions within col2 (absolute)
VX = [C2X + 100000 + i*(1280000 + 80000) for i in range(3)]   # 3 per row
VW = 1280000
# Superscale ratio bar at bottom of col2
SCALE_Y = VM_ROW2_Y + VM_H + 180000
SCALE_H = 480000

# Memory column sub-sections
L1Y, L1H = COL_TY + COL_TH + 80000, 1400000
L2Y, L2H = L1Y + L1H + 200000, 1300000
L3Y, L3H = L2Y + L2H + 200000, 1600000

# Down-arrow between memory layers
DA_X = C3X + C3W//2 - 150000
DA_W = 300000
DA_H = 180000


# ──────────────────────── request-type chips ─────────────────────
CHIPS = ['网页', '图片', '脚本', '数据', '媒体', 'API']

def add_chips(s, chip_fill, chip_text_color, chip_border):
    cx = C1X + 80000
    cw = (C1W - 160000 - 60000) // 3
    ch = 260000
    gap_x = (C1W - 160000 - 3*cw) // 2
    for idx, label in enumerate(CHIPS):
        row, col = divmod(idx, 3)
        cx2 = C1X + 80000 + col * (cw + gap_x)
        cy2 = CALLER_CHIPS_Y + row * (ch + 80000)
        s.rect(cx2, cy2, cw, ch, fill=chip_fill, stroke=chip_border, sw=9525,
               radius=10000,
               paras=[p(label, sz=900, color=chip_text_color, bold=True)],
               anchor='ctr')


# ─────────────────────── VM grid ────────────────────────
def add_vm_grid(s, vm_fill, vm_border, vm_star_fill, vm_star_border, vm_text, vm_text2):
    # Row 1: VM-01 to VM-03 (existing)
    for i in range(3):
        s.rect(VX[i], VM_ROW1_Y, VW, VM_H,
               fill=vm_fill, stroke=vm_border, sw=15875,
               paras=[p('openclaw', sz=900, color=vm_text, bold=True),
                      p(f'VM-0{i+1}', sz=800, color=vm_text)])
    # Row 2: VM-04 to VM-06 (SMAP superscale new)
    for i in range(3):
        s.rect(VX[i], VM_ROW2_Y, VW, VM_H,
               fill=vm_star_fill, stroke=vm_star_border, sw=15875,
               paras=[p('openclaw ★', sz=900, color=vm_text2, bold=True),
                      p(f'VM-0{i+4}', sz=800, color=vm_text2)])


# ──────────────────── memory layer boxes ────────────────────────
def add_memory_layers(s, l1_fill, l1_stroke, l2_fill, l2_stroke,
                      l3_fill, l3_stroke, arrow_fill,
                      l1_t, l1_s, l2_t, l2_s, l3_t, l3_s,
                      text_dark, text_light):
    # L1 DRAM
    s.rect(C3X + 80000, L1Y, C3W - 160000, L1H, fill=l1_fill, stroke=l1_stroke, sw=19050,
           paras=[p('L1 · DRAM', sz=1100, color=l1_t, bold=True),
                  blank_p(),
                  p('本地内存', sz=900, color=l1_s),
                  p('延迟 ~100 ns', sz=800, color=l1_s, italic=True)],
           anchor='ctr')
    # Arrow down L1→L2
    s.arrow_d(DA_X, L1Y + L1H, DA_W, DA_H, fill=arrow_fill)
    # L2 UB Borrowed
    s.rect(C3X + 80000, L2Y, C3W - 160000, L2H, fill=l2_fill, stroke=l2_stroke, sw=19050,
           paras=[p('L2 · UB 借用内存', sz=1100, color=l2_t, bold=True),
                  blank_p(),
                  p('跨节点内存借用', sz=900, color=l2_s),
                  p('延迟 ~1 μs', sz=800, color=l2_s, italic=True)],
           anchor='ctr')
    # Arrow down L2→L3
    s.arrow_d(DA_X, L2Y + L2H, DA_W, DA_H, fill=arrow_fill)
    # L3 NVMe
    s.rect(C3X + 80000, L3Y, C3W - 160000, L3H, fill=l3_fill, stroke=l3_stroke, sw=19050,
           paras=[p('★ L3 · NVMe SSD', sz=1100, color=l3_t, bold=True),
                  blank_p(),
                  p('核心超分扩展', sz=900, color=l3_s),
                  p('延迟 ~100 μs', sz=800, color=l3_s, italic=True),
                  blank_p(),
                  p('超分比 1.0× → 1.8×', sz=900, color=l3_t, bold=True)],
           anchor='ctr')


# ──────────────────────── superscale bar ────────────────────────
def add_scale_bar(s, bar_before, bar_after, label_color):
    # Label
    s.rect(C2X, SCALE_Y, C2W, 160000, fill=None,
           paras=[p('虚机承载超分比对比', sz=800, color=label_color, bold=True)],
           anchor='ctr')
    bar_y = SCALE_Y + 160000
    bar_h = 150000
    total_w = C2W - 200000
    # Before bar (gray)
    s.rect(C2X + 100000, bar_y, int(total_w * 0.55), bar_h, fill='AAAAAA')
    s.rect(C2X + 100000, bar_y, total_w, bar_h, fill=None, stroke='CCCCCC', sw=6350)
    s.rect(C2X + 100000, bar_y, int(total_w * 0.55), bar_h, fill=bar_before,
           paras=[p('无 SMAP  1.0×', sz=700, color='FFFFFF', bold=True)], anchor='ctr')
    # After bar (colored)
    bar_y2 = bar_y + bar_h + 60000
    s.rect(C2X + 100000, bar_y2, total_w, bar_h, fill=None, stroke='CCCCCC', sw=6350)
    s.rect(C2X + 100000, bar_y2, total_w, bar_h, fill=bar_after,
           paras=[p('SMAP L2+L3  ↑1.8×', sz=700, color='FFFFFF', bold=True)], anchor='ctr')


# ═══════════════════════════════════════════════════════════════
#  Style generators
# ═══════════════════════════════════════════════════════════════

def style_3col(
    style_name,
    # backgrounds
    bg, hdr_fill, hdr_txt, ftr_fill, ftr_txt,
    # column headers
    col_hdr_fill, col_hdr_txt,
    # caller area
    caller_bg, chip_fill, chip_txt, chip_border,
    # vm area
    vm_bg, vm_fill, vm_border, vm_star_fill, vm_star_border,
    vm_txt, vm_txt2, vm_label_fill, vm_label_txt,
    # memory area
    mem_bg,
    l1f, l1s, l1t, l1sub,
    l2f, l2s, l2t, l2sub,
    l3f, l3s, l3t, l3sub,
    arrow_fill,
    # scale bar
    bar_before, bar_after, bar_label,
    # horizontal arrows
    harrow_fill,
    # optional radius
    card_radius=0,
):
    s = S()

    # ── header bar ──
    s.rect(0, HDR_Y, SW, HDR_H, fill=hdr_fill,
           paras=[p('openclaw 业务场景 — SMAP 内存分层超分方案', sz=2000, color=hdr_txt, bold=True),
                  p('字节火山 AI 服务平台 · openEuler OLK-6.6', sz=1000, color=hdr_txt)],
           anchor='ctr')

    # ── footer bar ──
    s.rect(0, FTR_Y, SW, FTR_H, fill=ftr_fill,
           paras=[p('火山引擎 · SMAP 方案 · L1 DRAM + L2 UB借用内存 + L3 NVMe SSD', sz=800, color=ftr_txt)],
           anchor='ctr')

    # ── column backgrounds ──
    s.rect(C1X, CTY, C1W, CTH, fill=caller_bg, stroke=col_hdr_fill if card_radius==0 else None, sw=9525, radius=card_radius)
    s.rect(C2X, CTY, C2W, CTH, fill=vm_bg, stroke=col_hdr_fill if card_radius==0 else None, sw=9525, radius=card_radius)
    s.rect(C3X, CTY, C3W, CTH, fill=mem_bg, stroke=col_hdr_fill if card_radius==0 else None, sw=9525, radius=card_radius)

    # ── column title bands ──
    s.rect(C1X, COL_TY, C1W, COL_TH, fill=col_hdr_fill, radius=0,
           paras=[p('调 用 端', sz=1100, color=col_hdr_txt, bold=True)], anchor='ctr')
    s.rect(C2X, COL_TY, C2W, COL_TH, fill=col_hdr_fill, radius=0,
           paras=[p('VM 虚机层 · openclaw 执行环境', sz=1100, color=col_hdr_txt, bold=True)], anchor='ctr')
    s.rect(C3X, COL_TY, C3W, COL_TH, fill=col_hdr_fill, radius=0,
           paras=[p('SMAP 三级内存', sz=1100, color=col_hdr_txt, bold=True)], anchor='ctr')

    # ── caller area: icon box ──
    icon_cx = C1X + C1W//2 - 600000
    s.rect(icon_cx, CALLER_ICON_Y, 1200000, CALLER_ICON_H, fill=chip_fill, stroke=chip_border, sw=19050,
           paras=[p('📞', sz=2800, color=chip_txt),
                  p('调用端', sz=1000, color=chip_txt, bold=True)],
           anchor='ctr')
    # request type chips
    add_chips(s, chip_fill, chip_txt, chip_border)

    # ── VM label ──
    s.rect(C2X + 100000, VM_LABEL_Y, C2W - 200000, VM_LABEL_H,
           fill=vm_label_fill, stroke=None,
           paras=[p('▶ openclaw browser 任务执行', sz=1000, color=vm_label_txt, bold=True)],
           anchor='ctr')

    # ── VM grid ──
    add_vm_grid(s, vm_fill, vm_border, vm_star_fill, vm_star_border, vm_txt, vm_txt2)

    # ── superscale bar ──
    add_scale_bar(s, bar_before, bar_after, bar_label)

    # ── memory layers ──
    add_memory_layers(s, l1f, l1s, l2f, l2s, l3f, l3s, arrow_fill,
                      l1t, l1sub, l2t, l2sub, l3t, l3sub, '333333', 'FFFFFF')

    # ── horizontal arrows (caller→vm, vm→memory) ──
    arr_y = COL_TY + COL_TH + CTH//2 - 350000
    arr_h = 300000
    # col1 → col2
    s.arrow_h(C1X + C1W + 30000, arr_y, C2X - C1X - C1W - 60000, arr_h, fill=harrow_fill)
    # col2 → col3
    s.arrow_h(C2X + C2W + 30000, arr_y, C3X - C2X - C2W - 60000, arr_h, fill=harrow_fill)

    return make_slide(bg, s.xml())


# ════════════════════════════════════════════════════════════════
#  10 Style definitions
# ════════════════════════════════════════════════════════════════

def gen_s01():
    """S01 · 简约极简 — white/navy thin borders"""
    return style_3col(
        's01', bg='FFFFFF',
        hdr_fill='1A365D', hdr_txt='FFFFFF',
        ftr_fill='1A365D', ftr_txt='AACCEE',
        col_hdr_fill='2C5282', col_hdr_txt='FFFFFF',
        caller_bg='F0F4FF', chip_fill='E8EFF8', chip_txt='2C5282', chip_border='7BAFD4',
        vm_bg='FFFBF5', vm_fill='FFF0DC', vm_border='D97706',
        vm_star_fill='E8F5E9', vm_star_border='2E7D32',
        vm_txt='92400E', vm_txt2='1B5E20',
        vm_label_fill='FFF3CD', vm_label_txt='92400E',
        mem_bg='F0FAF5',
        l1f='DBEAFE', l1s='3B82F6', l1t='1E40AF', l1sub='3B82F6',
        l2f='D1FAE5', l2s='10B981', l2t='065F46', l2sub='10B981',
        l3f='FFF7ED', l3s='D97706', l3t='92400E', l3sub='D97706',
        arrow_fill='94A3B8',
        bar_before='9CA3AF', bar_after='D97706', bar_label='374151',
        harrow_fill='94A3B8',
    )


def gen_s02():
    """S02 · 企业深蓝 — corporate dark blue"""
    return style_3col(
        's02', bg='F2F4F8',
        hdr_fill='1B2D5B', hdr_txt='FFFFFF',
        ftr_fill='1B2D5B', ftr_txt='90A0C0',
        col_hdr_fill='283E7A', col_hdr_txt='FFFFFF',
        caller_bg='FFFFFF', chip_fill='EAF1FB', chip_txt='1B2D5B', chip_border='A0B4D0',
        vm_bg='FFFFFF', vm_fill='EAF1FB', vm_border='283E7A',
        vm_star_fill='FFF0E0', vm_star_border='D4520F',
        vm_txt='1B2D5B', vm_txt2='8B2500',
        vm_label_fill='FFF3E0', vm_label_txt='8B2500',
        mem_bg='FFFFFF',
        l1f='EAF1FB', l1s='283E7A', l1t='1B2D5B', l1sub='283E7A',
        l2f='E8F4EA', l2s='276129', l2t='1A3A1C', l2sub='276129',
        l3f='FFF0E0', l3s='D4520F', l3t='8B2500', l3sub='D4520F',
        arrow_fill='7A8FAF',
        bar_before='7A8FAF', bar_after='D4520F', bar_label='1B2D5B',
        harrow_fill='7A8FAF',
    )


def gen_s03():
    """S03 · Material 卡片 — Google Material Design palette"""
    return style_3col(
        's03', bg='ECEFF1',
        hdr_fill='37474F', hdr_txt='FFFFFF',
        ftr_fill='37474F', ftr_txt='B0BEC5',
        col_hdr_fill='455A64', col_hdr_txt='FFFFFF',
        caller_bg='FFFFFF', chip_fill='E3F2FD', chip_txt='0D47A1', chip_border='90CAF9',
        vm_bg='FFFFFF', vm_fill='E3F2FD', vm_border='1565C0',
        vm_star_fill='F3E5F5', vm_star_border='6A1B9A',
        vm_txt='0D47A1', vm_txt2='4A148C',
        vm_label_fill='FFF8E1', vm_label_txt='E65100',
        mem_bg='FFFFFF',
        l1f='E3F2FD', l1s='1565C0', l1t='0D47A1', l1sub='1565C0',
        l2f='E8F5E9', l2s='2E7D32', l2t='1B5E20', l2sub='2E7D32',
        l3f='FBE9E7', l3s='BF360C', l3t='B71C1C', l3sub='BF360C',
        arrow_fill='90A4AE',
        bar_before='90A4AE', bar_after='BF360C', bar_label='37474F',
        harrow_fill='90A4AE',
    )


def gen_s04():
    """S04 · 冷蓝科技 — cool tech blue"""
    return style_3col(
        's04', bg='EBF4FA',
        hdr_fill='154360', hdr_txt='FFFFFF',
        ftr_fill='154360', ftr_txt='7FB3CE',
        col_hdr_fill='1A6080', col_hdr_txt='FFFFFF',
        caller_bg='D6EAF8', chip_fill='AED6F1', chip_txt='154360', chip_border='5DADE2',
        vm_bg='D6EAF8', vm_fill='AED6F1', vm_border='1A6080',
        vm_star_fill='FFF3CD', vm_star_border='D4AC0D',
        vm_txt='154360', vm_txt2='7D6608',
        vm_label_fill='FDEBD0', vm_label_txt='784212',
        mem_bg='D6EAF8',
        l1f='AED6F1', l1s='1A6080', l1t='154360', l1sub='1A6080',
        l2f='A9DFBF', l2s='1E8449', l2t='145A32', l2sub='1E8449',
        l3f='F8C471', l3s='D4AC0D', l3t='7D6608', l3sub='D4AC0D',
        arrow_fill='5DADE2',
        bar_before='5DADE2', bar_after='D4AC0D', bar_label='154360',
        harrow_fill='5DADE2',
    )


def gen_s05():
    """S05 · 暖橙商务 — warm orange business"""
    return style_3col(
        's05', bg='FFFBF8',
        hdr_fill='7B341E', hdr_txt='FFFFFF',
        ftr_fill='7B341E', ftr_txt='F4A575',
        col_hdr_fill='C05621', col_hdr_txt='FFFFFF',
        caller_bg='FFF3EB', chip_fill='FDDCB5', chip_txt='7B341E', chip_border='F6AD55',
        vm_bg='FFF3EB', vm_fill='FDDCB5', vm_border='C05621',
        vm_star_fill='E9F5E9', vm_star_border='2D6A4F',
        vm_txt='7B341E', vm_txt2='1B4332',
        vm_label_fill='FEF3C7', vm_label_txt='92400E',
        mem_bg='FFF3EB',
        l1f='FDDCB5', l1s='C05621', l1t='7B341E', l1sub='C05621',
        l2f='D8F3DC', l2s='2D6A4F', l2t='1B4332', l2sub='2D6A4F',
        l3f='E0E7FF', l3s='3730A3', l3t='1E1B4B', l3sub='3730A3',
        arrow_fill='F6AD55',
        bar_before='C6C6C6', bar_after='C05621', bar_label='7B341E',
        harrow_fill='F6AD55',
    )


def gen_s06():
    """S06 · 森绿专业 — forest green professional"""
    return style_3col(
        's06', bg='F2F7F2',
        hdr_fill='1B4332', hdr_txt='FFFFFF',
        ftr_fill='1B4332', ftr_txt='74C69D',
        col_hdr_fill='2D6A4F', col_hdr_txt='FFFFFF',
        caller_bg='D8F3DC', chip_fill='B7E4C7', chip_txt='1B4332', chip_border='52B788',
        vm_bg='D8F3DC', vm_fill='B7E4C7', vm_border='2D6A4F',
        vm_star_fill='FFF3E0', vm_star_border='E65100',
        vm_txt='1B4332', vm_txt2='BF360C',
        vm_label_fill='FFF9C4', vm_label_txt='F57F17',
        mem_bg='D8F3DC',
        l1f='B7E4C7', l1s='2D6A4F', l1t='1B4332', l1sub='2D6A4F',
        l2f='D1ECF1', l2s='0C5460', l2t='0A3D42', l2sub='0C5460',
        l3f='FFE0B2', l3s='E65100', l3t='BF360C', l3sub='E65100',
        arrow_fill='52B788',
        bar_before='A0C4A5', bar_after='E65100', bar_label='1B4332',
        harrow_fill='52B788',
    )


def gen_s07():
    """S07 · 紫灰高端 — slate purple premium"""
    return style_3col(
        's07', bg='F5F3FF',
        hdr_fill='3B1D8A', hdr_txt='FFFFFF',
        ftr_fill='3B1D8A', ftr_txt='A78BFA',
        col_hdr_fill='5B21B6', col_hdr_txt='FFFFFF',
        caller_bg='EDE9FE', chip_fill='DDD6FE', chip_txt='3B1D8A', chip_border='8B5CF6',
        vm_bg='EDE9FE', vm_fill='DDD6FE', vm_border='5B21B6',
        vm_star_fill='FFF0F0', vm_star_border='BE185D',
        vm_txt='3B1D8A', vm_txt2='9D174D',
        vm_label_fill='FCE7F3', vm_label_txt='9D174D',
        mem_bg='EDE9FE',
        l1f='DDD6FE', l1s='5B21B6', l1t='3B1D8A', l1sub='5B21B6',
        l2f='CFFAFE', l2s='0E7490', l2t='164E63', l2sub='0E7490',
        l3f='FFE4E6', l3s='BE185D', l3t='9D174D', l3sub='BE185D',
        arrow_fill='8B5CF6',
        bar_before='A78BFA', bar_after='BE185D', bar_label='3B1D8A',
        harrow_fill='8B5CF6',
    )


def gen_s08():
    """S08 · 钢蓝沉稳 — steel blue steady"""
    return style_3col(
        's08', bg='F0F4F8',
        hdr_fill='2C4A6E', hdr_txt='FFFFFF',
        ftr_fill='2C4A6E', ftr_txt='90AECE',
        col_hdr_fill='3D6089', col_hdr_txt='FFFFFF',
        caller_bg='FFFFFF', chip_fill='DBE8F5', chip_txt='2C4A6E', chip_border='6E9FC0',
        vm_bg='FFFFFF', vm_fill='DBE8F5', vm_border='3D6089',
        vm_star_fill='FFF4EC', vm_star_border='CC5500',
        vm_txt='2C4A6E', vm_txt2='8A3500',
        vm_label_fill='FFF4EC', vm_label_txt='8A3500',
        mem_bg='FFFFFF',
        l1f='DBE8F5', l1s='3D6089', l1t='2C4A6E', l1sub='3D6089',
        l2f='DBF0E8', l2s='2A7356', l2t='1A4D38', l2sub='2A7356',
        l3f='FFEEDD', l3s='CC5500', l3t='8A3500', l3sub='CC5500',
        arrow_fill='6E9FC0',
        bar_before='6E9FC0', bar_after='CC5500', bar_label='2C4A6E',
        harrow_fill='6E9FC0',
    )


def gen_s09():
    """S09 · 柔和圆角 — soft rounded cards"""
    return style_3col(
        's09', bg='F8FAFC',
        hdr_fill='2D4059', hdr_txt='FFFFFF',
        ftr_fill='2D4059', ftr_txt='90A8BE',
        col_hdr_fill='EA6A47', col_hdr_txt='FFFFFF',
        caller_bg='FFFFFF', chip_fill='FFF0E8', chip_txt='EA6A47', chip_border='FFAC8E',
        vm_bg='FFFFFF', vm_fill='FFF0E8', vm_border='EA6A47',
        vm_star_fill='E8F4FD', vm_star_border='2980B9',
        vm_txt='C0392B', vm_txt2='1A5276',
        vm_label_fill='FEF9EF', vm_label_txt='784212',
        mem_bg='FFFFFF',
        l1f='E8F4FD', l1s='2980B9', l1t='1A5276', l1sub='2980B9',
        l2f='EAFAF1', l2s='27AE60', l2t='196F3D', l2sub='27AE60',
        l3f='FEF9E7', l3s='F39C12', l3t='9A7D0A', l3sub='F39C12',
        arrow_fill='EA6A47',
        bar_before='BDC3C7', bar_after='EA6A47', bar_label='2D4059',
        harrow_fill='EA6A47',
        card_radius=20000,
    )


def gen_s10():
    """S10 · 单色高级 — monochrome except openclaw orange"""
    return style_3col(
        's10', bg='FAFAFA',
        hdr_fill='1C1C1C', hdr_txt='FFFFFF',
        ftr_fill='1C1C1C', ftr_txt='888888',
        col_hdr_fill='333333', col_hdr_txt='FFFFFF',
        caller_bg='F0F0F0', chip_fill='E0E0E0', chip_txt='333333', chip_border='AAAAAA',
        vm_bg='F0F0F0', vm_fill='E0E0E0', vm_border='555555',
        vm_star_fill='FFF4E8', vm_star_border='EA580C',
        vm_txt='333333', vm_txt2='C45108',
        vm_label_fill='FFF4E8', vm_label_txt='C45108',
        mem_bg='F0F0F0',
        l1f='E0E0E0', l1s='555555', l1t='1C1C1C', l1sub='555555',
        l2f='D8D8D8', l2s='444444', l2t='1C1C1C', l2sub='444444',
        l3f='FFF4E8', l3s='EA580C', l3t='C45108', l3sub='EA580C',
        arrow_fill='888888',
        bar_before='888888', bar_after='EA580C', bar_label='1C1C1C',
        harrow_fill='888888',
    )


# ──────────────────────── main ────────────────────────
def main():
    styles = [
        ('ppt_v3_s01_minimalist',   gen_s01),
        ('ppt_v3_s02_corporate',    gen_s02),
        ('ppt_v3_s03_material',     gen_s03),
        ('ppt_v3_s04_coolblue',     gen_s04),
        ('ppt_v3_s05_warmorange',   gen_s05),
        ('ppt_v3_s06_forest',       gen_s06),
        ('ppt_v3_s07_purple',       gen_s07),
        ('ppt_v3_s08_steelblue',    gen_s08),
        ('ppt_v3_s09_softround',    gen_s09),
        ('ppt_v3_s10_monochrome',   gen_s10),
    ]
    out = 'diagrams'
    os.makedirs(out, exist_ok=True)
    print(f'Generating {len(styles)} PPTX files...')
    for name, fn in styles:
        slide_xml = fn()
        save_pptx(f'{out}/{name}.pptx', slide_xml)
    print('Done.')

if __name__ == '__main__':
    main()
