import sys
sys.path.insert(0, r"E:\Lib\site-packages")

from docx import Document
from docx.oxml.ns import qn

doc = Document(r"e:\SpectrumFall\课程设计报告.docx")

print("=" * 70)
print("Document structure overview")
print("=" * 70)

# Iterate through paragraphs and tables in order
body = doc.element.body
para_idx = 0
table_idx = 0

for child in body.iterchildren():
    tag = child.tag.split('}')[-1]
    if tag == 'p':
        # paragraph
        from docx.text.paragraph import Paragraph
        p = Paragraph(child, doc)
        text = p.text.strip()
        style = p.style.name if p.style else "None"
        if text:
            # Truncate long text
            display = text[:80] + ("..." if len(text) > 80 else "")
            print(f"[P#{para_idx:03d}] ({style}) {display}")
        para_idx += 1
    elif tag == 'tbl':
        from docx.table import Table
        t = Table(child, doc)
        rows = len(t.rows)
        cols = len(t.columns)
        # Get first cell text as preview
        try:
            preview = t.cell(0, 0).text.strip()[:40]
        except:
            preview = ""
        print(f"[T#{table_idx:03d}] TABLE {rows}x{cols}  first_cell='{preview}'")
        table_idx += 1

print()
print(f"Total paragraphs: {para_idx}")
print(f"Total tables: {table_idx}")
