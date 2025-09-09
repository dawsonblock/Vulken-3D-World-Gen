#!/usr/bin/env python3
import http.server
import socketserver
import mimetypes

# Ensure HTML files are served with correct MIME type
mimetypes.add_type('text/html', '.html')
mimetypes.add_type('text/css', '.css')
mimetypes.add_type('application/javascript', '.js')

class MyHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
        self.send_header('Pragma', 'no-cache')
        self.send_header('Expires', '0')
        super().end_headers()

PORT = 8080
Handler = MyHTTPRequestHandler

with socketserver.TCPServer(("", PORT), Handler) as httpd:
    print(f"🌐 VoxelVK Demo Server running at http://localhost:{PORT}")
    print("📁 Available demos:")
    print(f"   - http://localhost:{PORT}/test_viewer.html")
    print(f"   - http://localhost:{PORT}/simple_world_demo.html")
    print(f"   - http://localhost:{PORT}/world_viewer.html")
    print("🔄 Use Ctrl+C to stop server")
    httpd.serve_forever()
