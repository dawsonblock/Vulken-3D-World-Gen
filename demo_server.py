#!/usr/bin/env python3
from http.server import SimpleHTTPRequestHandler
import socketserver
import mimetypes
import os
import argparse

# Ensure HTML files are served with correct MIME type
mimetypes.add_type('text/html', '.html')
mimetypes.add_type('text/css', '.css')
mimetypes.add_type('application/javascript', '.js')
mimetypes.add_type('application/json', '.json')


class MyHTTPRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header(
            'Cache-Control',
            'no-cache, no-store, must-revalidate'
        )
        self.send_header('Pragma', 'no-cache')
        self.send_header('Expires', '0')
        super().end_headers()


def main():
    # Parse port from CLI or environment, default 8080
    parser = argparse.ArgumentParser(
        description="VoxelVK Demo HTTP Server"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=int(os.environ.get("PORT", 8080)),
        help="Port to listen on (default: 8080 or $PORT)",
    )
    args = parser.parse_args()

    port = args.port
    Handler = MyHTTPRequestHandler

    # Try up to 10 consecutive ports if the chosen one is busy
    httpd = None
    for attempt in range(10):
        try:
            httpd = socketserver.TCPServer(("", port), Handler)
            break
        except OSError as e:
            # 48 = EADDRINUSE on macOS, 98 on Linux
            if getattr(e, 'errno', None) in (48, 98):
                print(f"⚠️  Port {port} in use; trying {port + 1}...")
                port += 1
                continue
            raise

    if httpd is None:
        raise SystemExit("Failed to bind any port after multiple attempts.")

    with httpd:
        base_url = f"http://localhost:{port}"
        print(f"🌐 VoxelVK Demo Server running at {base_url}")
        print("📁 Available demos:")
        print(f"   - {base_url}/test_viewer.html")
        print(f"   - {base_url}/simple_world_demo.html")
        print(f"   - {base_url}/world_viewer.html")
        print(f"   - {base_url}/webgl_world_viewer.html")
        print("🔄 Use Ctrl+C to stop server")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\n🛑 Server stopped")


if __name__ == "__main__":
    main()
