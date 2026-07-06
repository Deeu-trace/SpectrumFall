import sys
sys.path.insert(0, r"E:\Lib\site-packages")

from docx import Document
from docx.text.paragraph import Paragraph
from docx.table import Table

doc = Document(r"e:\SpectrumFall\课程设计报告.docx")
body = doc.element.body

# Collect all elements in order
elements = []
for child in body.iterchildren():
    tag = child.tag.split('}')[-1]
    if tag == 'p':
        p = Paragraph(child, doc)
        elements.append(('p', p))
    elif tag == 'tbl':
        t = Table(child, doc)
        elements.append(('t', t))

# Find the innovation table (after "在任务书要求之外" paragraph)
print("=" * 70)
print("Looking for innovation features table")
print("=" * 70)

for i, (typ, el) in enumerate(elements):
    if typ == 'p' and '在任务书要求之外' in el.text:
        print(f"Found at element #{i}: {el.text}")
        # Print next 5 elements
        for j in range(i+1, min(i+6, len(elements))):
            t, e = elements[j]
            if t == 'p':
                print(f"  [{j}] P: {e.text[:100]}")
            else:
                print(f"  [{j}] TABLE {len(e.rows)}x{len(e.columns)}")
                for r_idx, row in enumerate(e.rows):
                    cells = [c.text.strip()[:30] for c in row.cells]
                    print(f"       Row {r_idx}: {cells}")
        break

# Find abstract
print()
print("=" * 70)
print("Abstract (first 20 paragraphs)")
print("=" * 70)
for i, (typ, el) in enumerate(elements[:30]):
    if typ == 'p':
        text = el.text.strip()
        if text:
            print(f"[{i:03d}] {text[:120]}")
