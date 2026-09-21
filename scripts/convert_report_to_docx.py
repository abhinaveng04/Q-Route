#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import argparse
import os
import re
import sys

# 1. Dependency Management
try:
    import docx
    from docx.shared import Inches, Pt, RGBColor
    from docx.enum.text import WD_ALIGN_PARAGRAPH
    from docx.enum.table import WD_TABLE_ALIGNMENT, WD_ALIGN_VERTICAL
    from docx.oxml import parse_xml, OxmlElement
    from docx.oxml.ns import nsdecls, qn
except ImportError:
    import subprocess
    print("python-docx not found. Attempting automatic installation...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "python-docx"])
    import docx
    from docx.shared import Inches, Pt, RGBColor
    from docx.enum.text import WD_ALIGN_PARAGRAPH
    from docx.enum.table import WD_TABLE_ALIGNMENT, WD_ALIGN_VERTICAL
    from docx.oxml import parse_xml, OxmlElement
    from docx.oxml.ns import nsdecls, qn

# Colors
COLOR_PRIMARY_NAVY   = RGBColor(0x00, 0x33, 0x66)  # #003366
COLOR_SECONDARY_NAVY = RGBColor(0x1A, 0x36, 0x5D)  # #1A365D
COLOR_HEADING_2      = RGBColor(0x2D, 0x37, 0x48)  # #2D3748
COLOR_HEADING_3      = RGBColor(0x4A, 0x55, 0x68)  # #4A5568
COLOR_BODY_TEXT      = RGBColor(0x2D, 0x37, 0x48)  # #2D3748
COLOR_MUTED          = RGBColor(0x71, 0x80, 0x96)  # #718096
COLOR_WHITE          = RGBColor(0xFF, 0xFF, 0xFF)  # #FFFFFF
COLOR_CODE_TEXT      = RGBColor(0x1E, 0x29, 0x3B)  # #1E293B

HEX_NAVY_HEADER      = "1A365D"
HEX_ROW_ALT          = "F7FAFC"
HEX_ROW_WHITE        = "FFFFFF"
HEX_BORDER           = "CCCCCC"
HEX_CODE_BG          = "F8FAFC"
HEX_CALLOUT_BAR      = "1A365D"

def set_cell_background(cell, hex_color):
    tcPr = cell._tc.get_or_add_tcPr()
    shd = parse_xml(f'<w:shd {nsdecls("w")} w:fill="{hex_color}"/>')
    tcPr.append(shd)

def set_cell_margins(cell, top=120, bottom=120, left=160, right=160):
    tcPr = cell._tc.get_or_add_tcPr()
    tcMar = parse_xml(f'<w:tcMar {nsdecls("w")}><w:top w:w="{top}" w:type="dxa"/><w:bottom w:w="{bottom}" w:type="dxa"/><w:left w:w="{left}" w:type="dxa"/><w:right w:w="{right}" w:type="dxa"/></w:tcMar>')
    tcPr.append(tcMar)

def set_table_borders(table, border_color=HEX_BORDER):
    tblPr = table._tbl.tblPr
    borders = parse_xml(f'''
        <w:tblBorders {nsdecls("w")}>
            <w:top w:val="single" w:sz="6" w:space="0" w:color="{border_color}"/>
            <w:bottom w:val="single" w:sz="6" w:space="0" w:color="{border_color}"/>
            <w:left w:val="single" w:sz="4" w:space="0" w:color="{border_color}"/>
            <w:right w:val="single" w:sz="4" w:space="0" w:color="{border_color}"/>
            <w:insideH w:val="single" w:sz="4" w:space="0" w:color="{border_color}"/>
            <w:insideV w:val="single" w:sz="4" w:space="0" w:color="{border_color}"/>
        </w:tblBorders>
    ''')
    tblPr.append(borders)

def clean_math_symbols(text):
    text = text.replace('\\le', '\u2264').replace('\\ge', '\u2265').replace('\\approx', '\u2248')
    text = text.replace('\\log_2', 'log\u2082').replace('\\theta', '\u03b8').replace('\\phi', '\u03d5')
    text = text.replace('\\psi', '\u03c8').replace('\\lvert', '|').replace('\\rangle', '\u27e9')
    text = text.replace('\\cdot', ' \u00b7 ').replace('\\times', ' \u00d7 ').replace('\\equiv', ' \u2261 ')
    text = text.replace('\\implies', ' \u21d2 ').replace('\\quad', '   ')
    text = text.replace('\\boxed{', '').replace('\\text{', '')
    text = text.replace('\\frac{', '(').replace('}{', ' / ')
    text = text.replace('\\sqrt{5}', '\u221a5').replace('\\', '').replace('}', '')
    return text

def add_formatted_runs(paragraph, text, default_size=10.5, default_color=COLOR_BODY_TEXT, font_family="Calibri"):
    tokens = re.split(r'(`[^`]+`|\*\*[^*]+\*\*|\$[^\$]+\$|\[[^\]]+\]\([^\)]+\))', text)

    for tok in tokens:
        if not tok:
            continue
        if tok.startswith('`') and tok.endswith('`') and len(tok) >= 2:
            code_content = tok[1:-1]
            run = paragraph.add_run(code_content)
            run.font.name = "Consolas"
            run.font.size = Pt(default_size - 1.0)
            run.font.color.rgb = RGBColor(0x80, 0x1F, 0x38)
        elif tok.startswith('**') and tok.endswith('**') and len(tok) >= 4:
            bold_content = tok[2:-2]
            run = paragraph.add_run(bold_content)
            run.font.name = font_family
            run.font.size = Pt(default_size)
            run.font.bold = True
            run.font.color.rgb = default_color
        elif tok.startswith('$') and tok.endswith('$') and len(tok) >= 2:
            math_content = clean_math_symbols(tok[1:-1])
            run = paragraph.add_run(math_content)
            run.font.name = "Cambria Math"
            run.font.size = Pt(default_size)
            run.font.italic = True
            run.font.color.rgb = RGBColor(0x1A, 0x36, 0x5D)
        elif tok.startswith('[') and '](' in tok and tok.endswith(')'):
            link_match = re.match(r'\[([^\]]+)\]\(([^\)]+)\)', tok)
            if link_match:
                label = link_match.group(1)
                run = paragraph.add_run(label)
                run.font.name = font_family
                run.font.size = Pt(default_size)
                run.font.color.rgb = RGBColor(0x00, 0x56, 0xB3)
                run.font.underline = True
            else:
                run = paragraph.add_run(tok)
                run.font.name = font_family
                run.font.size = Pt(default_size)
                run.font.color.rgb = default_color
        else:
            run = paragraph.add_run(tok)
            run.font.name = font_family
            run.font.size = Pt(default_size)
            run.font.color.rgb = default_color

def format_paragraph_spacing(p, space_before=0, space_after=4, line_spacing=1.15):
    p.paragraph_format.space_before = Pt(space_before)
    p.paragraph_format.space_after = Pt(space_after)
    p.paragraph_format.line_spacing = line_spacing

def add_callout_box(doc, text):
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Inches(0.3)
    p.paragraph_format.right_indent = Inches(0.3)
    format_paragraph_spacing(p, space_before=4, space_after=6, line_spacing=1.15)
    pPr = p._p.get_or_add_pPr()
    pBdr = parse_xml(f'''
        <w:pBdr {nsdecls("w")}>
            <w:left w:val="single" w:sz="24" w:space="12" w:color="{HEX_CALLOUT_BAR}"/>
        </w:pBdr>
    ''')
    pPr.append(pBdr)
    shd = parse_xml(f'<w:shd {nsdecls("w")} w:fill="F7FAFC"/>')
    pPr.append(shd)
    add_formatted_runs(p, text, default_size=10.0, default_color=COLOR_BODY_TEXT)

def add_code_block(doc, code_lines):
    code_text = "\n".join(code_lines)
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Inches(0.2)
    p.paragraph_format.right_indent = Inches(0.2)
    format_paragraph_spacing(p, space_before=4, space_after=6, line_spacing=1.0)
    pPr = p._p.get_or_add_pPr()
    pBdr = parse_xml(f'''
        <w:pBdr {nsdecls("w")}>
            <w:top w:val="single" w:sz="4" w:space="4" w:color="CBD5E0"/>
            <w:bottom w:val="single" w:sz="4" w:space="4" w:color="CBD5E0"/>
            <w:left w:val="single" w:sz="18" w:space="8" w:color="003366"/>
            <w:right w:val="single" w:sz="4" w:space="4" w:color="CBD5E0"/>
        </w:pBdr>
    ''')
    pPr.append(pBdr)
    shd = parse_xml(f'<w:shd {nsdecls("w")} w:fill="{HEX_CODE_BG}"/>')
    pPr.append(shd)

    run = p.add_run(code_text)
    run.font.name = "Consolas"
    max_len = max(len(l) for l in code_lines) if code_lines else 0
    font_size = 7.5 if max_len > 80 else (8.0 if max_len > 65 else 8.5)
    run.font.size = Pt(font_size)
    run.font.color.rgb = COLOR_CODE_TEXT

def convert_markdown_table_to_docx(doc, table_lines):
    if not table_lines:
        return

    parsed_rows = []
    for line in table_lines:
        if re.match(r'^\s*\|?\s*[-:]+[-| :]*$', line):
            continue
        cells = [c.strip() for c in line.strip().split('|')]
        if cells and cells[0] == '':
            cells = cells[1:]
        if cells and cells[-1] == '':
            cells = cells[:-1]
        if cells:
            parsed_rows.append(cells)

    if not parsed_rows:
        return

    num_cols = max(len(r) for r in parsed_rows)
    num_rows = len(parsed_rows)

    table = doc.add_table(rows=num_rows, cols=num_cols)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    set_table_borders(table, HEX_BORDER)

    for r_idx, row_data in enumerate(parsed_rows):
        is_header = (r_idx == 0)
        row = table.rows[r_idx]
        trPr = row._tr.get_or_add_trPr()
        trPr.append(parse_xml(f'<w:cantSplit {nsdecls("w")}/>'))
        if is_header:
            trPr.append(parse_xml(f'<w:tblHeader {nsdecls("w")}/>'))

        bg_color = HEX_NAVY_HEADER if is_header else (HEX_ROW_ALT if r_idx % 2 == 1 else HEX_ROW_WHITE)

        for c_idx in range(num_cols):
            cell = row.cells[c_idx]
            set_cell_background(cell, bg_color)
            set_cell_margins(cell, top=80, bottom=80, left=120, right=120)
            cell.vertical_alignment = WD_ALIGN_VERTICAL.CENTER

            p = cell.paragraphs[0]
            format_paragraph_spacing(p, space_before=0, space_after=0, line_spacing=1.05)

            cell_text = row_data[c_idx] if c_idx < len(row_data) else ""
            if is_header:
                run = p.add_run(cell_text)
                run.font.name = "Calibri"
                run.font.size = Pt(9.5)
                run.font.bold = True
                run.font.color.rgb = COLOR_WHITE
            else:
                add_formatted_runs(p, cell_text, default_size=9.0, default_color=COLOR_BODY_TEXT)

    doc.add_paragraph().paragraph_format.space_after = Pt(4)

def parse_and_convert(md_path, docx_path):
    with open(md_path, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    doc = docx.Document()

    for section in doc.sections:
        section.page_width = Inches(8.5)
        section.page_height = Inches(11.0)
        section.top_margin = Inches(1.0)
        section.bottom_margin = Inches(1.0)
        section.left_margin = Inches(1.0)
        section.right_margin = Inches(1.0)

    in_code_block = False
    code_block_lines = []
    table_lines = []

    for line_idx, raw_line in enumerate(lines):
        line = raw_line.rstrip('\r\n')

        if line.startswith('```'):
            if in_code_block:
                add_code_block(doc, code_block_lines)
                code_block_lines = []
                in_code_block = False
            else:
                if table_lines:
                    convert_markdown_table_to_docx(doc, table_lines)
                    table_lines = []
                in_code_block = True
            continue

        if in_code_block:
            code_block_lines.append(line)
            continue

        if line.strip().startswith('|') and line.strip().endswith('|'):
            table_lines.append(line)
            continue
        else:
            if table_lines:
                convert_markdown_table_to_docx(doc, table_lines)
                table_lines = []

        if not line.strip():
            continue

        if re.match(r'^\s*---\s*$', line):
            p = doc.add_paragraph()
            format_paragraph_spacing(p, space_before=4, space_after=6)
            pPr = p._p.get_or_add_pPr()
            pBdr = parse_xml(f'<w:pBdr {nsdecls("w")}><w:bottom w:val="single" w:sz="8" w:space="1" w:color="CBD5E0"/></w:pBdr>')
            pPr.append(pBdr)
            continue

        if line.startswith('# '):
            p = doc.add_paragraph()
            format_paragraph_spacing(p, space_before=16, space_after=4, line_spacing=1.1)
            title_text = line[2:].strip()
            run = p.add_run(title_text)
            run.font.name = "Calibri"
            run.font.size = Pt(24)
            run.font.bold = True
            run.font.color.rgb = COLOR_PRIMARY_NAVY
            continue

        if line.startswith('## '):
            p = doc.add_paragraph()
            format_paragraph_spacing(p, space_before=14, space_after=4, line_spacing=1.15)
            h1_text = line[3:].strip()
            run = p.add_run(h1_text)
            run.font.name = "Calibri"
            run.font.size = Pt(16)
            run.font.bold = True
            run.font.color.rgb = COLOR_SECONDARY_NAVY
            continue

        if line.startswith('### '):
            p = doc.add_paragraph()
            format_paragraph_spacing(p, space_before=10, space_after=3, line_spacing=1.15)
            h2_text = line[4:].strip()
            run = p.add_run(h2_text)
            run.font.name = "Calibri"
            run.font.size = Pt(13)
            run.font.bold = True
            run.font.color.rgb = COLOR_HEADING_2
            continue

        if line.startswith('#### '):
            p = doc.add_paragraph()
            format_paragraph_spacing(p, space_before=8, space_after=2, line_spacing=1.15)
            h3_text = line[5:].strip()
            run = p.add_run(h3_text)
            run.font.name = "Calibri"
            run.font.size = Pt(11)
            run.font.bold = True
            run.font.color.rgb = COLOR_HEADING_3
            continue

        if line.startswith('> '):
            quote_text = line[2:].strip()
            add_callout_box(doc, quote_text)
            continue

        bullet_match = re.match(r'^\s*[\-\*]\s+(.*)$', line)
        if bullet_match:
            item_text = bullet_match.group(1)
            p = doc.add_paragraph(style='List Bullet')
            format_paragraph_spacing(p, space_before=1, space_after=2, line_spacing=1.15)
            add_formatted_runs(p, item_text, default_size=10.5, default_color=COLOR_BODY_TEXT)
            continue

        num_match = re.match(r'^\s*(\d+)\.\s+(.*)$', line)
        if num_match:
            item_text = num_match.group(2)
            p = doc.add_paragraph(style='List Number')
            format_paragraph_spacing(p, space_before=1, space_after=2, line_spacing=1.15)
            add_formatted_runs(p, item_text, default_size=10.5, default_color=COLOR_BODY_TEXT)
            continue

        if line.startswith('$$') and line.endswith('$$') and len(line) >= 4:
            eq_text = line[2:-2].strip()
            p = doc.add_paragraph()
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            format_paragraph_spacing(p, space_before=4, space_after=4, line_spacing=1.15)
            clean_eq = clean_math_symbols(eq_text)
            run = p.add_run(clean_eq)
            run.font.name = "Cambria Math"
            run.font.size = Pt(11)
            run.font.italic = True
            run.font.color.rgb = COLOR_SECONDARY_NAVY
            continue

        p = doc.add_paragraph()
        format_paragraph_spacing(p, space_before=0, space_after=4, line_spacing=1.15)
        add_formatted_runs(p, line, default_size=10.5, default_color=COLOR_BODY_TEXT)

    if table_lines:
        convert_markdown_table_to_docx(doc, table_lines)
    if code_block_lines:
        add_code_block(doc, code_block_lines)

    doc.save(docx_path)
    print(f"Successfully generated DOCX report: {docx_path}")

def main():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    default_input = os.path.join(root_dir, "report.md")
    if not os.path.exists(default_input):
        default_input = os.path.join(root_dir, "docs", "report_phase1.md")
    default_output = os.path.join(root_dir, "docs", "Q_Route_Progress_Report_1.docx")

    parser = argparse.ArgumentParser(description="Convert Markdown report to DOCX")
    parser.add_argument("--input", "-i", default=default_input, help="Path to input Markdown file")
    parser.add_argument("--output", "-o", default=default_output, help="Path to output DOCX file")
    args = parser.parse_args()

    md_file = os.path.abspath(args.input)
    docx_file = os.path.abspath(args.output)

    if not os.path.exists(md_file):
        print(f"Error: Markdown report not found at {md_file}")
        sys.exit(1)

    os.makedirs(os.path.dirname(docx_file), exist_ok=True)
    parse_and_convert(md_file, docx_file)

if __name__ == "__main__":
    main()
