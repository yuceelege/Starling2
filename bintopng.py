import os
import numpy as np
import cv2

in_f = "raw_capture"
out_f = "raw_capture_image"
os.makedirs(out_f, exist_ok=True)
w, h = 1024, 768

for i,fn in enumerate(os.listdir(in_f)):
    print(i)
    if fn.endswith(".bin"):
        data = np.fromfile(os.path.join(in_f, fn), dtype=np.uint8)
        y = data[:w*h].reshape((h, w))
        uv = data[w*h:].reshape((h//2, w))
        yuv = np.vstack((y, uv))
        bgr = cv2.cvtColor(yuv, cv2.COLOR_YUV2BGR_NV12)
        out = os.path.splitext(fn)[0] + ".png"
        cv2.imwrite(os.path.join(out_f, out), bgr)
