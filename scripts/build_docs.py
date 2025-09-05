#!/usr/bin/env python3
"""
VoxelVK Documentation Builder

Builds API documentation using Doxygen and creates a documentation site.
"""

import os
import sys
import subprocess
import shutil
import argparse
from pathlib import Path

def run_command(cmd, cwd=None, check=True):
    """Run a command and return the result"""
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd, check=check, capture_output=True, text=True)
    
    if result.stdout:
        print(result.stdout)
    if result.stderr:
        print(result.stderr, file=sys.stderr)
        
    return result

def check_dependencies():
    """Check if required tools are available"""
    dependencies = {
        'doxygen': 'Doxygen documentation generator',
        'dot': 'Graphviz for diagrams (optional)'
    }
    
    missing = []
    for tool, description in dependencies.items():
        if not shutil.which(tool):
            missing.append(f"{tool}: {description}")
            
    if missing:
        print("Missing dependencies:")
        for dep in missing:
            print(f"  - {dep}")
        print("\nInstall missing dependencies:")
        print("  Ubuntu: sudo apt install doxygen graphviz")
        print("  macOS: brew install doxygen graphviz")
        print("  Windows: choco install doxygen.install graphviz")
        return False
        
    return True

def build_doxygen_docs(project_root, output_dir):
    """Build Doxygen documentation"""
    doxyfile = project_root / "Doxyfile"
    
    if not doxyfile.exists():
        print(f"Error: Doxyfile not found at {doxyfile}")
        return False
        
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    # Run Doxygen
    try:
        result = run_command(['doxygen', str(doxyfile)], cwd=project_root)
        
        # Check if documentation was generated
        html_dir = output_dir / 'html'
        if html_dir.exists() and (html_dir / 'index.html').exists():
            print(f"✓ Doxygen documentation generated in {html_dir}")
            return True
        else:
            print("✗ Doxygen documentation generation failed")
            return False
            
    except subprocess.CalledProcessError as e:
        print(f"Error running Doxygen: {e}")
        return False

def create_index_page(docs_dir, project_root):
    """Create main documentation index page"""
    index_content = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>VoxelVK Documentation</title>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            line-height: 1.6;
            max-width: 1200px;
            margin: 0 auto;
            padding: 40px 20px;
            color: #333;
        }}
        
        .header {{
            text-align: center;
            margin-bottom: 60px;
            padding: 40px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border-radius: 12px;
        }}
        
        .header h1 {{
            margin: 0 0 10px 0;
            font-size: 3em;
            font-weight: 700;
        }}
        
        .header p {{
            margin: 0;
            font-size: 1.2em;
            opacity: 0.9;
        }}
        
        .docs-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 30px;
            margin: 40px 0;
        }}
        
        .doc-card {{
            background: white;
            border: 1px solid #e2e8f0;
            border-radius: 12px;
            padding: 30px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            transition: transform 0.2s, box-shadow 0.2s;
        }}
        
        .doc-card:hover {{
            transform: translateY(-2px);
            box-shadow: 0 8px 15px rgba(0, 0, 0, 0.15);
        }}
        
        .doc-card h3 {{
            margin: 0 0 15px 0;
            color: #667eea;
            font-size: 1.4em;
        }}
        
        .doc-card p {{
            color: #666;
            margin-bottom: 20px;
        }}
        
        .doc-card a {{
            display: inline-block;
            background: #667eea;
            color: white;
            text-decoration: none;
            padding: 12px 24px;
            border-radius: 6px;
            font-weight: 500;
            transition: background 0.2s;
        }}
        
        .doc-card a:hover {{
            background: #5a67d8;
        }}
        
        .footer {{
            text-align: center;
            margin-top: 60px;
            padding-top: 40px;
            border-top: 1px solid #e2e8f0;
            color: #666;
        }}
    </style>
</head>
<body>
    <div class="header">
        <h1>VoxelVK Documentation</h1>
        <p>High-performance voxel rendering engine built with Vulkan</p>
    </div>
    
    <div class="docs-grid">
        <div class="doc-card">
            <h3>🚀 Getting Started</h3>
            <p>Quick start guide, installation instructions, and basic usage examples.</p>
            <a href="../README.md">View Guide</a>
        </div>
        
        <div class="doc-card">
            <h3>🏗️ Architecture</h3>
            <p>Detailed system architecture, rendering pipeline, and design decisions.</p>
            <a href="architecture.html">View Architecture</a>
        </div>
        
        <div class="doc-card">
            <h3>📚 API Reference</h3>
            <p>Complete API documentation generated from source code comments.</p>
            <a href="api/html/index.html">Browse API</a>
        </div>
        
        <div class="doc-card">
            <h3>⚙️ Runtime Guide</h3>
            <p>Build presets, environment variables, and troubleshooting information.</p>
            <a href="../RUN.md">View Guide</a>
        </div>
        
        <div class="doc-card">
            <h3>🧪 Testing</h3>
            <p>Testing framework, unit tests, integration tests, and benchmarks.</p>
            <a href="TESTING.html">View Testing</a>
        </div>
        
        <div class="doc-card">
            <h3>🤝 Contributing</h3>
            <p>Guidelines for contributing code, reporting issues, and development workflow.</p>
            <a href="../CONTRIBUTING.md">View Guidelines</a>
        </div>
    </div>
    
    <div class="footer">
        <p>VoxelVK v0.6.0 | <a href="https://github.com/voxelvk/voxelvk">GitHub</a> | Built with ❤️</p>
    </div>
</body>
</html>"""
    
    index_file = docs_dir / 'index.html'
    with open(index_file, 'w') as f:
        f.write(index_content)
        
    print(f"✓ Documentation index created at {index_file}")

def convert_markdown_to_html(md_file, output_dir):
    """Convert markdown files to HTML (simple conversion)"""
    try:
        # Try to use pandoc if available
        if shutil.which('pandoc'):
            html_file = output_dir / (md_file.stem + '.html')
            run_command([
                'pandoc', str(md_file), 
                '-o', str(html_file),
                '--standalone',
                '--css', 'style.css',
                '--metadata', f'title={md_file.stem.replace("_", " ").title()}'
            ])
            return True
    except subprocess.CalledProcessError:
        pass
        
    # Fallback: simple markdown to HTML conversion
    with open(md_file, 'r') as f:
        content = f.read()
        
    # Very basic markdown conversion
    html_content = f"""<!DOCTYPE html>
<html>
<head>
    <title>{md_file.stem.replace("_", " ").title()}</title>
    <style>
        body {{ font-family: system-ui, sans-serif; max-width: 800px; margin: 0 auto; padding: 40px 20px; }}
        code {{ background: #f5f5f5; padding: 2px 4px; border-radius: 3px; }}
        pre {{ background: #f5f5f5; padding: 16px; border-radius: 6px; overflow-x: auto; }}
    </style>
</head>
<body>
<pre>{content}</pre>
</body>
</html>"""
    
    html_file = output_dir / (md_file.stem + '.html')
    with open(html_file, 'w') as f:
        f.write(html_content)
        
    return True

def build_documentation(project_root, clean=False):
    """Build complete documentation"""
    project_root = Path(project_root).resolve()
    docs_dir = project_root / 'docs'
    api_dir = docs_dir / 'api'
    
    print(f"Building documentation for {project_root}")
    
    # Clean previous build
    if clean and api_dir.exists():
        print("Cleaning previous build...")
        shutil.rmtree(api_dir)
        
    # Check dependencies
    if not check_dependencies():
        return False
        
    # Build Doxygen documentation
    if not build_doxygen_docs(project_root, api_dir):
        return False
        
    # Convert markdown files
    markdown_files = [
        project_root / 'docs' / 'architecture.md',
        project_root / 'docs' / 'TESTING.md',
    ]
    
    for md_file in markdown_files:
        if md_file.exists():
            convert_markdown_to_html(md_file, docs_dir)
            
    # Create main index page
    create_index_page(docs_dir, project_root)
    
    print(f"\n✓ Documentation build complete!")
    print(f"  Open: {docs_dir / 'index.html'}")
    print(f"  API:  {api_dir / 'html' / 'index.html'}")
    
    return True

def main():
    parser = argparse.ArgumentParser(description='Build VoxelVK documentation')
    parser.add_argument('--project-root', default='.', 
                       help='Project root directory')
    parser.add_argument('--clean', action='store_true',
                       help='Clean previous build')
    parser.add_argument('--serve', action='store_true',
                       help='Serve documentation locally')
    parser.add_argument('--port', type=int, default=8000,
                       help='Port for local server')
    
    args = parser.parse_args()
    
    # Build documentation
    if not build_documentation(args.project_root, args.clean):
        sys.exit(1)
        
    # Serve documentation locally
    if args.serve:
        docs_dir = Path(args.project_root) / 'docs'
        print(f"\nServing documentation at http://localhost:{args.port}")
        print("Press Ctrl+C to stop")
        
        try:
            import http.server
            import socketserver
            
            os.chdir(docs_dir)
            handler = http.server.SimpleHTTPRequestHandler
            with socketserver.TCPServer(("", args.port), handler) as httpd:
                httpd.serve_forever()
                
        except KeyboardInterrupt:
            print("\nServer stopped")
        except ImportError:
            print("Python http.server not available")

if __name__ == '__main__':
    main()