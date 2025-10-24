#!/bin/bash
set -e

echo "Deploying to M5 device..."
pio run --target upload

echo ""
echo "Deployment complete!"
echo "Run 'pio device monitor' to view serial output"
