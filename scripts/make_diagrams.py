#!/usr/bin/env python3
"""make_diagrams.py : draws the ER diagram (Chen notation) and the relational schema as SVG, from the schema in sql/schema.sql.

  public/img/er_diagram.svg   public/img/schema.svg   (shown on the Database Model page)
  docs/img/er_diagram.png     docs/img/schema.png     (for the report; made by macOS Quick Look when --png is given)

ER notation used (Chen, as in Elmasri & Navathe / Korth):
  rectangle = strong entity          double rectangle = weak entity
  diamond   = relationship           double diamond   = identifying relationship (owner -> weak entity)
  ellipse   = attribute              dashed ellipse   = derived attribute
  solid underline = key attribute    dashed underline = partial key (discriminator) of a weak entity
  single line = partial participation      double line = total participation (every entity takes part)
  1 / N = cardinality ratio, written next to the entity it belongs to
Participation follows the schema: a NOT NULL foreign key means every row must take part, so that side is total.
"""
import math
import os
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
INK, LINE, FILL_E, FILL_R, FILL_A, RED = '#1f3864', '#1f3864', '#fff2cc', '#e2efda', '#ffffff', '#c00000'
FONT = "'Avenir Next','Helvetica Neue',Arial,sans-serif"


def esc(s):
    return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


def text_w(s, size):
    return len(s) * size * .56


class Svg:
    def __init__(self, w, h):
        self.w, self.h, self.back, self.front = w, h, [], []

    def line(self, x1, y1, x2, y2, double=False, width=2.2, color=LINE, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ''
        if not double:
            self.back.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{color}" stroke-width="{width}"{d}/>')
            return
        L = math.hypot(x2 - x1, y2 - y1) or 1
        ox, oy = -(y2 - y1) / L * 3.6, (x2 - x1) / L * 3.6
        for s in (1, -1):
            self.back.append(f'<line x1="{x1 + s * ox:.1f}" y1="{y1 + s * oy:.1f}" x2="{x2 + s * ox:.1f}" y2="{y2 + s * oy:.1f}" stroke="{color}" stroke-width="{width * .8}"/>')

    def text(self, x, y, s, size=17, weight=500, color='#111', anchor='middle', layer=None):
        (layer if layer is not None else self.front).append(
            f'<text x="{x:.1f}" y="{y:.1f}" font-family="{FONT}" font-size="{size}" font-weight="{weight}" fill="{color}" text-anchor="{anchor}" dominant-baseline="central">{esc(s)}</text>')

    def out(self):
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" viewBox="0 0 {self.w} {self.h}">'
                f'<rect width="{self.w}" height="{self.h}" fill="#ffffff"/>' + ''.join(self.back) + ''.join(self.front) + '</svg>')


# ======================================================================= ER diagram
EW, EH = 214, 62          # entity box
RW, RH = 150, 84          # relationship diamond
AX, AY = 84, 26           # attribute ellipse radii

ENTITIES = {   # name: (x, y, weak)
    'Stock_Transactions': (760, 150, False),
    'Categories': (210, 640, False), 'Products': (760, 640, False), 'Suppliers': (1330, 640, False),
    'Sale_Items': (600, 1130, True), 'Purchase_Items': (930, 1130, True),
    'Customers': (210, 1470, False), 'Sales': (600, 1470, False), 'Purchases': (930, 1470, False),
    'Users': (1520, 1190, False),
}
# attribute: (entity, label, x, y, kind)  kind: '' plain, 'key', 'partial', 'derived'
ATTRS = [
    ('Stock_Transactions', 'txn_id', 1030, 150, 'key'), ('Stock_Transactions', 'txn_type', 520, 245, ''),
    ('Stock_Transactions', 'quantity_change', 610, 50, ''), ('Stock_Transactions', 'txn_date', 490, 140, ''),
    ('Stock_Transactions', 'reference_id', 920, 50, ''), ('Stock_Transactions', 'remarks', 1000, 245, ''),
    ('Categories', 'category_id', 200, 510, 'key'), ('Categories', 'category_name', 100, 775, ''), ('Categories', 'description', 290, 770, ''),
    ('Products', 'product_id', 960, 520, 'key'), ('Products', 'sku', 590, 528, ''), ('Products', 'unit_price', 630, 440, ''),
    ('Products', 'min_stock_level', 390, 515, ''), ('Products', 'expiry_date', 860, 440, ''), ('Products', 'product_name', 1110, 440, ''),
    ('Products', 'stock_quantity', 760, 805, ''), ('Products', 'description', 470, 880, ''), ('Products', 'photo', 1080, 790, ''),
    ('Suppliers', 'supplier_id', 1310, 515, 'key'), ('Suppliers', 'city', 1490, 515, ''), ('Suppliers', 'supplier_name', 1600, 600, ''),
    ('Suppliers', 'contact_person', 1610, 690, ''), ('Suppliers', 'phone', 1570, 780, ''), ('Suppliers', 'email', 1450, 855, ''),
    ('Sale_Items', 'product_id', 380, 1050, 'partial'), ('Sale_Items', 'quantity', 360, 1135, ''), ('Sale_Items', 'unit_price', 400, 1220, ''),
    ('Purchase_Items', 'product_id', 1130, 1040, 'partial'), ('Purchase_Items', 'quantity', 1125, 1125, ''), ('Purchase_Items', 'unit_cost', 1110, 1215, ''),
    ('Customers', 'customer_id', 95, 1340, 'key'), ('Customers', 'city', 240, 1290, ''), ('Customers', 'email', 385, 1340, ''),
    ('Customers', 'customer_name', 100, 1595, ''), ('Customers', 'phone', 240, 1655, ''), ('Customers', 'registered_on', 375, 1610, ''),
    ('Sales', 'sale_id', 560, 1620, 'key'), ('Sales', 'sale_date', 700, 1700, ''), ('Sales', 'total_amount', 770, 1575, 'derived'),
    ('Purchases', 'purchase_id', 945, 1650, 'key'), ('Purchases', 'purchase_date', 1070, 1600, ''), ('Purchases', 'total_amount', 1090, 1380, 'derived'),
    ('Users', 'user_id', 1520, 1060, 'key'), ('Users', 'username', 1655, 1105, ''), ('Users', 'password_hash', 1400, 1310, ''),
    ('Users', 'role', 1640, 1300, ''), ('Users', 'created_at', 1550, 1360, ''),
]
# relationship: (name, x, y, identifying, (entity A, card A, total A), (entity B, card B, total B))
RELS = [
    ('logs', 760, 395, False, ('Products', '1', False), ('Stock_Transactions', 'N', True)),
    ('classifies', 480, 640, False, ('Categories', '1', False), ('Products', 'N', True)),
    ('supplies', 1050, 640, False, ('Suppliers', '1', False), ('Products', 'N', True)),
    ('sold_in', 600, 920, False, ('Products', '1', False), ('Sale_Items', 'N', True)),
    ('listed_in', 930, 920, False, ('Products', '1', False), ('Purchase_Items', 'N', True)),
    ('includes', 600, 1300, True, ('Sales', '1', False), ('Sale_Items', 'N', True)),
    ('contains', 930, 1300, True, ('Purchases', '1', False), ('Purchase_Items', 'N', True)),
    ('makes', 405, 1470, False, ('Customers', '1', False), ('Sales', 'N', True)),
    ('fulfils', 1240, 1470, False, ('Suppliers', '1', False), ('Purchases', 'N', True)),
]


def box_edge(cx, cy, tx, ty, hw, hh):
    """Point where the line from the box centre towards (tx, ty) leaves the box."""
    dx, dy = tx - cx, ty - cy
    if dx == 0 and dy == 0:
        return cx, cy
    s = min(hw / abs(dx) if dx else 1e9, hh / abs(dy) if dy else 1e9)
    return cx + dx * s, cy + dy * s


def er(svg_path):
    s = Svg(1760, 2090)
    # attribute lines
    for ent, label, x, y, kind in ATTRS:
        ex, ey, _ = ENTITIES[ent]
        s.line(ex, ey, x, y, width=1.4, color='#5a6c8f')
    # relationship lines + cardinality
    for name, rx, ry, ident, *ends in RELS:
        for ent, card, total in ends:
            ex, ey, _ = ENTITIES[ent]
            s.line(rx, ry, ex, ey, double=total, width=2.4)
            px, py = box_edge(ex, ey, rx, ry, EW / 2 + 4, EH / 2 + 4)
            L = math.hypot(rx - ex, ry - ey) or 1
            ux, uy = (rx - ex) / L, (ry - ey) / L
            qx, qy = px + ux * 20 - uy * 16, py + uy * 20 + ux * 16
            s.text(qx, qy, card, 19, 800, RED)
    # attributes
    for ent, label, x, y, kind in ATTRS:
        dash = ' stroke-dasharray="7 5"' if kind == 'derived' else ''
        rx = max(AX, text_w(label, 16) / 2 + 22)
        s.front.append(f'<ellipse cx="{x}" cy="{y}" rx="{rx:.1f}" ry="{AY}" fill="{FILL_A}" stroke="{INK}" stroke-width="2"{dash}/>')
        s.text(x, y, label, 16, 500, '#111')
        if kind in ('key', 'partial'):
            w = text_w(label, 16)
            d = ' stroke-dasharray="6 4"' if kind == 'partial' else ''
            s.front.append(f'<line x1="{x - w / 2:.1f}" y1="{y + 11}" x2="{x + w / 2:.1f}" y2="{y + 11}" stroke="#111" stroke-width="1.6"{d}/>')
    # relationships
    for name, rx, ry, ident, *ends in RELS:
        pts = f'{rx},{ry - RH / 2} {rx + RW / 2},{ry} {rx},{ry + RH / 2} {rx - RW / 2},{ry}'
        s.front.append(f'<polygon points="{pts}" fill="{FILL_R}" stroke="{INK}" stroke-width="2.6"/>')
        if ident:
            k = .78
            pts2 = f'{rx},{ry - RH / 2 * k} {rx + RW / 2 * k},{ry} {rx},{ry + RH / 2 * k} {rx - RW / 2 * k},{ry}'
            s.front.append(f'<polygon points="{pts2}" fill="none" stroke="{INK}" stroke-width="1.8"/>')
        s.text(rx, ry, name, 17, 600, '#111')
    # entities
    for name, (x, y, weak) in ENTITIES.items():
        s.front.append(f'<rect x="{x - EW / 2}" y="{y - EH / 2}" width="{EW}" height="{EH}" fill="{FILL_E}" stroke="{INK}" stroke-width="3"/>')
        if weak:
            s.front.append(f'<rect x="{x - EW / 2 + 7}" y="{y - EH / 2 + 7}" width="{EW - 14}" height="{EH - 14}" fill="none" stroke="{INK}" stroke-width="2"/>')
        s.text(x, y, name, 19, 800, '#111')
    s.text(1520, 1415, 'login accounts (no relationships)', 14, 600, '#5a6c8f')
    legend(s, 1790)
    write(svg_path, s.out())


def legend(s, y0):
    s.front.append(f'<line x1="30" y1="{y0 - 40}" x2="1730" y2="{y0 - 40}" stroke="#d0d7e2" stroke-width="1.5"/>')
    s.text(30, y0 - 12, 'Notation', 18, 800, '#111', 'start')
    items = [
        ('entity', 'Entity'), ('weak', 'Weak entity'), ('rel', 'Relationship'), ('ident', 'Identifying relationship'),
        ('attr', 'Attribute'), ('key', 'Key attribute'), ('partial', 'Partial key (discriminator)'), ('derived', 'Derived attribute'),
        ('single', 'Partial participation'), ('double', 'Total participation'), ('card', '1 : N cardinality ratio'),
    ]
    cols, cw, rh = 4, 425, 52
    for i, (kind, label) in enumerate(items):
        x = 30 + (i % cols) * cw
        y = y0 + 30 + (i // cols) * rh
        cx = x + 50
        if kind in ('entity', 'weak'):
            s.front.append(f'<rect x="{cx - 44}" y="{y - 18}" width="88" height="36" fill="{FILL_E}" stroke="{INK}" stroke-width="2.4"/>')
            if kind == 'weak':
                s.front.append(f'<rect x="{cx - 38}" y="{y - 12}" width="76" height="24" fill="none" stroke="{INK}" stroke-width="1.6"/>')
        elif kind in ('rel', 'ident'):
            s.front.append(f'<polygon points="{cx},{y - 20} {cx + 40},{y} {cx},{y + 20} {cx - 40},{y}" fill="{FILL_R}" stroke="{INK}" stroke-width="2.2"/>')
            if kind == 'ident':
                s.front.append(f'<polygon points="{cx},{y - 14} {cx + 29},{y} {cx},{y + 14} {cx - 29},{y}" fill="none" stroke="{INK}" stroke-width="1.5"/>')
        elif kind in ('attr', 'key', 'partial', 'derived'):
            dash = ' stroke-dasharray="6 4"' if kind == 'derived' else ''
            s.front.append(f'<ellipse cx="{cx}" cy="{y}" rx="44" ry="17" fill="#fff" stroke="{INK}" stroke-width="2"{dash}/>')
            s.text(cx, y, 'name', 14, 500, '#111')
            if kind in ('key', 'partial'):
                d = ' stroke-dasharray="5 3"' if kind == 'partial' else ''
                s.front.append(f'<line x1="{cx - 17}" y1="{y + 9}" x2="{cx + 17}" y2="{y + 9}" stroke="#111" stroke-width="1.4"{d}/>')
        elif kind in ('single', 'double'):
            s.line(cx - 44, y, cx + 44, y, double=kind == 'double', width=2.4)
        else:
            s.text(cx - 26, y, '1', 18, 800, RED)
            s.text(cx + 26, y, 'N', 18, 800, RED)
        s.text(x + 112, y, label, 16, 500, '#111', 'start')
    s.text(30, y0 + 200, 'Sale_Items and Purchase_Items are weak entities: a line has no identity of its own. It is identified by its owner (Sales / Purchases)',
           15, 500, '#333', 'start')
    s.text(30, y0 + 224, 'through the identifying relationship plus the partial key product_id, so the full key is (sale_id, product_id) / (purchase_id, product_id).',
           15, 500, '#333', 'start')
    s.text(30, y0 + 248, 'Double lines: every product has a category and a supplier, every sale a customer, every purchase a supplier, every line an owner and a',
           15, 500, '#333', 'start')
    s.text(30, y0 + 272, 'product, every ledger row a product (NOT NULL foreign keys). total_amount is derived: the triggers keep it equal to the sum of the lines.',
           15, 500, '#333', 'start')


# ======================================================================= relational schema
TABLES = {   # name: (x, y, [(column, type, tag)])   tag: PK, FK, PK,FK, UQ, ''
    'Categories': (30, 40, [('category_id', 'INT', 'PK'), ('category_name', 'VARCHAR(50)', 'UQ'), ('description', 'VARCHAR(150)', '')]),
    'Products': (540, 40, [('product_id', 'INT', 'PK'), ('sku', 'VARCHAR(20)', 'UQ'), ('product_name', 'VARCHAR(80)', ''),
                           ('category_id', 'INT', 'FK'), ('supplier_id', 'INT', 'FK'), ('unit_price', 'DECIMAL(10,2)', ''),
                           ('stock_quantity', 'INT', ''), ('min_stock_level', 'INT', ''), ('expiry_date', 'DATE', ''),
                           ('description', 'VARCHAR(150)', ''), ('photo', 'MEDIUMBLOB', '')]),
    'Suppliers': (1050, 40, [('supplier_id', 'INT', 'PK'), ('supplier_name', 'VARCHAR(80)', ''), ('contact_person', 'VARCHAR(60)', ''),
                             ('phone', 'CHAR(10)', 'UQ'), ('email', 'VARCHAR(80)', 'UQ'), ('city', 'VARCHAR(40)', '')]),
    'Stock_Transactions': (30, 700, [('txn_id', 'INT', 'PK'), ('product_id', 'INT', 'FK'), ('txn_type', 'VARCHAR(10)', ''),
                                     ('quantity_change', 'INT', ''), ('txn_date', 'DATETIME', ''), ('reference_id', 'INT', ''), ('remarks', 'VARCHAR(100)', '')]),
    'Sale_Items': (540, 700, [('sale_id', 'INT', 'PK,FK'), ('product_id', 'INT', 'PK,FK'), ('quantity', 'INT', ''), ('unit_price', 'DECIMAL(10,2)', '')]),
    'Purchases': (1050, 560, [('purchase_id', 'INT', 'PK'), ('supplier_id', 'INT', 'FK'), ('purchase_date', 'DATE', ''), ('total_amount', 'DECIMAL(12,2)', '')]),
    'Customers': (30, 1200, [('customer_id', 'INT', 'PK'), ('customer_name', 'VARCHAR(80)', ''), ('phone', 'CHAR(10)', 'UQ'),
                             ('email', 'VARCHAR(80)', 'UQ'), ('city', 'VARCHAR(40)', ''), ('registered_on', 'DATE', '')]),
    'Sales': (540, 1200, [('sale_id', 'INT', 'PK'), ('customer_id', 'INT', 'FK'), ('sale_date', 'DATE', ''), ('total_amount', 'DECIMAL(12,2)', '')]),
    'Purchase_Items': (1050, 1040, [('purchase_id', 'INT', 'PK,FK'), ('product_id', 'INT', 'PK,FK'), ('quantity', 'INT', ''), ('unit_cost', 'DECIMAL(10,2)', '')]),
    'Users': (1050, 1450, [('user_id', 'INT', 'PK'), ('username', 'VARCHAR(40)', 'UQ'), ('password_hash', 'VARCHAR(140)', ''),
                           ('role', 'VARCHAR(10)', ''), ('created_at', 'DATETIME', '')]),
}
TW, TH, RHGT = 430, 56, 42


def row_y(table, col):
    x, y, cols = TABLES[table]
    i = [c[0] for c in cols].index(col)
    return y + TH + i * RHGT + RHGT / 2


def bottom(table):
    x, y, cols = TABLES[table]
    return y + TH + len(cols) * RHGT


def schema(svg_path):
    s = Svg(1510, 1760)
    for name, (x, y, cols) in TABLES.items():
        hgt = TH + len(cols) * RHGT
        s.front.append(f'<rect x="{x}" y="{y}" width="{TW}" height="{hgt}" fill="#fff" stroke="{INK}" stroke-width="2.5"/>')
        s.front.append(f'<rect x="{x}" y="{y}" width="{TW}" height="{TH}" fill="{INK}"/>')
        s.text(x + TW / 2, y + TH / 2, name, 21, 800, '#fff')
        for i, (c, t, tag) in enumerate(cols):
            ry = y + TH + i * RHGT
            bg = '#fff2cc' if 'PK' in tag else '#dde7f5' if tag == 'FK' else None
            if bg:
                s.front.append(f'<rect x="{x + 1.25}" y="{ry}" width="{TW - 2.5}" height="{RHGT}" fill="{bg}"/>')
            if i:
                s.front.append(f'<line x1="{x}" y1="{ry}" x2="{x + TW}" y2="{ry}" stroke="#e1e6ee" stroke-width="1"/>')
            s.text(x + 14, ry + RHGT / 2, c, 17, 800 if 'PK' in tag else 500, '#111', 'start')
            s.text(x + 248, ry + RHGT / 2, t, 15, 500, '#666', 'start')
            if tag:
                s.text(x + TW - 12, ry + RHGT / 2, tag, 14, 800, RED if tag != 'FK' else '#1f5bb5', 'end')
        s.front.append(f'<rect x="{x}" y="{y}" width="{TW}" height="{hgt}" fill="none" stroke="{INK}" stroke-width="2.5"/>')

    def path(pts, one, many):
        d = 'M' + ' L'.join(f'{a:.1f},{b:.1f}' for a, b in pts)
        s.back.append(f'<path d="{d}" fill="none" stroke="#333" stroke-width="2.2"/>')
        (ox, oy, ol), (mx, my, ml) = one, many
        s.text(ox, oy, '1', 18, 800, RED)
        s.text(mx, my, 'N', 18, 800, RED)

    # Categories 1 - N Products (category_id)
    a, b = row_y('Categories', 'category_id'), row_y('Products', 'category_id')
    path([(460, a), (500, a), (500, b), (540, b)], (475, a - 14, ''), (523, b - 14, ''))
    # Suppliers 1 - N Products (supplier_id)
    a, b = row_y('Suppliers', 'supplier_id'), row_y('Products', 'supplier_id')
    path([(1050, a), (1010, a), (1010, b), (970, b)], (1035, a - 14, ''), (987, b - 14, ''))
    # Products 1 - N Stock_Transactions (product_id)
    a = row_y('Stock_Transactions', 'product_id')
    path([(245, 700), (245, 650), (640, 650), (640, bottom('Products'))], (656, bottom('Products') + 16, ''), (262, 684, ''))
    # Products 1 - N Sale_Items
    path([(800, bottom('Products')), (800, 700)], (816, bottom('Products') + 16, ''), (816, 684, ''))
    # Sales 1 - N Sale_Items
    path([(755, 1200), (755, bottom('Sale_Items'))], (771, 1184, ''), (771, bottom('Sale_Items') + 16, ''))
    # Customers 1 - N Sales
    a, b = row_y('Customers', 'customer_id'), row_y('Sales', 'customer_id')
    path([(460, a), (500, a), (500, b), (540, b)], (475, a - 14, ''), (523, b - 14, ''))
    # Suppliers 1 - N Purchases
    path([(1265, bottom('Suppliers')), (1265, 560)], (1281, bottom('Suppliers') + 16, ''), (1281, 544, ''))
    # Purchases 1 - N Purchase_Items
    path([(1265, bottom('Purchases')), (1265, 1040)], (1281, bottom('Purchases') + 16, ''), (1281, 1024, ''))
    # Products 1 - N Purchase_Items (product_id)
    a = row_y('Purchase_Items', 'product_id')
    path([(970, 520), (1025, 520), (1025, a), (1050, a)], (987, 506, ''), (1037, a - 14, ''))
    s.text(30, 1730, 'PK = primary key (yellow), FK = foreign key (blue), UQ = unique, 1 / N = cardinality at each end of a foreign-key link. '
           'Composite keys: Sale_Items (sale_id, product_id), Purchase_Items (purchase_id, product_id).', 15, 500, '#333', 'start')
    write(svg_path, s.out())


def write(path, text):
    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)
    print('wrote', os.path.relpath(path, ROOT))


def png(svg, out_png, size, crop):
    """Quick Look renders into a square: put the drawing in the middle of a square page, render, then crop it out."""
    w, h = crop
    side = max(w, h)
    body = open(svg, encoding='utf-8').read().replace('<svg ', f'<svg x="{(side - w) / 2}" y="{(side - h) / 2}" ', 1)
    d = os.path.dirname(out_png)
    tmp = os.path.join(d, '_square_' + os.path.basename(svg))
    with open(tmp, 'w', encoding='utf-8') as f:
        f.write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{side}" height="{side}" viewBox="0 0 {side} {side}">{body}</svg>')
    try:
        subprocess.run(['qlmanage', '-t', '-s', str(side), '-o', d, tmp], check=True, capture_output=True)
        os.replace(tmp + '.png', out_png)
    finally:
        os.remove(tmp)
    subprocess.run(['sips', '-c', str(h), str(w), out_png, '--out', out_png], check=True, capture_output=True)
    print('wrote', os.path.relpath(out_png, ROOT))


if __name__ == '__main__':
    er_svg = os.path.join(ROOT, 'public', 'img', 'er_diagram.svg')
    sc_svg = os.path.join(ROOT, 'public', 'img', 'schema.svg')
    er(er_svg)
    schema(sc_svg)
    if '--png' in sys.argv:
        png(er_svg, os.path.join(ROOT, 'docs', 'img', 'er_diagram.png'), 2090, (1760, 2090))
        png(sc_svg, os.path.join(ROOT, 'docs', 'img', 'schema.png'), 1760, (1510, 1760))
