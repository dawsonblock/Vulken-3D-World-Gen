#!/usr/bin/env python3
import redis
import sys
import os

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <mesh_id> <file_path>")
        sys.exit(1)

    mesh_id = sys.argv[1]
    file_path = sys.argv[2]

    if not os.path.exists(file_path):
        print(f"Error: File not found at '{file_path}'")
        sys.exit(1)

    r = redis.Redis(host='localhost', port=6379, db=0)

    with open(file_path, 'rb') as f:
        file_content = f.read()

    try:
        r.set(f"mesh:{mesh_id}", file_content)
        print(f"Successfully pushed mesh '{mesh_id}' from '{file_path}' to Redis.")
        
        # Publish a notification for hot-reloading
        r.publish("assets.mesh", mesh_id)
        print(f"Published hot-reload notification for '{mesh_id}'.")

    except redis.exceptions.ConnectionError as e:
        print(f"Error connecting to Redis: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
