"""
serverlocal.py - Servidor ML para LSEC con K-Nearest Neighbors
==============================================================
Clasifica las señas detectando la distancia generalizada 
matemática con respecto al archivo 'dataset.json' pre-grabado 
por el propio usuario.
"""

import cv2
import mediapipe as mp
import threading
import time
import os
import urllib.request
import json
import collections
import numpy as np
from flask import Flask, jsonify

app = Flask(__name__)
current_letter = "ninguno"

MODEL_PATH = os.path.join(os.path.dirname(__file__), "hand_landmarker.task")
DATASET_FILE = os.path.join(os.path.dirname(__file__), "dataset.json")

# Smoothing
letter_buffer = collections.deque(maxlen=10)

def normalize_landmarks(landmarks):
    base_x = landmarks[0].x
    base_y = landmarks[0].y
    base_z = landmarks[0].z

    norm_vector = []
    for lm in landmarks:
        norm_vector.append(lm.x - base_x)
        norm_vector.append(lm.y - base_y)
        norm_vector.append(lm.z - base_z)
    return norm_vector

def knn_predict(current_vector, dataset):
    """
    Busca la letra en 'dataset' cuyo vector sea estructuralmente 
    más idéntico al 'current_vector' usando Distancia Euclidiana (KNN k=1).
    """
    if not dataset:
        return "Falta Entrenar"

    best_letter = "ninguno"
    min_dist = float('inf')
    
    cv = np.array(current_vector)

    for letter, saved_vector in dataset.items():
        sv = np.array(saved_vector)
        # Distancia euclidiana entre arreglos de 63 dimensiones
        dist = np.linalg.norm(cv - sv)
        
        if dist < min_dist:
            min_dist = dist
            best_letter = letter

    # Si la distancia es anormalmente alta, la mano no está haciendo ninguna seña conocida
    # (Umbral de confianza empírico para LSEC)
    if min_dist > 1.2: 
        return "ninguno"
        
    return best_letter

def get_smoothed_letter(letter):
    if letter != "ninguno" and letter != "Falta Entrenar":
        letter_buffer.append(letter)
    else:
        if len(letter_buffer) > 0:
            letter_buffer.popleft() 
        return letter
        
    if len(letter_buffer) < 5:
        return letter_buffer[-1]
        
    freq = collections.Counter(letter_buffer)
    most_common = freq.most_common(1)[0]
    
    # 50% de consistencia
    if most_common[1] >= len(letter_buffer) // 2:
        return most_common[0]
    return "ninguno"


def camera_thread():
    global current_letter

    # Cargar Dataset (En tiempo real por si el usuario graba algo nuevo)
    dataset = {}
    last_modified = 0

    BaseOptions = mp.tasks.BaseOptions
    HandLandmarker = mp.tasks.vision.HandLandmarker
    HandLandmarkerOptions = mp.tasks.vision.HandLandmarkerOptions
    VisionRunningMode = mp.tasks.vision.RunningMode

    options = HandLandmarkerOptions(
        base_options=BaseOptions(model_asset_path=MODEL_PATH),
        running_mode=VisionRunningMode.VIDEO,
        num_hands=1,
        min_hand_detection_confidence=0.7,
        min_hand_presence_confidence=0.7,
        min_tracking_confidence=0.5,
    )

    cap = cv2.VideoCapture(0)
    print("\n[OK] Servidor de Reconocimiento ML Iniciado. Presiona 'q' para salir.\n")

    timestamp_ms = 0
    with HandLandmarker.create_from_options(options) as landmarker:
        while True:
            # Hot-Reload del dataset si el usuario usa recorder.py
            if os.path.exists(DATASET_FILE):
                mod_time = os.path.getmtime(DATASET_FILE)
                if mod_time > last_modified:
                    try:
                        with open(DATASET_FILE, "r") as f:
                            dataset = json.load(f)
                        last_modified = mod_time
                        print(f"[!] Dataset KNN Recargado ({len(dataset)} letras).")
                    except:
                        pass
                        
            success, image = cap.read()
            if not success:
                time.sleep(0.1)
                continue

            image = cv2.flip(image, 1) # Espejo
            h, w, _ = image.shape

            image_rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=image_rgb)
            
            timestamp_ms += 33
            result = landmarker.detect_for_video(mp_image, timestamp_ms)

            raw_letter = "ninguno"

            if result.hand_landmarks:
                for hand_landmarks in result.hand_landmarks:
                    # 1. Normalizar Mano
                    vector = normalize_landmarks(hand_landmarks)
                    # 2. Predecir con Machine Learning (KNN)
                    raw_letter = knn_predict(vector, dataset)
                    
                    # Dibujar
                    for lm in hand_landmarks:
                        cx, cy = int(lm.x * w), int(lm.y * h)
                        cv2.circle(image, (cx, cy), 3, (255, 0, 0), -1)

            current_letter = get_smoothed_letter(raw_letter)

            # UI
            cv2.rectangle(image, (0,0), (640, 70), (0,0,0), -1)
            cv2.putText(image, f"SEÑA: {current_letter}", (20, 50), cv2.FONT_HERSHEY_SIMPLEX, 1.5, (0, 255, 0), 3)

            cv2.imshow('IA LSEC - K-Nearest Neighbors', image)
            if cv2.waitKey(5) & 0xFF == ord('q'):
                break

    cap.release()
    cv2.destroyAllWindows()
    os._exit(0)


# ------------------------------------------------------------------ #
#  Endpoints Flask
# ------------------------------------------------------------------ #
@app.route("/status/<name>", methods=["GET"])
@app.route("/status", methods=["GET"])
def get_status(name=None):
    global current_letter
    return jsonify({"gesture": current_letter})

@app.route("/register", methods=["POST"])
def register():
    return jsonify({"status": "ok"}), 200

@app.route("/", methods=["GET"])
def index():
    return "<h1>Servidor IA Activo</h1><p>Ve a <a href='/status'>/status</a>.</p>", 200

if __name__ == "__main__":
    t = threading.Thread(target=camera_thread, daemon=True)
    t.start()
    app.run(host="0.0.0.0", port=5002, debug=False, use_reloader=False)
