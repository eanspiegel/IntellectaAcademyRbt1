"""
recorder.py - Creador de Dataset KNN para Lenguaje de Señas
============================================================
Este script permite al usuario grabar sus propias señas
frente a la cámara para generar un archivo 'dataset.json'.
Luego, serverlocal.py usará este archivo para predecir
la seña con alta precisión midiendo distancias (K-Nearest Neighbors).

Instrucciones:
1. Ejecuta python recorder.py
2. Sigue las instrucciones en pantalla para grabar la A, B, C...
3. Presiona 'espacio' para guardar la seña actual.
4. Presiona 'r' para regrabar una letra si te equivocaste.
5. Presiona 'q' para salir en cualquier momento.
"""

import cv2
import mediapipe as mp
import time
import os
import json
import numpy as np

DATASET_FILE = "dataset.json"
MODEL_PATH = os.path.join(os.path.dirname(__file__), "hand_landmarker.task")

# Abecedario incluyendo la Ñ
ALPHABET = [
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", 
    "K", "L", "M", "N", "ENIE", "O", "P", "Q", "R", "S", 
    "T", "U", "V", "W", "X", "Y", "Z"
]

def normalize_landmarks(landmarks):
    """
    Convierte los 21 puntos (x, y, z) de MediaPipe en un 
    arreglo unidimensional de 63 valores.
    Normalizado con respecto a la muñeca (punto 0) para que 
    la posición de la mano en la pantalla no importe.
    """
    base_x = landmarks[0].x
    base_y = landmarks[0].y
    base_z = landmarks[0].z

    norm_vector = []
    for lm in landmarks:
        norm_vector.append(lm.x - base_x)
        norm_vector.append(lm.y - base_y)
        norm_vector.append(lm.z - base_z)
        
    return norm_vector

def main():
    # Cargar dataset existente o crear uno nuevo
    dataset = {}
    if os.path.exists(DATASET_FILE):
        try:
            with open(DATASET_FILE, "r") as f:
                dataset = json.load(f)
            print(f"[+] Dataset pre-existente cargado con {len(dataset)} letras.")
        except Exception as e:
            print(f"[-] Error leyendo {DATASET_FILE}: {e}")

    # Inicializar MediaPipe
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
    if not cap.isOpened():
        print("[ERR] Error al abrir la cámara.")
        return

    current_letter_idx = 0
    
    # Encontrar la primera letra que falta en el abecedario para continuar allí
    for i, letter in enumerate(ALPHABET):
        if letter not in dataset:
            current_letter_idx = i
            break
            
    # Si el abecedario ya esta completo al cargar, preguntar que letra quiere regrabar o arrancar de 0.
    if current_letter_idx == 0 and len(dataset) == len(ALPHABET):
        print("\n¡Ya tienes grabado todo el abecedario!")
        print("Empezaremos desde la A para regrabar. Presiona SPACE si deseas conservar y saltar a la siguiente.")

    print("\n" + "="*50)
    print(" INSTRUCCIONES DE GRABACIÓN LSEC ")
    print("="*50)
    print(" 1. Pon tu mano firme haciendo la seña indicada.")
    print(" 2. Presiona ESPACIO para GUARDAR la letra.")
    print(" 3. Presiona ESPACIO para SALTAR a la siguiente letra (si ya existe).")
    print(" 4. Presiona 'R' para REGRESAR a la letra anterior.")
    print(" 5. Presiona 'Q' para SALIR y GUARDAR EL ARCHIVO.")
    print("="*50 + "\n")

    timestamp_ms = 0

    with HandLandmarker.create_from_options(options) as landmarker:
        while True:
            if current_letter_idx >= len(ALPHABET):
                print("\n[+] ¡Has completado todo el abecedario! Saliendo y guardando...")
                break

            target_letter = ALPHABET[current_letter_idx]

            success, image = cap.read()
            if not success:
                continue

            image = cv2.flip(image, 1)
            h, w, _ = image.shape
            
            image_rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=image_rgb)
            
            timestamp_ms += 33
            result = landmarker.detect_for_video(mp_image, timestamp_ms)

            hand_vector = None
            if result.hand_landmarks:
                for hand_landmarks in result.hand_landmarks:
                    # Rellenar vector numérico
                    hand_vector = normalize_landmarks(hand_landmarks)
                    
                    # Dibujar puntos
                    for lm in hand_landmarks:
                        cx, cy = int(lm.x * w), int(lm.y * h)
                        cv2.circle(image, (cx, cy), 3, (0, 255, 0), -1)

            # UI de Grabación en cámara
            cv2.rectangle(image, (0,0), (640, 80), (0,0,0), -1)
            
            # Estado actual de grabación
            status = "FALTA GRABAR" if target_letter not in dataset else "-- YA GRABADA --"
            color = (0, 0, 255) if target_letter not in dataset else (0, 255, 0)
            
            cv2.putText(image, f"LETRA: {target_letter}", (20, 40), cv2.FONT_HERSHEY_SIMPLEX, 1.2, (255, 255, 255), 2)
            cv2.putText(image, status, (200, 40), cv2.FONT_HERSHEY_SIMPLEX, 1.0, color, 2)
            cv2.putText(image, "ESPACIO=Guardar/Saltar | R=Atras | Q=Salir", (20, 70), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (200, 200, 200), 1)

            if hand_vector is None:
                cv2.putText(image, "[NO SE DETECTA LA MANO]", (150, 400), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)

            cv2.imshow("GRABADORA - DATASET LSEC", image)

            key = cv2.waitKey(1) & 0xFF
            
            # Opciones de teclado
            if key == ord('q'):
                print("\nSaliendo antes de completar...")
                break
                
            elif key == ord('r'):
                if current_letter_idx > 0:
                    current_letter_idx -= 1
                time.sleep(0.3)
                
            elif key == 32: # ESPACIO
                if hand_vector is not None:
                    # Guardamos el vector asociado a esa letra (pisando si ya existía)
                    dataset[target_letter] = hand_vector
                    print(f"[+] GRABADA: {target_letter}")
                    current_letter_idx += 1
                else:
                    # Si no hay mano, y ya estaba grabada, entonces permitimos saltar.
                    if target_letter in dataset:
                        print(f"[*] SALTANDO: {target_letter}")
                        current_letter_idx += 1
                    else:
                        print(f"[-] PON TU MANO FRENTE A LA CAMARA PARA GRABAR {target_letter}.")
                
                time.sleep(0.3)

    # Al salir del bucle, guardar JSON
    try:
        with open(DATASET_FILE, "w") as f:
            json.dump(dataset, f, indent=4)
        print(f"\n[OK] Dataset guardado exitosamente en {DATASET_FILE}. Total: {len(dataset)} letras.")
    except Exception as e:
        print(f"\n[ERR] Error al guardar dataset.json: {e}")

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
