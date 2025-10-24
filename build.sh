#!/bin/bash
set -e

echo "Building M5 Platform Game..."
pio run

echo ""
echo "Build complete!"
echo "Run ./deploy.sh to upload to device"
