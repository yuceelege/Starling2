#!/home/yuceelege/miniconda3/envs/drone/bin/python
import onnx
from onnx_tf.backend import prepare
import tensorflow as tf

onnx_model = onnx.load("gatenet_classifier.onnx")
tf_rep = prepare(onnx_model, strict=False)
tf_rep.export_graph("gatenet_classifier_tf")

converter = tf.lite.TFLiteConverter.from_saved_model("gatenet_classifier_tf")
converter.experimental_new_converter = True
converter.allow_custom_ops = True
converter.target_spec.supported_types = [tf.float16]
tflite_model = converter.convert()

with open("gatenet_classifier.tflite", "wb") as f:
    f.write(tflite_model)
