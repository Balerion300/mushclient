"""Local telnet fixture for visually and programmatically checking both protocols.

Connect only a separate test world. Enter `suite`, `scroll`, or `clear-test`.
The fixture never contacts a production game. Images use a public GitHub fixture
so the normal application binary can be tested without its loopback test hook.
"""
import argparse
import json
import socketserver
import threading
import time

IMAGE = 'https://raw.githubusercontent.com/Balerion300/mushclient/codex/inline-static-images/tests/fixture-scene.png'


class Mud(socketserver.BaseRequestHandler):
    def send(self, text):
        self.request.sendall(text.encode('utf-8'))

    def suite(self):
        self.send('\r\nBEFORE_IMAGES: paragraph above the first illustration.\r\n')
        if self.server.protocol == 'mxp':
            self.send('\x1b[1z<image url="' + IMAGE + '">\r\n')
            self.send('\x1b[1z<send href="text-clicked">Clickable text</send>\r\n')
            self.send('\x1b[1z<send href="image-clicked"><image url="' + IMAGE + '"></send>\r\n')
        else:
            self.send('<img src="' + IMAGE + '">\r\n')
            self.send('<a xch_cmd="text-clicked">Clickable text</a>\r\n')
            self.send('<a xch_cmd="image-clicked"><img src="' + IMAGE + '"></a>\r\n')
        self.send('AFTER_IMAGES: paragraph below the illustrations.\r\nINLINE_SUITE_END\r\n')

    def handle(self):
        if self.server.protocol == 'mxp':
            self.request.sendall(bytes((255, 251, 91)))  # WILL MXP
            time.sleep(.3)
            self.request.sendall(bytes((255, 250, 91, 255, 240)))
        else:
            self.send('This world is Pueblo 1.10 enhanced.\r\n')
        self.send('Isolated inline-image test. Commands: suite, scroll, clear-test.\r\n')
        time.sleep(.5)
        self.suite()
        pending = b''
        while True:
            data = self.request.recv(4096)
            if not data: return
            pending += data
            while b'\n' in pending:
                raw, pending = pending.split(b'\n', 1)
                command = raw.decode('utf-8', errors='ignore').strip()
                print(json.dumps({'protocol': self.server.protocol, 'command': command}), flush=True)
                if command == 'suite': self.suite()
                elif command == 'scroll':
                    for i in range(80): self.send(f'Scrollback test line {i+1:02d}\r\n')
                    self.suite()
                elif command in ('image-clicked', 'text-clicked'):
                    self.send('CLICK_CONFIRMED: ' + command + '\r\n')
                elif command == 'clear-test': self.suite()


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--mxp-port', type=int, default=18766)
    parser.add_argument('--pueblo-port', type=int, default=18767)
    args = parser.parse_args()
    servers = []
    for protocol, port in [('mxp', args.mxp_port), ('pueblo', args.pueblo_port)]:
        server = Server(('127.0.0.1', port), Mud)
        server.protocol = protocol
        servers.append(server)
        threading.Thread(target=server.serve_forever, daemon=True).start()
    print('Loopback fixtures ready: MXP and Pueblo', flush=True)
    try:
        while True: time.sleep(.5)
    except KeyboardInterrupt:
        for server in servers: server.shutdown(); server.server_close()
