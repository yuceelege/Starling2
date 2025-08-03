#!/bin/bash

source common
echo "Image: "$IMAGE_NAME

echo "Waiting for device..."
adb wait-for-device

echo "Checking for docker support..."
DOCKER_ACTIVE=$(adb shell systemctl is-active docker-start)
if [[ $DOCKER_ACTIVE == *active* ]]; then
	echo "-> Docker support is running on target."
else
	echo "-> Docker support not detected, running docker support on target..."
	adb shell voxl-configure-docker-support.sh
fi

echo "Building image: $IMAGE_NAME"
adb shell mkdir -p $TARGET_DIR
adb push Dockerfile $TARGET_DIR
adb push voxl2_takeoff_land.py $TARGET_DIR
adb push check_status.py $TARGET_DIR
adb push vicon_test.py $TARGET_DIR
adb push offboard_8_figure.py $TARGET_DIR
adb push offboard_attitude.py $TARGET_DIR
adb push offboard_position_velocity_ned.py $TARGET_DIR
adb push offboard_velocity_body.py $TARGET_DIR
adb push offboard_velocity_ned.py $TARGET_DIR
adb push neural_control.py $TARGET_DIR

adb push start-mavsdk-server.sh $TARGET_DIR
adb push run-docker.sh $TARGET_DIR

adb push mpa_reader.c $TARGET_DIR
adb push mpa_pipe_reader.py $TARGET_DIR
adb push mpa_writer.c $TARGET_DIR

adb push libmodal-json_0.4.7_arm64.deb  $TARGET_DIR
adb push libvoxl-cutils_0.1.5_arm64.deb  $TARGET_DIR
adb push libmodal-pipe_2.13.2_arm64.deb  $TARGET_DIR
adb push qti-gbm_18.0.0-rc5_arm64.deb  $TARGET_DIR


adb shell docker build -t $IMAGE_NAME $TARGET_DIR
adb shell rm -rf $TARGET_DIR/voxl2_takeoff_land.py
adb push run-docker.sh $TARGET_DIR
