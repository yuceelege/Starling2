#!/bin/bash

# Variables
LOCAL_WORKSPACE_DIR="."  # Replace with your local workspace path
REMOTE_USER="root"                   # Replace with your remote server username
REMOTE_HOST="192.168.122.115"                # Replace with your remote server hostname or IP
REMOTE_DIR="~/"        # Replace with the directory on the remote server
PASSWORD="oelinux123"
SUBDIRECTORIES=("src" "include")
# Sync workspace to remote server
for SUBDIR in "${SUBDIRECTORIES[@]}"; do
  sshpass -p "$PASSWORD" rsync -avz --delete -e "ssh -o StrictHostKeyChecking=no" \
  rsync -avz --delete "$LOCAL_WORKSPACE_DIR/$SUBDIR" "$REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR"
done

# Check if rsync was successful
if [ $? -eq 0 ]; then
  echo "Workspace successfully synced to the remote server."
else
  echo "Failed to sync workspace."
fi
