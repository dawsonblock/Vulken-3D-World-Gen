#!/usr/bin/env python3
import os
import json

try:
    import faiss  # type: ignore
except Exception:
    faiss = None

try:
    from sentence_transformers import SentenceTransformer  # type: ignore
except Exception:
    SentenceTransformer = None  # type: ignore

import numpy as np

KB_DIR = os.path.join(os.path.dirname(__file__), '..', 'data', 'rag_kb')
OUT_DIR = os.path.join(os.path.dirname(__file__), '..', 'data')
INDEX_PATH = os.path.join(OUT_DIR, 'rag_index.faiss')
DOCS_PATH = os.path.join(OUT_DIR, 'rag_documents.txt')
MODEL_NAME = os.environ.get('RAG_EMBED_MODEL', 'all-MiniLM-L6-v2')

SAMPLE_DOCS = {
    'desert_architecture.txt': (
        'Buildings in the desert often use sandstone and have thick walls to keep cool. '
        'Roofs are typically flat; courtyards and shaded walkways are common. '
        'Palm wood and adobe are frequent materials. Water collection systems via cisterns are practical.'
    ),
    'elven_style.txt': (
        'Elven architecture features graceful curves, slender towers, and integration with nature. '
        'Structures often weave around ancient trees; materials include white stone, living wood, and crystal. '
        'Light bridges and leaf-patterned roofs are hallmark elements.'
    ),
}


def ensure_sample_kb():
    os.makedirs(KB_DIR, exist_ok=True)
    if not any(fname.endswith('.txt') for fname in os.listdir(KB_DIR)):
        for name, text in SAMPLE_DOCS.items():
            with open(os.path.join(KB_DIR, name), 'w') as f:
                f.write(text)


def build_embeddings(docs):
    if SentenceTransformer is None:
        print('[WARN] sentence-transformers not installed. Using random embeddings as placeholder.')
        dim = 384
        emb = np.random.rand(len(docs), dim).astype('float32')
        return emb
    model = SentenceTransformer(MODEL_NAME)
    emb = model.encode(docs, convert_to_tensor=False, show_progress_bar=True)
    emb = np.array(emb, dtype='float32')
    return emb


def main():
    ensure_sample_kb()
    docs = []
    for fname in sorted(os.listdir(KB_DIR)):
        if not fname.endswith('.txt'):
            continue
        with open(os.path.join(KB_DIR, fname), 'r') as f:
            docs.append(f.read())

    if not docs:
        print('[ERROR] No documents found in', KB_DIR)
        return 1

    emb = build_embeddings(docs)
    os.makedirs(OUT_DIR, exist_ok=True)

    # Save FAISS index if available
    if faiss is not None:
        index = faiss.IndexFlatL2(emb.shape[1])
        index.add(emb)
        faiss.write_index(index, INDEX_PATH)
        print('[OK] Wrote FAISS index to', INDEX_PATH)
    else:
        print('[WARN] faiss not installed. Skipping index file; fallback keyword retrieval will be used.')

    # Save documents
    with open(DOCS_PATH, 'w') as f:
        for doc in docs:
            f.write(doc.replace('\n', '<NEWLINE>') + '\n')
    print('[OK] Wrote documents to', DOCS_PATH)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
