"""The local OTA server: GitHub's release downloads, served from this PC (ADR-010, gate 1).

Update tests never fetch from GitHub's releases (the owner, 2026-11-17: they are not to be hammered). A unit's poll
and download, and `rc_run.py update`, are pointed here instead, through the unit's update source.

What it serves, under any owner and repository, as GitHub does:
    /<owner>/<repo>/releases/latest/download/<file>    the latest release's file
    /<owner>/<repo>/releases/download/<tag>/<file>      that release's file
The releases are the subfolders of --root, one per tag, each as build_release.py's `release` folder is laid out
(manifest.json, SHA256SUMS and the images). The latest is --latest, or else the highest tag.

As on GitHub, a download is answered with a 302 to an asset host, which serves the file: a second port, with a
certificate chain of its own. GitHub's own host has an ECDSA chain (a P-256 leaf and intermediate under a P-384 root),
its asset host (release-assets.githubusercontent.com) an RSA one (2048-bit leaf and intermediate under a 4096-bit
root), both read 2026-11-17. The test chains here are of the same kinds, so a unit meets the same handshakes.

Ports: 8080 (HTTP) and 8443 (HTTPS) for the main host, 8081 and 8444 for the asset host. Plain HTTP redirects to
plain HTTP, HTTPS to HTTPS. Every request is printed with its status and the bytes sent.

Certificates: --make-certs writes the two test roots, their intermediates and the server's leaves into --cert-dir,
with openssl (on the PATH: Git for Windows carries one), for the addresses given with --host. A unit is built to trust
the two roots (`ecc_root.pem`, `rsa_root.pem`).

Usage, from the repo root with the venv:
    venv\\Scripts\\python tools\\ota_server.py --make-certs --host 192.168.2.13
    venv\\Scripts\\python tools\\ota_server.py --root logs\\ota_server\\releases

Plain HTTP and test certificates: run it only on a network you trust.
"""
import argparse
import http.server
import re
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DEFAULT_ROOT = REPO / "logs" / "ota_server" / "releases"
DEFAULT_CERT_DIR = REPO / "logs" / "ota_server" / "certs"

# Long enough for a handshake at the Debug build's -O0 on the Pico 2 W (148 s, 2026-11-17)
CONNECTION_TIMEOUT_S = 300

RELEASE_PATH = re.compile(r"^/[^/]+/[^/]+/releases/(?:latest/download|download/(?P<tag>[^/]+))/(?P<file>[^/?]+)$")
ASSET_PATH = re.compile(r"^/assets/(?P<tag>[^/]+)/(?P<file>[^/?]+)$")

# Each chain: the root's key and digest, the intermediate's key and the digest the root signs it with, the leaf's
# key and the digest the intermediate signs it with. A key is an EC curve's name or an RSA key's bits
CHAINS = {
    "ecc": {"root_key": "secp384r1", "root_md": "sha384",
            "int_key": "prime256v1", "int_md": "sha384",
            "leaf_key": "prime256v1", "leaf_md": "sha256"},
    "rsa": {"root_key": "4096", "root_md": "sha256",
            "int_key": "2048", "int_md": "sha256",
            "leaf_key": "2048", "leaf_md": "sha256"},
}


def tag_key(tag: str) -> list:
    """Order tags by their numbers, so v2.0.10 comes after v2.0.9.

    Args:
        tag: a release's tag, such as v2.0.2 or v2.0.2-rc1.

    Returns:
        A sort key: the tag's numbers, then the tag itself.
    """
    return [int(number) for number in re.findall(r"\d+", tag)] + [tag]


class Releases:
    """The releases under a root folder, read again at each request so a test can change them while serving."""

    def __init__(self, root: Path, latest: str | None):
        """Args:
            root: the folder whose subfolders are releases, one per tag.
            latest: the tag served as the latest, or None for the highest tag.
        """
        self.root = root
        self.latest = latest

    def tags(self) -> list:
        """Returns: the tags, lowest first."""
        return sorted((path.name for path in self.root.iterdir() if path.is_dir()), key=tag_key)

    def resolve(self, tag: str | None, file: str) -> tuple:
        """Find a release's file.

        Args:
            tag: the release's tag, or None for the latest.
            file: the file's name within the release.

        Returns:
            (tag, path) where path is the file, or (tag, None) when the release or the file does not exist.
        """
        if tag is None:
            tags = self.tags()
            tag = self.latest or (tags[-1] if tags else None)
        if tag is None:
            return None, None
        path = self.root / tag / file
        if path.parent.parent != self.root or not path.is_file():
            return tag, None
        return tag, path


def make_handler(releases: Releases, role: str, asset_base: str):
    """Make the request handler for one host.

    Args:
        releases: the releases served.
        role: "main" (GitHub's own host: redirects a download) or "asset" (the asset host: serves the file).
        asset_base: the asset host's address the main host redirects to, as scheme://host:port.

    Returns:
        A BaseHTTPRequestHandler class.
    """

    class Handler(http.server.BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"
        # A unit gone quiet (restarted, or flashed mid-request) is dropped, not waited for
        timeout = CONNECTION_TIMEOUT_S
        server_version = "OpenTricklerOtaServer"

        def log_message(self, format, *args):
            """Every request is printed by send_done instead."""

        def send_done(self, status: int, sent: int):
            """Print one line for a request answered.

            Args:
                status: the HTTP status sent.
                sent: the body's bytes sent.
            """
            print(f"{role:5} {self.client_address[0]} {self.command} {self.path} -> {status}, {sent} bytes", flush=True)

        def send_status(self, status: int, location: str | None = None):
            """Answer with no body: a redirect, or an error.

            Args:
                status: the HTTP status.
                location: where a redirect leads, or None.
            """
            self.send_response(status)
            if location:
                self.send_header("Location", location)
            self.send_header("Content-Length", "0")
            self.end_headers()
            self.send_done(status, 0)

        def do_HEAD(self):
            """A HEAD is answered as a GET with no body."""
            self.answer(send_body=False)

        def do_GET(self):
            """A GET of a release's file."""
            self.answer(send_body=True)

        def answer(self, send_body: bool):
            """Redirect a download to the asset host, or serve the file from it.

            Args:
                send_body: False for a HEAD.
            """
            pattern = RELEASE_PATH if role == "main" else ASSET_PATH
            match = pattern.match(self.path)
            if match is None:
                self.send_status(404)
                return
            tag, path = releases.resolve(match.group("tag"), match.group("file"))
            if path is None:
                self.send_status(404)
                return
            if role == "main":
                self.send_status(302, f"{asset_base}/assets/{tag}/{path.name}")
                return
            body = path.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            if send_body:
                self.wfile.write(body)
            self.send_done(200, len(body) if send_body else 0)

    return Handler


def openssl(*args: str, cwd: Path):
    """Run openssl, failing loudly.

    Args:
        args: its arguments.
        cwd: the folder it runs in.
    """
    subprocess.run(["openssl", *args], cwd=cwd, check=True, stdout=subprocess.DEVNULL)


def make_key(spec: str, out: str, cwd: Path):
    """Write a private key.

    Args:
        spec: an EC curve's name, or an RSA key's bits.
        out: the key file's name.
        cwd: the folder it is written in.
    """
    if spec.isdigit():
        openssl("genrsa", "-out", out, spec, cwd=cwd)
    else:
        openssl("ecparam", "-name", spec, "-genkey", "-noout", "-out", out, cwd=cwd)


def make_chain(name: str, hosts: list, cert_dir: Path):
    """Write one test chain: a root, an intermediate and a server leaf for the given addresses.

    Args:
        name: "ecc" or "rsa" (CHAINS).
        hosts: the addresses and names the leaf is for.
        cert_dir: the folder written into: <name>_root.pem, <name>_server.pem (leaf then intermediate), <name>_key.pem.
    """
    chain = CHAINS[name]
    san = ",".join(f"IP:{host}" if re.fullmatch(r"[\d.]+", host) else f"DNS:{host}" for host in hosts)
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        (work / "ca.ext").write_text("basicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign,cRLSign\n"
                                     "subjectKeyIdentifier=hash\nauthorityKeyIdentifier=keyid\n", encoding="ascii")
        (work / "leaf.ext").write_text(f"basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,"
                                       f"keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName={san}\n"
                                       "subjectKeyIdentifier=hash\nauthorityKeyIdentifier=keyid\n", encoding="ascii")
        make_key(chain["root_key"], "root.key", work)
        openssl("req", "-x509", "-new", "-key", "root.key", f"-{chain['root_md']}", "-days", "3650",
                "-subj", f"/CN=OpenTrickler test {name} root", "-addext", "basicConstraints=critical,CA:TRUE",
                "-addext", "keyUsage=critical,keyCertSign,cRLSign", "-out", "root.pem", cwd=work)
        for level, issuer, ext in (("int", "root", "ca.ext"), ("leaf", "int", "leaf.ext")):
            make_key(chain[f"{level}_key"], f"{level}.key", work)
            openssl("req", "-new", "-key", f"{level}.key", "-subj", f"/CN=OpenTrickler test {name} {level}",
                    "-out", f"{level}.csr", cwd=work)
            openssl("x509", "-req", "-in", f"{level}.csr", "-CA", f"{issuer}.pem", "-CAkey", f"{issuer}.key",
                    "-CAcreateserial", f"-{chain[f'{level}_md']}", "-days", "3650", "-extfile", ext,
                    "-out", f"{level}.pem", cwd=work)
        shutil.copyfile(work / "root.pem", cert_dir / f"{name}_root.pem")
        (cert_dir / f"{name}_server.pem").write_bytes((work / "leaf.pem").read_bytes() + (work / "int.pem").read_bytes())
        shutil.copyfile(work / "leaf.key", cert_dir / f"{name}_key.pem")


class TlsServer(http.server.ThreadingHTTPServer):
    """An HTTPS server whose handshakes run in each request's own thread.

    Wrapping the listening socket does the handshake inside accept(), on the one thread that accepts: a unit that
    stops half-way through a handshake (restarted, or flashed) then stops every connection after it. Seen 2026-11-17.
    """

    def __init__(self, address: tuple, handler, context: ssl.SSLContext):
        """Args:
            address: (host, port) to listen on.
            handler: the request handler class.
            context: the TLS context its connections are wrapped in.
        """
        super().__init__(address, handler)
        self.context = context

    def get_request(self) -> tuple:
        """Accept a connection and wrap it, its handshake left to the request's thread (on its first read).

        Returns:
            (the wrapped socket, the client's address).
        """
        sock, address = self.socket.accept()
        sock.settimeout(CONNECTION_TIMEOUT_S)
        return self.context.wrap_socket(sock, server_side=True, do_handshake_on_connect=False), address

    def handle_error(self, request, client_address):
        """A handshake or connection that fails is one line, not a traceback."""
        print(f"error {client_address[0]}: {sys.exc_info()[1]}", flush=True)


def serve(port: int, handler, context: ssl.SSLContext | None) -> http.server.ThreadingHTTPServer:
    """Start one host's server in a thread of its own.

    Args:
        port: the port it listens on, on every address.
        handler: its request handler class.
        context: the TLS context for HTTPS, or None for plain HTTP.

    Returns:
        The server, serving.
    """
    if context is None:
        server = http.server.ThreadingHTTPServer(("0.0.0.0", port), handler)
    else:
        server = TlsServer(("0.0.0.0", port), handler, context)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def tls_context(cert_dir: Path, name: str) -> ssl.SSLContext:
    """The server side of one test chain.

    Args:
        cert_dir: where make_chain wrote it.
        name: "ecc" or "rsa".

    Returns:
        An SSLContext that offers TLS 1.2 and later with that chain.
    """
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(cert_dir / f"{name}_server.pem", cert_dir / f"{name}_key.pem")
    return context


def main() -> int:
    """Make the test certificates, or serve the releases until stopped.

    Returns:
        The process's exit code.
    """
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT, help="the releases, one subfolder per tag")
    parser.add_argument("--latest", help="the tag served as the latest (default: the highest)")
    parser.add_argument("--cert-dir", type=Path, default=DEFAULT_CERT_DIR)
    parser.add_argument("--make-certs", action="store_true", help="write the test chains, then stop")
    parser.add_argument("--host", action="append", default=[],
                        help="an address or name the server is reached at (with --make-certs; may repeat)")
    parser.add_argument("--advertise", help="the address units are redirected to (default: the first --host given "
                                            "to --make-certs, read from the ECC leaf)")
    parser.add_argument("--http-port", type=int, default=8080)
    parser.add_argument("--https-port", type=int, default=8443)
    parser.add_argument("--asset-http-port", type=int, default=8081)
    parser.add_argument("--asset-https-port", type=int, default=8444)
    args = parser.parse_args()

    if args.make_certs:
        if not args.host:
            parser.error("--make-certs needs at least one --host")
        if shutil.which("openssl") is None:
            parser.error("openssl is not on the PATH (Git for Windows carries one: its mingw64\\bin)")
        args.cert_dir.mkdir(parents=True, exist_ok=True)
        for name in CHAINS:
            make_chain(name, args.host + ["localhost"], args.cert_dir)
        (args.cert_dir / "hosts.txt").write_text("\n".join(args.host) + "\n", encoding="ascii")
        print(f"test chains for {', '.join(args.host)} written to {args.cert_dir}; a unit trusts "
              f"{args.cert_dir / 'ecc_root.pem'} and {args.cert_dir / 'rsa_root.pem'}")
        return 0

    hosts_file = args.cert_dir / "hosts.txt"
    if not hosts_file.is_file():
        parser.error(f"no test chains in {args.cert_dir}: run with --make-certs --host <this PC's address> first")
    advertise = args.advertise or hosts_file.read_text(encoding="ascii").split()[0]
    if not args.root.is_dir():
        parser.error(f"no releases folder {args.root}: make it, with one subfolder per tag")

    releases = Releases(args.root, args.latest)
    serve(args.asset_http_port, make_handler(releases, "asset", ""), None)
    serve(args.asset_https_port, make_handler(releases, "asset", ""), tls_context(args.cert_dir, "rsa"))
    serve(args.http_port, make_handler(releases, "main", f"http://{advertise}:{args.asset_http_port}"), None)
    serve(args.https_port, make_handler(releases, "main", f"https://{advertise}:{args.asset_https_port}"),
          tls_context(args.cert_dir, "ecc"))
    print(f"serving {args.root} ({', '.join(releases.tags()) or 'no releases yet'}) as {advertise}: "
          f"http {args.http_port}, https {args.https_port}; assets http {args.asset_http_port}, "
          f"https {args.asset_https_port}", flush=True)
    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        print("stopped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
