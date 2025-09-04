#!/usr/bin/env bash
# Compose helper for headless training inside container

set -euo pipefail

WORLD_CONFIG=${WORLD_CONFIG:-"./config/world.yaml"}
TRAINING_CONFIG=${TRAINING_CONFIG:-"./config/training.yaml"}
OUTPUT_DIR=${OUTPUT_DIR:-"./output"}

export WORLD_CONFIG TRAINING_CONFIG OUTPUT_DIR

echo "WORLD_CONFIG=$WORLD_CONFIG"
echo "TRAINING_CONFIG=$TRAINING_CONFIG"
echo "OUTPUT_DIR=$OUTPUT_DIR"

docker compose up --build headless-trainer
