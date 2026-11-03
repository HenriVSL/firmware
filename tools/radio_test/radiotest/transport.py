# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Henri Lavikainen
"""Links to the radio's console: the log port of a radio over USB, or the
headless simulator run as a child process. Both keep every line received,
numbered, with the PC's time, so a wait can look from the moment a
command was sent."""
import glob
import os
import select
import subprocess
import threading
import time

LOG_PORT = '/dev/serial/by-id/*DeltaMM*-if02'
DATA_PORT = '/dev/serial/by-id/*DeltaMM*-if00'


class Line:
    __slots__ = ('seq', 'time', 'text')

    def __init__(self, seq, at, text):
        self.seq = seq
        self.time = at
        self.text = text

    def __str__(self):
        return self.text


class LinkError(Exception):
    pass


class Link:
    """Lines received, and a way to send one."""

    def __init__(self):
        self.lines = []
        self.cond = threading.Condition()
        self.closing = False
        self.watchers = []
        self.started = time.monotonic()
        self.connects = 0

    def _add(self, text):
        with self.cond:
            line = Line(len(self.lines), time.monotonic(), text)
            self.lines.append(line)
            self.cond.notify_all()
        for watch in self.watchers:
            watch(line)

    def note(self, text):
        """A line of the runner's own in the kept log."""
        self._add('[runner] ' + text)

    def mark(self):
        with self.cond:
            return len(self.lines)

    def since(self, seq):
        with self.cond:
            return self.lines[seq:]

    def wait_for(self, test, since, timeout):
        """The first line from @p since for which test(line) holds, or None
        after @p timeout seconds."""
        end = time.monotonic() + timeout
        seq = since
        with self.cond:
            while True:
                while seq < len(self.lines):
                    line = self.lines[seq]
                    seq += 1
                    if test(line):
                        return line
                left = end - time.monotonic()
                if left <= 0:
                    return None
                self.cond.wait(left)

    def send(self, text):
        raise NotImplementedError

    def close(self):
        self.closing = True


def find_port(pattern):
    found = sorted(glob.glob(pattern))
    return found[0] if found else None


def port_users(path):
    """The other processes that have the device @p path open, as
    (pid, command line)."""
    device = os.path.realpath(path)
    users = []
    for pid in os.listdir('/proc'):
        if not pid.isdigit() or int(pid) == os.getpid():
            continue
        try:
            fds = os.listdir('/proc/%s/fd' % pid)
        except OSError:
            continue
        for fd in fds:
            try:
                if os.readlink('/proc/%s/fd/%s' % (pid, fd)) == device:
                    with open('/proc/%s/cmdline' % pid, 'rb') as f:
                        command = f.read().replace(b'\0', b' ').decode('utf-8', 'replace').strip()
                    users.append((int(pid), command))
                    break
            except OSError:
                continue
    return users


def data_device():
    """The data port's device name (ttyACM0), as dmrconf -D wants it."""
    port = find_port(DATA_PORT)
    if port is None:
        raise LinkError('no DeltaMM data port (%s)' % DATA_PORT)
    return os.path.basename(os.path.realpath(port))


class SerialLink(Link):
    """The radio's log port. It goes away while the radio restarts and
    comes back under the same name: the reader opens it again."""

    def __init__(self, pattern=LOG_PORT):
        super().__init__()
        self.pattern = pattern
        self.port = None
        self.fd = None
        self.serial = None
        self.write_lock = threading.Lock()
        # One program per port: another reader takes lines away.
        port = find_port(pattern)
        if port is not None:
            users = port_users(port)
            if users:
                raise LinkError('the log port %s is open in another program: %s; close it first' % (
                    os.path.realpath(port), ', '.join('PID %d (%s)' % u for u in users)))
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()
        if not self.wait_connected(10):
            raise LinkError('no DeltaMM log port (%s)' % pattern)

    def wait_connected(self, timeout):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if self.fd is not None or self.serial is not None:
                return True
            time.sleep(0.05)
        return False

    def _open(self, path):
        try:
            import serial  # optional
            self.serial = serial.Serial(path, 115200, timeout=0.1)
            return
        except ImportError:
            pass
        import termios
        import tty
        fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        tty.setraw(fd)
        attrs = termios.tcgetattr(fd)
        attrs[3] &= ~termios.ECHO
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        self.fd = fd

    def _close(self):
        if self.serial is not None:
            try:
                self.serial.close()
            except OSError:
                pass
        if self.fd is not None:
            try:
                os.close(self.fd)
            except OSError:
                pass
        self.serial = None
        self.fd = None

    def _read(self):
        if self.serial is not None:
            return self.serial.read(4096)
        ready, _, _ = select.select([self.fd], [], [], 0.1)
        if not ready:
            return b''
        data = os.read(self.fd, 4096)
        if not data:
            raise OSError('port closed')
        return data

    def _run(self):
        pending = b''
        while not self.closing:
            if self.fd is None and self.serial is None:
                path = find_port(self.pattern)
                if path is None:
                    time.sleep(0.1)
                    continue
                try:
                    self._open(os.path.realpath(path))
                except OSError:
                    time.sleep(0.2)
                    continue
                self.port = path
                self.connects += 1
                if self.connects > 1:
                    self.note('log port back: ' + os.path.realpath(path))
            try:
                data = self._read()
            except Exception:  # pyserial raises its own types
                self._close()
                self.note('log port gone')
                continue
            pending += data
            while b'\n' in pending:
                raw, pending = pending.split(b'\n', 1)
                self._add(raw.decode('utf-8', 'replace').rstrip('\r'))
        self._close()

    def send(self, text):
        if not self.wait_connected(20):
            raise LinkError('log port did not come back')
        data = (text + '\r\n').encode()
        with self.write_lock:
            try:
                if self.serial is not None:
                    self.serial.write(data)
                else:
                    os.write(self.fd, data)
            except Exception as e:
                raise LinkError('writing to the log port: %s' % e)

    def describe(self):
        return 'radio, log port %s' % (self.port or self.pattern)


class SimLink(Link):
    """The headless simulator with its console on standard input. A restart
    ends the program; it is started again, as the radio would."""

    def __init__(self, path, args=()):
        super().__init__()
        if not os.access(path, os.X_OK):
            raise LinkError('no simulator at %s' % path)
        self.path = path
        self.args = list(args)
        self.proc = None
        self.lock = threading.Lock()
        self._start()

    def _start(self):
        self.proc = subprocess.Popen([self.path, '--stdin'] + self.args, stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0)
        self.connects += 1
        if self.connects > 1:
            self.note('simulator started again')
        threading.Thread(target=self._run, args=(self.proc,), daemon=True).start()

    def _run(self, proc):
        pending = b''
        while True:
            data = os.read(proc.stdout.fileno(), 65536)
            if not data:
                break
            pending += data
            while b'\n' in pending:
                raw, pending = pending.split(b'\n', 1)
                self._add(raw.decode('utf-8', 'replace').rstrip('\r'))
        proc.wait()
        if self.closing:
            return
        self.note('simulator ended (%d)' % proc.returncode)
        with self.lock:
            if self.proc is proc and not self.closing:
                time.sleep(0.2)
                self._start()

    def send(self, text):
        for _ in range(100):
            with self.lock:
                proc = self.proc
            try:
                proc.stdin.write((text + '\n').encode())
                proc.stdin.flush()
                return
            except (BrokenPipeError, OSError):
                time.sleep(0.05)
        raise LinkError('simulator does not take input')

    def close(self):
        self.closing = True
        with self.lock:
            proc = self.proc
        if proc and proc.poll() is None:
            try:
                proc.stdin.close()
                proc.wait(5)
            except Exception:
                proc.kill()

    def describe(self):
        return 'simulator %s' % self.path
