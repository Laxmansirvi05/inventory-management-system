#!/usr/bin/env python3
"""fetch_product_photos.py : a real photograph for each of the 60 seed products, from Wikimedia Commons.

Every photo below is openly licensed (CC0, public domain, CC BY or CC BY-SA). The script downloads each one at
800 px wide, crops it to 4:3 (the shape of the product cards), resizes it to 640 x 480 and saves it as JPEG with
macOS sips, then writes:

    sql/photos.sql            UPDATE Products SET photo = 0xFFD8... WHERE sku = 'SKU0001';   (one per product)
    docs/PHOTO-CREDITS.md     author, licence and source page of every photo (the licences require this credit)

Run on macOS:  python3 scripts/fetch_product_photos.py
Load it:       scripts/load_photos.sh <database>   (scripts/load_db.sh loads it as part of a fresh database)
"""
import json
import os
import re
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
UA = {'User-Agent': 'InventoryCollegeProject/1.0 (product photos for a student inventory app)'}
W, H = 640, 480

PHOTOS = {   # SKU -> Wikimedia Commons file
    'SKU0001': 'File:Ballpoint Pen.jpg',
    'SKU0002': 'File:Disassembled - Reassembled Ystudio Classic Revolve Brassing Black Ballpoint Pen 20261030.jpg',
    'SKU0003': 'File:Gel pen.jpg',
    'SKU0004': 'File:Pencils hb.jpg',
    'SKU0005': 'File:Spiralbinderücken -- 2022 -- 9722.jpg',
    'SKU0006': 'File:Notebooks-rainbow.jpg',
    'SKU0007': 'File:1990s Rank Xerox A4 75gsm copier paper cardboard box and A4 paper made in Sweden.jpg',
    'SKU0008': 'File:2017 Zszywacz biurowy.jpg',
    'SKU0009': 'File:Markeerstift-01 (xndr).jpg',
    'SKU0010': 'File:Brass Protractor.jpg',
    'SKU0011': 'File:USB-C cable 2017 A.jpg',
    'SKU0012': 'File:Earphones BW 2011-12-10 15-49-08.JPG',
    'SKU0013': 'File:USB-C cable connected to a powerbank.jpg',
    'SKU0014': 'File:Kingston Technology DataTraveler G4 USB flash drive USB 3.0 32 Gb.jpg',
    'SKU0015': 'File:A wireless computer mouse.jpg',
    'SKU0016': 'File:Razer Arctosa keyboard-top-ar 16to9 PNr°0325.jpg',
    'SKU0017': 'File:Energizer NiMH rechargeable AA batteries (four) (vertical).jpg',
    'SKU0018': 'File:DIY tablet smartphone stand 2.jpg',
    'SKU0019': 'File:LED light bulb ~171113.jpg',
    'SKU0020': 'File:Power strip 2014.JPG',
    'SKU0021': 'File:Basmati Rice India, raw.jpg',
    'SKU0022': 'File:Atta-01.jpg',
    'SKU0023': 'File:Dal packet.jpg',
    'SKU0024': 'File:Cube sugar bowl at Matching Green, Essex, England.jpg',
    'SKU0025': 'File:Table salt with salt shaker V1.jpg',
    'SKU0026': 'File:Bottle 1 liter Sunflower refined oil.jpg',
    'SKU0027': 'File:Koekjestrommel open.jpg',
    'SKU0028': 'File:Lobster instant noodle.jpg',
    'SKU0029': 'File:Ceylon black tea leaves.jpg',
    'SKU0030': 'File:Peanut butter 14juni09 001.jpg',
    'SKU0031': 'File:Kirkland Signature Drinking Water 1.5L 20050508.jpg',
    'SKU0032': 'File:Coca-cola 50cl can - Italia.jpg',
    'SKU0033': 'File:Orange juice 1 edit1.jpg',
    'SKU0034': 'File:Energy Drink Battery Cans.jpg',
    'SKU0035': 'File:Teabag with green tag.jpg',
    'SKU0036': 'File:Instant coffee.jpg',
    'SKU0037': 'File:Es soda gembira Fanta air soda.JPG',
    'SKU0038': 'File:Coconut Drink, Pangandaran.JPG',
    'SKU0039': 'File:Glass of red lemonade with bottle.jpg',
    'SKU0040': 'File:Plain Lassi in a glass.jpg',
    'SKU0041': 'File:Handmade soap.jpg',
    'SKU0042': 'File:Dove shampoo bottle.jpg',
    'SKU0043': 'File:舒客 Shuke cherry blossom enzyme toothpaste in a tube.jpg',
    'SKU0044': 'File:Toothbrush x3 20050716 002.jpg',
    'SKU0045': 'File:Palmolive soap.jpg',
    'SKU0046': 'File:Woman reaches for a cosmetic product in a display.jpg',
    'SKU0047': 'File:Bath and Body Works Sparking Blackberry Woods lotion (10765070103).jpg',
    'SKU0048': 'File:Spray on deodorant.jpg',
    'SKU0049': 'File:A Sanitary Towel on White Background.jpg',
    'SKU0050': 'File:Coconut oil bottle in the background of dried coconuts from Kaleeswari Farm.jpg',
    'SKU0051': 'File:Ghari Detergent 1 kg Pack.jpg',
    'SKU0052': 'File:Dish-soap.jpg',
    'SKU0053': 'File:Mop and bucket.jpg',
    'SKU0054': 'File:ToiletDuck200706251755.jpg',
    'SKU0055': 'File:Black garbage bag.jpg',
    'SKU0056': 'File:Haushaltsschwaemme.jpg',
    'SKU0057': 'File:Dülmen, Kreuzkapelle, Desinfektionsmittel -- 2021 -- 7363.jpg',
    'SKU0058': 'File:Mosquito coil.JPG',
    'SKU0059': 'File:Candle light projected through a glass orb.jpg',
    'SKU0060': 'File:Matchbox home object Rotimi8.jpg',
}


def get(url):
    for attempt in range(7):
        try:
            return urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60).read()
        except Exception:
            time.sleep(5 + attempt * 8)
    raise SystemExit('could not download ' + url)


def info(title):
    url = 'https://commons.wikimedia.org/w/api.php?' + urllib.parse.urlencode({
        'action': 'query', 'format': 'json', 'titles': title, 'prop': 'imageinfo',
        'iiprop': 'url|size|extmetadata', 'iiurlwidth': 800})
    page = next(iter(json.loads(get(url))['query']['pages'].values()))
    ii = page['imageinfo'][0]
    meta = ii.get('extmetadata', {})
    clean = lambda k: re.sub(r'\s+', ' ', re.sub('<[^>]+>', '', meta.get(k, {}).get('value', ''))).strip()
    return {'thumb': ii['thumburl'], 'page': ii['descriptionurl'], 'artist': clean('Artist') or 'unknown',
            'license': clean('LicenseShortName'), 'license_url': meta.get('LicenseUrl', {}).get('value', '')}


def to_card_jpeg(src, dst):
    """Centre-crop to 4:3, resize to W x H, save as JPEG (quality 82)."""
    out = subprocess.run(['sips', '-g', 'pixelWidth', '-g', 'pixelHeight', src], capture_output=True, text=True, check=True).stdout
    w = int(re.search(r'pixelWidth: (\d+)', out).group(1))
    h = int(re.search(r'pixelHeight: (\d+)', out).group(1))
    cw, ch = (round(h * 4 / 3), h) if w / h > 4 / 3 else (w, round(w * 3 / 4))
    subprocess.run(['sips', '-c', str(ch), str(cw), src, '--out', src], check=True, capture_output=True)
    subprocess.run(['sips', '-z', str(H), str(W), '-s', 'format', 'jpeg', '-s', 'formatOptions', '82', src, '--out', dst], check=True, capture_output=True)


def main():
    names = dict(re.findall(r"\('(SKU\d+)',\s*'([^']*)'", open(os.path.join(ROOT, 'sql', 'seed.sql'), encoding='utf-8').read()))
    credits = []
    with tempfile.TemporaryDirectory() as tmp, open(os.path.join(ROOT, 'sql', 'photos.sql'), 'w', encoding='ascii') as sql:
        sql.write('-- photos.sql : a real photograph of each of the 60 seed products (Wikimedia Commons, open licences;\n'
                  '-- credits in docs/PHOTO-CREDITS.md). Generated by scripts/fetch_product_photos.py.\n'
                  '-- Stores a 640x480 JPEG in Products.photo (MEDIUMBLOB). Run after seed.sql.\n')
        total = 0
        for sku, title in PHOTOS.items():
            meta = info(title)
            raw = os.path.join(tmp, sku + '.src')
            with open(raw, 'wb') as f:
                f.write(get(meta['thumb']))
            jpg = os.path.join(tmp, sku + '.jpg')
            to_card_jpeg(raw, jpg)
            data = open(jpg, 'rb').read()
            total += len(data)
            sql.write("UPDATE Products SET photo = 0x%s WHERE sku = '%s';\n" % (data.hex().upper(), sku))
            credits.append((sku, names.get(sku, ''), title, meta))
            print(sku, len(data) // 1024, 'KB', flush=True)
            time.sleep(1.5)
    with open(os.path.join(ROOT, 'docs', 'PHOTO-CREDITS.md'), 'w', encoding='utf-8') as f:
        f.write('# Product photo credits\n\nThe product pictures come from Wikimedia Commons under open licences. '
                'Each was cropped to 4:3 and resized to 640 x 480 for the product cards (sql/photos.sql, made by '
                'scripts/fetch_product_photos.py). CC BY and CC BY-SA licences require this credit.\n\n'
                '| SKU | Product | Photo | Author | Licence |\n|---|---|---|---|---|\n')
        for sku, name, title, m in credits:
            lic = '[%s](%s)' % (m['license'], m['license_url']) if m['license_url'] else m['license']
            f.write('| %s | %s | [%s](%s) | %s | %s |\n' % (sku, name, title.replace('File:', '').replace('|', '-'), m['page'],
                                                         m['artist'].replace('|', '-')[:80], lic))
    write_credits_page(credits)
    print('wrote sql/photos.sql (%d KB of JPEG), docs/PHOTO-CREDITS.md and public/credits.html' % (total // 1024))


def write_credits_page(credits):
    """The same credits as a page in the app (linked from the products page), since the photos are shown there."""
    esc = lambda t: t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;').replace('"', '&quot;')
    rows = ''.join('<tr><td>%s</td><td>%s</td><td><a href="%s" rel="noopener">%s</a></td><td>%s</td><td>%s</td></tr>' % (
        sku, esc(name), esc(m['page']), esc(title.replace('File:', '')), esc(m['artist'][:80]),
        ('<a href="%s" rel="noopener">%s</a>' % (esc(m['license_url']), esc(m['license']))) if m['license_url'] else esc(m['license']))
        for sku, name, title, m in credits)
    page = ('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width, initial-scale=1">\n'
            '<title>Photo credits · Inventory</title>\n<link rel="icon" href="img/favicon.svg" type="image/svg+xml">\n'
            '<link rel="stylesheet" href="css/style.css">\n</head>\n<body style="padding:32px">\n<main style="max-width:1100px;margin:0 auto">\n'
            '<p><a href="products.html">Back to products</a></p>\n<h1 style="font:800 40px var(--head);margin:8px 0">Product photo credits</h1>\n'
            '<p class="muted">The product photos come from Wikimedia Commons under open licences (CC0, public domain, CC BY, CC BY-SA). '
            'Each was cropped to 4:3 and resized to 640 x 480.</p>\n<section class="card"><div class="table-wrap"><table class="tbl">'
            '<thead><tr><th>SKU</th><th>Product</th><th>Photo</th><th>Author</th><th>Licence</th></tr></thead><tbody>'
            + rows + '</tbody></table></div></section>\n</main>\n</body>\n</html>\n')
    with open(os.path.join(ROOT, 'public', 'credits.html'), 'w', encoding='utf-8') as f:
        f.write(page)


if __name__ == '__main__':
    main()
