"""
servidor.py - Servidor central para multiples ESP32-CAM
=======================================================
Endpoints:
  POST /register  -> Recibe {name, ip} de cada ESP32-CAM
  GET  /cameras   -> Devuelve lista de camaras activas (JSON)
  GET  /          -> Dashboard web

Uso:
  pip install flask
  python server.py
"""

from flask import Flask, request, jsonify, render_template
import threading
import time
import logging

app = Flask(__name__)
logging.basicConfig(level=logging.INFO, format="[%(asctime)s] %(message)s", datefmt="%H:%M:%S")

# Registro de camaras: { name -> {ip, last_seen} }
cameras = {}
cameras_lock = threading.Lock()

# Tiempo maximo sin actividad para remover una camara (segundos)
CAMERA_TIMEOUT = 3000


# ------------------------------------------------------------------ #
#  POST /register
# ------------------------------------------------------------------ #
@app.route("/register", methods=["POST"])
def register():
    data = request.get_json(force=True, silent=True)
    if not data or "name" not in data or "ip" not in data:
        return jsonify({"error": "Se requiere 'name' e 'ip'"}), 400

    name = data["name"].strip()
    ip   = data["ip"].strip()

    with cameras_lock:
        is_new = name not in cameras
        cameras[name] = {"ip": ip, "last_seen": time.time()}

    if is_new:
        logging.info(f"Nueva camara registrada: {name} ({ip})")
    else:
        logging.info(f"Heartbeat de camara: {name} ({ip})")

    return jsonify({"status": "ok", "name": name}), 200


# ------------------------------------------------------------------ #
#  GET /cameras
# ------------------------------------------------------------------ #
@app.route("/cameras", methods=["GET"])
def get_cameras():
    with cameras_lock:
        result = [
            {"name": name, "ip": info["ip"]}
            for name, info in cameras.items()
        ]
    return jsonify(result), 200


# ------------------------------------------------------------------ #
#  GET /  ->  Dashboard
# ------------------------------------------------------------------ #
@app.route("/")
def index():
    return render_template("index.html")


# ------------------------------------------------------------------ #
#  Hilo de limpieza: elimina camaras inactivas
# ------------------------------------------------------------------ #
def cleanup_thread():
    while True:
        time.sleep(10)
        now = time.time()
        with cameras_lock:
            inactive = [n for n, info in cameras.items()
                        if now - info["last_seen"] > CAMERA_TIMEOUT]
            for name in inactive:
                logging.info(f"Camara eliminada por inactividad: {name}")
                del cameras[name]


if __name__ == "__main__":
    t = threading.Thread(target=cleanup_thread, daemon=True)
    t.start()

    logging.info("Servidor iniciado en http://0.0.0.0:5000")
    logging.info("Abre http://localhost:5000 en tu navegador")
    app.run(host="0.0.0.0", port=5000, debug=False, threaded=True)
