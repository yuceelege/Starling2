#!/bin/bash

repository="voxl-mavsdk-python"

# Get a list of Docker images and inspect each one
docker_images=$(docker images)

# Loop through the list of images and extract the tag for the specified repository
while read -r line; do
  # Split the line into columns
  columns=($line)
  if [[ "${columns[0]}" == "$repository" ]]; then
    tag_to_find="${columns[1]}"
    break
  fi
done <<< "$docker_images"

VOLUMES="-v /tmp:/tmp -v /etc/modalai:/etc/modalai -v /dev:/dev"

docker_name="$repository:$tag_to_find"
docker run -it --rm --privileged --net=host --ipc=host -v /run/mpa:/run/mpa ${VOLUMES} $docker_name

