import tensorflow as tf
import numpy as np
from PIL import Image
import sys
import os

# Suppress TensorFlow logs
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'

print("Loading TFLite model... please wait.")

# ==========================================
# LOAD TFLITE MODEL
# ==========================================
interpreter = tf.lite.Interpreter(model_path="mobnet_model_quantized.tflite")
interpreter.allocate_tensors()

input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

print("Model loaded successfully!")

# ==========================================
# LABELS (TrashNet)
# ==========================================
labels = [
    "cardboard",
    "glass",
    "metal",
    "paper",
    "plastic",
    "trash"
]

# ==========================================
# TEST FUNCTION
# ==========================================
def test_image(image_path):
    try:
        # 1. Load image
        image = Image.open(image_path).convert('RGB')

        # 2. Resize
        image = image.resize((224, 224))
        image_array = np.asarray(image)

        # 3. Normalize (IMPORTANT: TrashNet uses 0–1)
        normalized = image_array.astype(np.float32) / 255.0

        # 4. Add batch dimension
        data = np.expand_dims(normalized, axis=0)

        # ==========================================
        # TFLITE INFERENCE
        # ==========================================
        interpreter.set_tensor(input_details[0]['index'], data)
        interpreter.invoke()
        prediction = interpreter.get_tensor(output_details[0]['index'])[0]

        # 5. Get result
        index = np.argmax(prediction)
        class_name = labels[index]
        confidence = float(prediction[index])

        # ==========================================
        # PRINT RESULT
        # ==========================================
        print("\n" + "="*30)
        print("🤖 INFERENCE RESULTS")
        print("="*30)
        print(f"File: {image_path}")
        print(f"Prediction: {class_name}")
        print(f"Confidence: {confidence * 100:.2f}%")
        print("="*30 + "\n")

    except Exception as e:
        print(f"\n❌ Error: {e}\n")


# ==========================================
# ENTRY POINT
# ==========================================
if __name__ == '__main__':
    if len(sys.argv) > 1:
        test_image(sys.argv[1])
    else:
        print("\n⚠️ Usage: python test_local.py <image.jpg>")
        print("Example: python test_local.py test.jpg\n")