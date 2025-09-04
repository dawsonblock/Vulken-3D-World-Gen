"""Configuration validation and management utilities."""

import json
import jsonschema
from pathlib import Path
from typing import Dict, Any, Optional


class ConfigValidator:
    """Validates configuration files against schemas."""
    
    # Schema for world configuration
    WORLD_CONFIG_SCHEMA = {
        "type": "object",
        "properties": {
            "chunk_size": {"type": "integer", "minimum": 8, "maximum": 64},
            "world_height": {"type": "integer", "minimum": 64, "maximum": 512},
            "sea_level": {"type": "integer", "minimum": 0},
            "render_distance": {"type": "integer", "minimum": 1, "maximum": 32},
            "simulation_distance": {"type": "integer", "minimum": 1, "maximum": 16},
            "max_loaded_chunks": {"type": "integer", "minimum": 100, "maximum": 10000},
            "generation_threads": {"type": "integer", "minimum": 1, "maximum": 16},
            "compress_chunks": {"type": "boolean"},
            "async_generation": {"type": "boolean"},
            "world_radius": {"type": "integer", "minimum": 32, "maximum": 2048}
        },
        "required": ["chunk_size", "world_height", "render_distance"],
        "additionalProperties": True
    }
    
    # Schema for training configuration  
    TRAINING_CONFIG_SCHEMA = {
        "type": "object",
        "properties": {
            "learning_rate": {"type": "number", "minimum": 1e-6, "maximum": 1.0},
            "batch_size": {"type": "integer", "minimum": 1, "maximum": 1024},
            "num_envs": {"type": "integer", "minimum": 1, "maximum": 2048},
            "total_timesteps": {"type": "integer", "minimum": 1000},
            "gamma": {"type": "number", "minimum": 0.9, "maximum": 0.999},
            "use_mixed_precision": {"type": "boolean"},
            "save_interval": {"type": "integer", "minimum": 1000},
            "log_interval": {"type": "integer", "minimum": 1}
        },
        "required": ["learning_rate", "num_envs", "total_timesteps"],
        "additionalProperties": True
    }
    
    @classmethod
    def validate_config(cls, config: Dict[str, Any], config_type: str) -> tuple[bool, Optional[str]]:
        """Validate configuration against schema.
        
        Args:
            config: Configuration dictionary to validate
            config_type: Type of config ("world" or "training")
            
        Returns:
            Tuple of (is_valid, error_message)
        """
        try:
            if config_type == "world":
                schema = cls.WORLD_CONFIG_SCHEMA
            elif config_type == "training":
                schema = cls.TRAINING_CONFIG_SCHEMA
            else:
                return False, f"Unknown config type: {config_type}"
                
            jsonschema.validate(config, schema)
            
            # Additional validation logic
            if config_type == "world":
                if config.get("sea_level", 0) >= config.get("world_height", 256):
                    return False, "sea_level must be less than world_height"
                    
                if config.get("simulation_distance", 6) > config.get("render_distance", 8):
                    return False, "simulation_distance should not exceed render_distance"
            
            return True, None
            
        except jsonschema.ValidationError as e:
            return False, f"Validation error: {e.message}"
        except Exception as e:
            return False, f"Unexpected error: {str(e)}"
    
    @classmethod
    def load_and_validate_config(cls, config_path: Path, config_type: str) -> tuple[Optional[Dict], Optional[str]]:
        """Load and validate configuration from file.
        
        Args:
            config_path: Path to configuration file
            config_type: Type of config to validate against
            
        Returns:
            Tuple of (config_dict, error_message)
        """
        try:
            if not config_path.exists():
                return None, f"Config file not found: {config_path}"
                
            with open(config_path, 'r') as f:
                config = json.load(f)
                
            is_valid, error = cls.validate_config(config, config_type)
            if not is_valid:
                return None, error
                
            return config, None
            
        except json.JSONDecodeError as e:
            return None, f"Invalid JSON in config file: {e}"
        except Exception as e:
            return None, f"Error loading config: {e}"
    
    @classmethod
    def get_default_config(cls, config_type: str) -> Dict[str, Any]:
        """Get default configuration for given type."""
        if config_type == "world":
            return {
                "chunk_size": 32,
                "world_height": 256,
                "sea_level": 64,
                "render_distance": 8,
                "simulation_distance": 6,
                "load_distance": 10,
                "max_loaded_chunks": 1000,
                "generation_threads": 4,
                "compress_chunks": True,
                "async_generation": True,
                "world_radius": 512
            }
        elif config_type == "training":
            return {
                "learning_rate": 3e-4,
                "batch_size": 256,
                "num_envs": 256,
                "total_timesteps": 10000000,
                "gamma": 0.99,
                "use_mixed_precision": True,
                "save_interval": 10000,
                "log_interval": 100
            }
        else:
            raise ValueError(f"Unknown config type: {config_type}")


def validate_config_file(config_path: str, config_type: str) -> bool:
    """Standalone function to validate a config file.
    
    Args:
        config_path: Path to config file
        config_type: Type of config ("world" or "training")
        
    Returns:
        True if valid, False otherwise
    """
    config, error = ConfigValidator.load_and_validate_config(Path(config_path), config_type)
    if error:
        print(f"Configuration validation failed: {error}")
        return False
    print(f"Configuration is valid!")
    return True


if __name__ == "__main__":
    import sys
    
    if len(sys.argv) != 3:
        print("Usage: python config_validator.py <config_file> <config_type>")
        print("config_type: 'world' or 'training'")
        sys.exit(1)
    
    config_file = sys.argv[1]
    config_type = sys.argv[2]
    
    if validate_config_file(config_file, config_type):
        sys.exit(0)
    else:
        sys.exit(1)