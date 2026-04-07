"""
server.py - Deteccion de gestos multi-dispositivo (ESP32-CAM)
=============================================================
Cada ESP32-CAM se registra con un nombre unico. El servidor
procesa la camara de cada uno en un hilo separado con MediaPipe
y guarda el gesto de forma individual.

Endpoints:
  POST /register          -> ESP32 envia {name, ip, port}
  GET  /status/<name>     -> Gesto del dispositivo: {gesture}
  GET  /status            -> Todos los gestos
  GET  /cam_status        -> Lista de camaras registradas
  GET  /                  -> Dashboard web

Uso:
  pip install flask opencv-python mediapipe numpy
  python server.py
"""

import cv2
import mediapipe as mp
import threading
import time
import logging
import os
import urllib.request

from flask import Flask, Response, jsonify, request, render_template

# ------------------------------------------------------------------ #
#  Configuracion
# ------------------------------------------------------------------ #
app = Flask(__name__)
logging.basicConfig(
    level=logging.INFO,
    format="[%(asctime)s] %(message)s",
    datefmt="%H:%M:%S"
)

# ---- Modelo MediaPipe ----
MODEL_PATH = os.path.join(os.path.dirname(__file__), "hand_landmarker.task")
MODEL_URL  = (
    "https://storage.googleapis.com/mediapipe-models/"
    "hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task"
)
if not os.path.exists(MODEL_PATH):
    logging.info("Descargando modelo hand_landmarker.task ...")
    urllib.request.urlretrieve(MODEL_URL, MODEL_PATH)
    logging.info("Modelo descargado.")

# ---- Registro de dispositivos ----
devices      = {}
devices_lock = threading.Lock()

DEVICE_TIMEOUT = 60   # segundos sin heartbeat para eliminar dispositivo

# ------------------------------------------------------------------ #
#  Deteccion de gestos con MediaPipe
# ------------------------------------------------------------------ #
DATASET_FILE = os.path.join(os.path.dirname(__file__), "dataset.json")

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
    if not dataset:
        return "ninguno"

    best_letter = "ninguno"
    min_dist = float('inf')
    
    import numpy as np
    cv = np.array(current_vector)

    for letter, saved_vector in dataset.items():
        sv = np.array(saved_vector)
        dist = np.linalg.norm(cv - sv)
        
        if dist < min_dist:
            min_dist = dist
            best_letter = letter

    # Threshold empirico de LSEC (ajustar si es necesario)
    if min_dist > 1.2: 
        return "ninguno"
        
    return best_letter


def make_placeholder(msg="Sin señal"):
    import numpy as np
    img = np.zeros((240, 320, 3), dtype="uint8")
    (tw, _), _ = cv2.getTextSize(msg, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1)
    x = max(0, (320 - tw) // 2)
    cv2.putText(img, msg, (x, 125),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (80, 80, 80), 1, cv2.LINE_AA)
    return img


# ------------------------------------------------------------------ #
#  Hilo de procesamiento por camara
# ------------------------------------------------------------------ #

def _device_thread(name: str, stop_event: threading.Event):
    BaseOptions           = mp.tasks.BaseOptions
    HandLandmarker        = mp.tasks.vision.HandLandmarker
    HandLandmarkerOptions = mp.tasks.vision.HandLandmarkerOptions
    VisionRunningMode     = mp.tasks.vision.RunningMode

    options = HandLandmarkerOptions(
        base_options=BaseOptions(model_asset_path=MODEL_PATH),
        running_mode=VisionRunningMode.VIDEO,
        num_hands=1,
        min_hand_detection_confidence=0.6,
        min_hand_presence_confidence=0.6,
        min_tracking_confidence=0.5,
    )

    with devices_lock:
        if name in devices:
            devices[name]["frame"] = make_placeholder(f"{name}: conectando...")

    with HandLandmarker.create_from_options(options) as landmarker:
        stream       = None
        bytes_data   = b''
        active_key   = None
        timestamp_ms = 0
        
        # Cargar Dataset local para este hilo
        dev_dataset = {}
        if os.path.exists(DATASET_FILE):
            import json
            try:
                with open(DATASET_FILE, "r") as f:
                    dev_dataset = json.load(f)
            except:
                pass

        while not stop_event.is_set():
            with devices_lock:
                dev = devices.get(name)
                if dev is None:
                    break
                ip   = dev["ip"]
                port = dev["port"]

            wanted_key = f"{ip}:{port}"

            if wanted_key != active_key or stream is None:
                if stream:
                    try: stream.release()
                    except: pass
                    stream = None
                active_key = None

                stream_url = f"http://{ip}:{port}/stream"
                try:
                    stream = cv2.VideoCapture(stream_url)
                    stream.set(cv2.CAP_PROP_BUFFERSIZE, 1) # Minimizar lag
                    if not stream.isOpened():
                        raise Exception("No se pudo abrir VideoCapture")
                        
                    active_key = wanted_key
                    logging.info(f"[{name}] Stream abierto: {stream_url}")
                except Exception as e:
                    logging.warning(f"[{name}] Error al conectar a la camara: {e}")
                    with devices_lock:
                        if name in devices:
                            devices[name]["frame"] = make_placeholder(f"{name}: esperando camara...")
                    time.sleep(2)
                    continue

            try:
                ret, frame = stream.read()
                if not ret or frame is None:
                    logging.warning(f"[{name}] Stream vacio. Reiniciando conexion...")
                    raise Exception("Stream vacio")
            except Exception as e:
                # Si falla, cerramos y forzamos re-conectar
                if stream:
                    try: stream.release()
                    except: pass
                    stream = None
                active_key = None
                time.sleep(1)
                continue

            h, w, _ = frame.shape
            timestamp_ms += 33

            mp_image = mp.Image(
                image_format=mp.ImageFormat.SRGB,
                data=cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            )
            result = landmarker.detect_for_video(mp_image, timestamp_ms)

            gesture = "ninguno"
            if result.hand_landmarks:
                for hand_landmarks in result.hand_landmarks:
                    # Dibujar puntos en pantalla para visualizacion local de lo que ve el ESP32
                    for lm in hand_landmarks:
                        cx = int(lm.x * w)
                        cy = int(lm.y * h)
                        cv2.circle(frame, (cx, cy), 3, (0, 255, 0), -1)
                        
                    # 1. Normalizar Mano
                    vector = normalize_landmarks(hand_landmarks)
                    # 2. Predecir con Machine Learning (KNN)
                    gesture = knn_predict(vector, dev_dataset)

            # Dibujar la letra y preparar el frame
            cv2.putText(frame, f"ESP32 - Letra: {gesture}", (10, 30), 
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 0, 0), 2)

            with devices_lock:
                if name in devices:
                    devices[name]["frame"] = frame.copy()
                    prev = devices[name]["gesture"]
                    if gesture != prev and gesture != "ninguno":
                        logging.info(f"[{name}] Letra: {gesture}")
                    devices[name]["gesture"] = gesture

        if stream:
            try: stream.release()
            except: pass

    logging.info(f"[{name}] Hilo terminado.")


def _ensure_thread(name: str):
    dev = devices[name]
    t   = dev.get("thread")
    if t is not None and t.is_alive():
        return
    stop_ev          = threading.Event()
    dev["stop_event"] = stop_ev
    t = threading.Thread(target=_device_thread, args=(name, stop_ev), daemon=True)
    dev["thread"] = t
    t.start()


def _stop_thread(name: str):
    dev = devices.get(name)
    if dev:
        ev = dev.get("stop_event")
        if ev:
            ev.set()


# ------------------------------------------------------------------ #
#  Rutas Flask
# ------------------------------------------------------------------ #

@app.route("/register", methods=["POST"])
def register():
    data = request.get_json(force=True, silent=True)
    if not data or "name" not in data or "ip" not in data:
        return jsonify({"error": "Se requiere 'name' e 'ip'"}), 400

    name = data["name"].strip()
    ip   = data["ip"].strip()
    port = int(data.get("port", 80))

    with devices_lock:
        is_new = name not in devices
        if is_new:
            devices[name] = {
                "ip": ip, "port": port,
                "gesture": "ninguno",
                "frame": None,
                "last_seen": time.time(),
                "thread": None,
                "stop_event": None,
            }
            logging.info(f"Nuevo dispositivo: {name} ({ip}:{port})")
        else:
            devices[name]["ip"]        = ip
            devices[name]["port"]      = port
            devices[name]["last_seen"] = time.time()
        _ensure_thread(name)

    return jsonify({"status": "ok", "name": name}), 200


@app.route("/status/<name>", methods=["GET"])
def status_by_name(name):
    with devices_lock:
        dev = devices.get(name)
    if dev is None:
        return jsonify({"error": "dispositivo no encontrado"}), 404
    return jsonify({"name": name, "gesture": dev["gesture"]}), 200


@app.route("/status", methods=["GET"])
def status_all():
    with devices_lock:
        result = {n: d["gesture"] for n, d in devices.items()}
    return jsonify(result), 200


@app.route("/cam_status", methods=["GET"])
def cam_status():
    with devices_lock:
        result = [
            {"name": n, "ip": d["ip"], "port": d["port"], "gesture": d["gesture"]}
            for n, d in devices.items()
        ]
    return jsonify(result), 200


@app.route("/")
def index():
    return render_template("index.html")


# ------------------------------------------------------------------ #
#  Limpieza de dispositivos inactivos
# ------------------------------------------------------------------ #
def cleanup_thread():
    while True:
        time.sleep(15)
        now = time.time()
        with devices_lock:
            inactive = [n for n, d in devices.items()
                        if now - d["last_seen"] > DEVICE_TIMEOUT]
            for name in inactive:
                logging.info(f"Dispositivo inactivo eliminado: {name}")
                _stop_thread(name)
                del devices[name]


# ------------------------------------------------------------------ #
#  Bucle principal GUI
# ------------------------------------------------------------------ #
def main_gui_loop():
    active_windows = set()
    while True:
        frames_to_show = []
        current_devices = set()
        
        with devices_lock:
            current_devices = set(devices.keys())
            for name, dev in devices.items():
                if dev.get("frame") is not None:
                    frames_to_show.append((name, dev["frame"]))
        
        # Cerrar ventanas de los dispositivos inactivos/eliminados
        for name in list(active_windows):
            if name not in current_devices:
                try: cv2.destroyWindow(f"Camara ESP32: {name}")
                except: pass
                active_windows.remove(name)
                
        if not frames_to_show:
            time.sleep(0.05)
            continue
            
        for name, frame in frames_to_show:
            win_name = f"Camara ESP32: {name}"
            try:
                cv2.imshow(win_name, frame)
                active_windows.add(name)
            except Exception:
                pass
                
        # Un solo waitKey por iteración en el hilo principal
        if cv2.waitKey(1) == 27: # ESC para salir
            break

# ------------------------------------------------------------------ #
#  Arranque
# ------------------------------------------------------------------ #
if __name__ == "__main__":
    t = threading.Thread(target=cleanup_thread, daemon=True)
    t.start()
    
    # Ejecutar Flask en un hilo separado
    flask_thread = threading.Thread(
        target=app.run, 
        kwargs={"host":"0.0.0.0", "port":5001, "debug":False, "use_reloader":False, "threaded":True},
        daemon=True
    )
    flask_thread.start()
    
    logging.info("Servidor iniciado en http://0.0.0.0:5001")
    logging.info("Dashboard: http://localhost:5001")
    logging.info("Cada ESP32 llama: POST /register  y  GET /status/<nombre>")
    
    # El hilo principal se encarga de las ventanas de OpenCV
    main_gui_loop()

