import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'

from flask import Flask, request, jsonify
import tensorflow as tf
import numpy as np
from PIL import Image, ImageFilter
import io
import time

app = Flask(__name__)

# ===== CONFIG =====
DEBUG_SAVE = True
DEBUG_DIR = "debug_images"
os.makedirs(DEBUG_DIR, exist_ok=True)

# ===== LOAD MODEL =====
print("🚀 Loading model...")
interpreter = tf.lite.Interpreter(model_path="mobnet_model_quantized.tflite")
interpreter.allocate_tensors()

input_details = interpreter.get_input_details()
output_details = interpreter.get_output_details()

INPUT_DTYPE = input_details[0]['dtype']
INPUT_SCALE, INPUT_ZERO_POINT = input_details[0]['quantization']

print("✅ Model loaded")
print("Input dtype:", INPUT_DTYPE)
print("Quantization:", (INPUT_SCALE, INPUT_ZERO_POINT))

# ===== LABELS =====
labels = ["cardboard", "glass", "metal", "paper", "plastic", "trash"]

# ===== BUFFER =====
buffer_predictions = []
FRAME_BUFFER_SIZE = 3

# ===== PREPROCESS =====
def preprocess_image(image_bytes, debug_id):
    image = Image.open(io.BytesIO(image_bytes)).convert('RGB')

    # SAVE RAW IMAGE
    if DEBUG_SAVE:
        image.save(f"{DEBUG_DIR}/{debug_id}_raw.jpg")

    # ===== CROP =====
    w, h = image.size
    image = image.crop((
        int(w * 0.2),
        int(h * 0.2),
        int(w * 0.8),
        int(h * 0.8)
    ))

    # ===== RESIZE =====
    image = image.resize((224, 224))

    # ===== DENOISE =====
    image = image.filter(ImageFilter.SMOOTH)

    image_array = np.asarray(image).astype(np.float32)

    # ===== CONTRAST BOOST =====
    image_array = np.clip(image_array * 1.3, 0, 255)

    # SAVE PROCESSED IMAGE (what model sees)
    if DEBUG_SAVE:
        debug_img = Image.fromarray(image_array.astype(np.uint8))
        debug_img.save(f"{DEBUG_DIR}/{debug_id}_processed.jpg")

    # ===== NORMALIZATION =====
    if INPUT_DTYPE == np.uint8:
        image_array = (image_array / INPUT_SCALE + INPUT_ZERO_POINT).astype(np.uint8)
    else:
        image_array = (image_array / 255.0).astype(np.float32)

    return np.expand_dims(image_array, axis=0)


# ===== PREDICT =====
@app.route('/predict', methods=['POST'])
def predict():
    global buffer_predictions

    try:
        start_time = time.time()
        debug_id = str(int(time.time() * 1000))

        image_bytes = request.get_data()

        if not image_bytes:
            return jsonify({'error': 'No image received'}), 400

        print("\n==============================")
        print(f"📥 Received: {len(image_bytes)} bytes | ID: {debug_id}")

        # ===== PREPROCESS =====
        input_data = preprocess_image(image_bytes, debug_id)

        # ===== INFERENCE =====
        interpreter.set_tensor(input_details[0]['index'], input_data)
        interpreter.invoke()
        prediction = interpreter.get_tensor(output_details[0]['index'])[0]

        print("🔢 Raw Prediction:", prediction)

        # ===== BUFFER =====
        buffer_predictions.append(prediction)
        if len(buffer_predictions) > FRAME_BUFFER_SIZE:
            buffer_predictions.pop(0)

        if len(buffer_predictions) < FRAME_BUFFER_SIZE:
            print("⏳ Collecting frames...")
            return jsonify({"status": "collecting"})

        avg_pred = np.mean(buffer_predictions, axis=0)

        # ===== TOP 3 =====
        top_indices = avg_pred.argsort()[-3:][::-1]

        print("🏆 Top Predictions:")
        for i in top_indices:
            print(f"   {labels[i]} → {avg_pred[i]:.4f}")

        idx = int(np.argmax(avg_pred))
        class_name = labels[idx]
        confidence = float(avg_pred[idx])

        elapsed = (time.time() - start_time) * 1000

        print(f"✅ FINAL: {class_name} ({confidence:.2%})")
        print(f"⏱ Time: {elapsed:.1f} ms")

        return jsonify({
            "class": class_name,
            "confidence": confidence,
            "top3": [
                {labels[i]: float(avg_pred[i])} for i in top_indices
            ]
        })

    except Exception as e:
        print("❌ ERROR:", str(e))
        return jsonify({'error': str(e)}), 500


@app.route('/')
def home():
    return "🚀 Debug Server Running"


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False)