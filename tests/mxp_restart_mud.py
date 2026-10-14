"""Loopback-only MXP restart fixture for upstream issue #87.

The `silent` command emits plain text while leaving MXP negotiated. The
`reoffer` command sends IAC DONT MXP followed by IAC DO MXP on the same socket,
then emits tagged text. The `hard-close` command closes that socket so a fresh
client connection can be compared. No external MUD or installed client is used.
"""
import argparse
import json
import socket
import socketserver
import threading
import time

IAC, WILL, DO, DONT, SB, SE, MXP = 255, 251, 253, 254, 250, 240, 91


class Mud(socketserver.BaseRequestHandler):
    def send(self, payload):
        self.request.sendall(payload.encode('ascii'))

    def tagged(self, marker):
        self.send(f'\x1b[6z<color fore=#ff0000>{marker}</color>\r\n')

    def handle(self):
        with self.server.counter_lock:
            self.server.connections += 1
            connection = self.server.connections
        self.request.settimeout(60)
        self.request.sendall(bytes((IAC, WILL, MXP)))
        time.sleep(.1)
        self.request.sendall(bytes((IAC, SB, MXP, IAC, SE)))
        self.tagged(f'RESTART_INITIAL_{connection}')
        print(json.dumps({'event': 'connected', 'connection': connection}), flush=True)

        pending = bytearray()
        telnet_state = 'text'
        verb = 0
        try:
            while True:
                incoming = self.request.recv(4096)
                if not incoming:
                    break
                for byte in incoming:
                    if telnet_state == 'text':
                        if byte == IAC:
                            telnet_state = 'iac'
                        else:
                            pending.append(byte)
                    elif telnet_state == 'iac':
                        if byte in (WILL, 252, DO, DONT):
                            verb, telnet_state = byte, 'option'
                        elif byte == SB:
                            telnet_state = 'subnegotiation'
                        else:
                            if byte == IAC:
                                pending.append(byte)
                            telnet_state = 'text'
                    elif telnet_state == 'option':
                        print(json.dumps({'event': 'telnet-reply', 'connection': connection,
                                          'verb': verb, 'option': byte}), flush=True)
                        telnet_state = 'text'
                    elif telnet_state == 'subnegotiation':
                        if byte == IAC:
                            telnet_state = 'subnegotiation-iac'
                    elif telnet_state == 'subnegotiation-iac':
                        telnet_state = 'text' if byte == SE else 'subnegotiation'
                while b'\n' in pending:
                    raw, _, remainder = pending.partition(b'\n')
                    pending = bytearray(remainder)
                    command = raw.decode('ascii', errors='replace').strip()
                    print(json.dumps({'event': 'command', 'connection': connection,
                                      'command': command}), flush=True)
                    if command == 'silent':
                        self.send('RESTART_SERVER_SILENT: no MXP markup in this packet.\r\n')
                    elif command == 'soft-no-neg':
                        self.tagged('RESTART_SOFT_NO_NEG')
                    elif command == 'reoffer':
                        self.request.sendall(bytes((IAC, DONT, MXP)))
                        time.sleep(.1)
                        self.request.sendall(bytes((IAC, DO, MXP)))
                        time.sleep(.1)
                        self.tagged('RESTART_REOFFER')
                    elif command == 'reoffer-will':
                        self.request.sendall(bytes((IAC, DONT, MXP)))
                        time.sleep(.1)
                        self.request.sendall(bytes((IAC, WILL, MXP)))
                        time.sleep(.1)
                        self.tagged('RESTART_REOFFER_WILL')
                    elif command == 'query-reoffer':
                        self.request.sendall(bytes((IAC, DONT, MXP)))
                        time.sleep(.1)
                        self.request.sendall(bytes((IAC, DO, MXP)))
                        time.sleep(.1)
                        self.tagged('RESTART_QUERY_REOFFER')
                    elif command == 'no-offer':
                        self.request.sendall(bytes((IAC, DO, MXP, IAC, WILL, MXP)))
                        time.sleep(.1)
                        self.tagged('RESTART_NO_MXP')
                    elif command == 'oncommand-before':
                        self.request.sendall(bytes((IAC, DO, MXP)))
                        time.sleep(.1)
                        self.tagged('RESTART_ONCOMMAND_BEFORE')
                    elif command == 'oncommand-start':
                        self.request.sendall(bytes((IAC, SB, MXP, IAC, SE)))
                        time.sleep(.1)
                        self.tagged('RESTART_ONCOMMAND_STARTED')
                    elif command == 'hard-close':
                        self.request.shutdown(socket.SHUT_RDWR)
                        return
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass
        finally:
            print(json.dumps({'event': 'disconnected', 'connection': connection}), flush=True)


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=18766)
    args = parser.parse_args()
    if args.port != 18766:
        raise SystemExit('This test is restricted to the dedicated 127.0.0.1:18766 fixture')
    server = Server(('127.0.0.1', args.port), Mud)
    server.connections = 0
    server.counter_lock = threading.Lock()
    print('MXP restart fixture ready on 127.0.0.1:18766', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        server.shutdown()
    finally:
        server.server_close()
