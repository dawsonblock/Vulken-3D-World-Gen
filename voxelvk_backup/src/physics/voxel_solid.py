
from typing import Dict
# Adapt this to your engine's block registry
class BlockType:
    AIR = 0

BLOCK_PROPERTIES: Dict[int, dict] = {}

def is_solid(block_type: int) -> bool:
    try:
        props = BLOCK_PROPERTIES.get(block_type, {})
        return bool(props.get("solid", block_type != BlockType.AIR))
    except Exception:
        return block_type != BlockType.AIR
