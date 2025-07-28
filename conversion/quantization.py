import tensorflow as tf

converter = tf.lite.TFLiteConverter.from_saved_model("gatenet_classifier_tf")
converter.experimental_new_converter = True
converter.allow_custom_ops = True
converter.target_spec.supported_types = [tf.float16]
tflite_model = converter.convert()
with open("gatenet_classifier_fp16.tflite", "wb") as f:
    f.write(tflite_model)
