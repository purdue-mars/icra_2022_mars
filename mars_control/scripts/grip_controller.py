#!/usr/bin/env python3

import rospy
from wsg_50_common.msg import Cmd, Status
from numpy as np

POS_MIN = 3.0 # mm (Point where gelsight fingers touch) 
POS_MAX = 68.0 # mm (Physical limit)
POS_MARGIN = 1.0 # mm
MOVE_SPEED = 50.0 # mm/s (Default 50mm/s)

friction = None
def friction_cb(msg):
    global friction

    ref = np.array(msg.ref_markers.data)
    ref = ref.reshape((ref.shape[0]//2, 2))
    cur = np.array(msg.cur_markers.data)
    cur = cur.reshape((cur.shape[0]//2, 2))

    friction = np.mean(cur - ref, axis=1)

cur_pos = None
def pos_cb(msg: Cmd):
    global cur_pos
    cur_pos = msg.width

if __name__ == "__main__":
    rospy.init_node("gripper_controller")
    rate = rospy.Rate(10)

    rospy.Subscriber("/wsg_50_driver/status", Status, pos_cb)
    pub = rospy.Publisher("/wsg_50_driver/goal_position", Cmd, queue_size=1, latch=True)

    while not rospy.is_shutdown():
        print(friction) 
        rate.sleep()