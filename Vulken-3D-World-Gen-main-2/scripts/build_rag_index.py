#!/usr/bin/env python3
"""
RAG Index Builder for VoxelVK
Processes knowledge base files and creates search indices
"""

import os
import sys
import argparse
from pathlib import Path
import re

def clean_text(text):
    """Clean and normalize text content"""
    # Remove extra whitespace
    text = re.sub(r'\s+', ' ', text.strip())
    # Remove very short fragments
    if len(text) < 20:
        return ""
    return text

def process_knowledge_base(kb_dir="data/rag_kb", output_dir="data"):
    """Process knowledge base files into searchable documents"""
    kb_path = Path(kb_dir)
    output_path = Path(output_dir)
    
    # Create output directory
    output_path.mkdir(parents=True, exist_ok=True)
    
    if not kb_path.exists():
        print(f"Creating knowledge base directory: {kb_path}")
        kb_path.mkdir(parents=True, exist_ok=True)
        
        # Create sample knowledge base files
        sample_files = {
            "elven_architecture.txt": """
Elven architecture emphasizes organic curves and natural integration.
Elven bridges use slender pointed arches that seem to grow from stone.
Materials favor pale stone, silver inlays, and living wood integration.
Elven structures often incorporate flowing water features.
            """,
            "dwarven_construction.txt": """
Dwarven architecture focuses on robust, geometric construction.
Dwarven halls feature massive stone blocks and metal reinforcement.
Materials include granite, iron, bronze, and precious metal accents.
Dwarven engineering prioritizes structural integrity and permanence.
            """,
            "human_settlements.txt": """
Human architecture varies by region and available materials.
Human buildings balance functionality with aesthetic appeal.
Materials include timber, brick, stone, and regional variations.
Human settlements adapt to local geography and climate conditions.
            """
        }
        
        for filename, content in sample_files.items():
            (kb_path / filename).write_text(content.strip())
        
        print(f"Created sample knowledge base files in {kb_path}")
    
    documents = []
    source_files = []
    
    # Process all text files in knowledge base
    for file_path in kb_path.glob("*.txt"):
        try:
            content = file_path.read_text(encoding='utf-8')
            cleaned = clean_text(content)
            
            if cleaned:
                documents.append(cleaned)
                source_files.append(file_path.name)
                print(f"Processed: {file_path.name} ({len(cleaned)} chars)")
            
        except Exception as e:
            print(f"Error processing {file_path}: {e}")
    
    if not documents:
        print("No documents found in knowledge base")
        return False
    
    # Write documents file
    documents_file = output_path / "rag_documents.txt"
    with open(documents_file, 'w', encoding='utf-8') as f:
        for i, doc in enumerate(documents):
            f.write(f"DOC_{i:04d}|{source_files[i]}|{doc}\n")
    
    print(f"Created documents file: {documents_file} ({len(documents)} documents)")
    
    # Try to create FAISS index if available
    try:
        import numpy as np
        try:
            import faiss
            from sentence_transformers import SentenceTransformer
            
            print("Creating FAISS index with sentence-transformers...")
            
            # Initialize embedding model
            model = SentenceTransformer('all-MiniLM-L6-v2')
            
            # Generate embeddings
            embeddings = model.encode(documents)
            embeddings = embeddings.astype(np.float32)
            
            # Create FAISS index
            dimension = embeddings.shape[1]
            index = faiss.IndexFlatIP(dimension)  # Inner product for similarity
            
            # Normalize embeddings for cosine similarity
            faiss.normalize_L2(embeddings)
            index.add(embeddings)
            
            # Save index
            index_file = output_path / "rag_index.faiss"
            faiss.write_index(index, str(index_file))
            
            print(f"Created FAISS index: {index_file} ({dimension}D, {len(documents)} vectors)")
            
        except ImportError:
            print("FAISS or sentence-transformers not available - using keyword fallback only")
            
    except ImportError:
        print("NumPy not available - using keyword fallback only")
    
    return True

def main():
    parser = argparse.ArgumentParser(description="Build RAG index for VoxelVK")
    parser.add_argument("--kb-dir", default="data/rag_kb", help="Knowledge base directory")
    parser.add_argument("--output-dir", default="data", help="Output directory")
    
    args = parser.parse_args()
    
    print("VoxelVK RAG Index Builder")
    print("=" * 30)
    
    success = process_knowledge_base(args.kb_dir, args.output_dir)
    
    if success:
        print("\n✅ RAG index building complete!")
        print("Usage:")
        print("  1. Include PromptAssembler.hpp in your code")
        print("  2. Use RAGQueryEngine to search the index")
        print("  3. Assemble prompts with retrieved context")
    else:
        print("\n❌ RAG index building failed")
        sys.exit(1)

if __name__ == "__main__":
    main()