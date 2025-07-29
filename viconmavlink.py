#!/usr/bin/env python3

import threading
import time
import numpy as np
from pymavlink import mavutil
from pymavlink.dialects.v20 import common as mavlink20
from vicon_dssdk import ViconDataStream

def quat_conjugate(q):
    q = np.asarray(q, float)
    # conjugate: invert vector part
    r = q * np.array([-1, -1, -1, 1], float)
    return tuple(r.tolist())

def quat_form_change(q):
    q = np.asarray(q, float)
    # reorder from (x, y, z, w) to (w, x, y, z)
    r = q[[3, 0, 1, 2]]
    return tuple(r.tolist())

def quat_mult(a, b):
    a = np.asarray(a, float)
    b = np.asarray(b, float)
    x1, y1, z1, w1 = a
    M = np.array([
        [ w1, -z1,  y1, x1],
        [ z1,  w1, -x1, y1],
        [-y1,  x1,  w1, z1],
        [-x1, -y1, -z1, w1],
    ], float)
    r = M.dot(b)
    return tuple(r.tolist())

def flip_quaternion_flu_to_frd(q):
    q = np.asarray(q, float)
    # FLU-to-FRD mapping
    r = np.array([q[3], -q[2], q[1], -q[0]], float)
    return tuple(r.tolist())

# VOXL 2 connection parameters
VOXL2_IP = "192.168.1.230"
VOXL2_PORT = 14550
subject_segment = "modal"

# MAVLink output
mav_out = mavutil.mavlink_connection(
    f"udpout:{VOXL2_IP}:{VOXL2_PORT}",
    source_system=255,
    dialect="common"
)

def heartbeat_thread():
    while True:
        mav_out.mav.heartbeat_send(
            mavutil.mavlink.MAV_TYPE_GCS,
            mavutil.mavlink.MAV_AUTOPILOT_INVALID,
            0, 0,
            mavutil.mavlink.MAV_STATE_ACTIVE
        )
        time.sleep(1)

threading.Thread(target=heartbeat_thread, daemon=True).start()

# Vicon client setup
client = ViconDataStream.Client()
client.Connect("localhost:801")
client.EnableSegmentData()
client.SetStreamMode(ViconDataStream.Client.StreamMode.EClientPull)

while not client.IsConnected():
    time.sleep(0.1)

# State variables
origin_set = False
initial_pos = None
initial_quat_inv = None
prev_pos = None
prev_time = None

try:
    while True:
        # Get a fresh frame
        while True:
            try:
                client.GetFrame()
                pos_mm, occ_pos = client.GetSegmentGlobalTranslation(subject_segment, subject_segment)
                quat, occ_quat = client.GetSegmentGlobalRotationQuaternion(subject_segment, subject_segment)
                break
            except ViconDataStream.DataStreamException as e:
                if str(e) == "NoFrame":
                    time.sleep(0.01)
                    continue
                else:
                    raise

        # Establish origin once
        if not origin_set and not occ_pos:
            initial_pos = pos_mm
            origin_set = True
            print(f"ORIGIN (mm): {initial_pos}")
            continue

        # Invert initial orientation once
        if initial_quat_inv is None and not occ_quat:
            raw = flip_quaternion_flu_to_frd(quat)
            initial_quat_inv = quat_conjugate(raw)

        # Compute position in meters, FRD frame
        if origin_set:
            pos = (
                (pos_mm[0] - initial_pos[0]) / 1000.0,
                -(pos_mm[1] - initial_pos[1]) / 1000.0,
                -(pos_mm[2] - initial_pos[2]) / 1000.0
            )
        else:
            pos = (0.0, 0.0, 0.0)

        now = time.time()

        # Estimate velocity
        if prev_pos is not None:
            dt = now - prev_time
            if dt > 0:
                vx = (pos[0] - prev_pos[0]) / dt
                vy = (pos[1] - prev_pos[1]) / dt
                vz = (pos[2] - prev_pos[2]) / dt
            else:
                vx = vy = vz = 0.0
        else:
            vx = vy = vz = 0.0

        prev_pos = pos
        prev_time = now

        # Compute relative orientation
        rel_quat = (
            quat_mult(initial_quat_inv, flip_quaternion_flu_to_frd(quat))
            if initial_quat_inv is not None else quat
        )
        final_quat = quat_form_change(rel_quat)

        # Prepare and send MAVLink odometry
        time_usec = int(now * 1e6)
        pose_cov = [1e-9] * 21
        vel_cov  = [1e-9] * 21

        odom = mavlink20.MAVLink_odometry_message(
            time_usec,
            mavlink20.MAV_FRAME_LOCAL_FRD,
            mavlink20.MAV_FRAME_BODY_FRD,
            pos[0], pos[1], pos[2],
            list(final_quat),
            vx, vy, vz,
            0.0, 0.0, 0.0,
            pose_cov,
            vel_cov,
            0,
            mavlink20.MAV_ESTIMATOR_TYPE_VISION,
            1
        )
        mav_out.mav.send(odom)

        time.sleep(0.02)

except KeyboardInterrupt:
    pass
