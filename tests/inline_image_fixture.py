"""Loopback-only deterministic HTTP fixtures for the native image tests.

Run with Python 3 and Pillow installed. Production builds reject loopback image
URLs; the backend harness is compiled with the explicit test-only definition.
"""
import argparse
import io
import json
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from PIL import Image, ImageDraw


def encoded(image, fmt, **kwargs):
    out = io.BytesIO()
    image.save(out, fmt, **kwargs)
    return out.getvalue()


RED = Image.new('RGB', (2, 2), (255, 0, 0))
FIXTURES = {
    '/png': ('image/png', encoded(RED, 'PNG')),
    '/caf%C3%A9': ('image/png', encoded(RED, 'PNG')),
    '/jpeg': ('image/jpeg', encoded(RED, 'JPEG')),
    '/bmp': ('image/bmp', encoded(RED, 'BMP')),
    '/gif': ('image/gif', encoded(RED, 'GIF', save_all=True,
              append_images=[Image.new('RGB', (2, 2), (0, 0, 255))],
              duration=100, loop=0)),
    '/alpha': ('image/png', encoded(Image.new('RGBA', (2, 2), (255, 0, 0, 128)), 'PNG')),
    '/huge-dimensions': ('image/png', encoded(Image.new('RGB', (8193, 1)), 'PNG')),
    '/bad': ('image/png', b'This is deliberately not an image.'),
}
scene = Image.new('RGB', (640, 240), '#22445c')
draw = ImageDraw.Draw(scene)
draw.rectangle((16, 16, 623, 223), outline='#efce79', width=4)
draw.rectangle((40, 65, 155, 175), fill='#efce79')
draw.ellipse((205, 65, 315, 175), fill='#72bd8d')
draw.polygon(((420, 65), (480, 175), (360, 175)), fill='#e98c79')
draw.text((40, 30), 'INLINE IMAGE TEST - 640 x 240', fill='white')
draw.text((40, 200), 'The text below must scroll with this image.', fill='white')
FIXTURES['/scene'] = ('image/png', encoded(scene, 'PNG'))


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def do_GET(self):
        path = self.path.split('?', 1)[0]
        if path == '/large':
            if path not in FIXTURES:
                FIXTURES[path] = ('image/png', encoded(Image.new('RGB', (4096, 4095), 'red'), 'PNG'))
        if path in ('/redirect', '/redirect-loop', '/redirect-file'):
            self.send_response(302)
            self.send_header('Location', {'/redirect': '/png',
                '/redirect-loop': '/redirect-loop',
                '/redirect-file': 'file:///C:/Windows/win.ini'}[path])
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        if path == '/too-large':
            self.send_response(200)
            self.send_header('Content-Length', str(17 * 1024 * 1024))
            self.end_headers()
            return
        if path.startswith('/slow'):
            time.sleep(1.5)
            path = '/scene' if path == '/slow-scene' else '/png'
        mime, payload = FIXTURES.get(path, ('text/plain', b'Not found'))
        self.send_response(200 if path in FIXTURES else 404)
        self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(payload)))
        self.end_headers()
        try:
            self.wfile.write(payload)
        except (BrokenPipeError, ConnectionResetError):
            pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=18765)
    args = parser.parse_args()
    server = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
    print(json.dumps({'fixture_url': f'http://127.0.0.1:{server.server_port}'}), flush=True)
    server.serve_forever()
