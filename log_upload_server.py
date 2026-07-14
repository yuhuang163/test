#!/usr/bin/env python3
from flask import Flask, request, jsonify
import os

app = Flask(__name__)

UPLOAD_FOLDER = './uploaded_logs'
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

@app.route("/api/factory-tool/logs/upload", methods=["POST"])
def upload_logs():
    """模拟 FastAPI 服务的 log 文件上传接口"""
    if 'file' not in request.files:
        return jsonify({"code": 1, "message": "No file part"}), 400
    
    file = request.files['file']
    
    if file.filename == '':
        return jsonify({"code": 1, "message": "No selected file"}), 400
    
    if file:
        filename = file.filename
        filepath = os.path.join(UPLOAD_FOLDER, filename)
        file.save(filepath)
        
        file_size = os.path.getsize(filepath)
        
        print(f"[INFO] Log file uploaded: {filename} ({file_size} bytes)")
        
        return jsonify({
            "code": 0,
            "message": "Log file uploaded successfully",
            "filename": filename,
            "size": file_size
        })

if __name__ == "__main__":
    print("Log Upload Server (FastAPI Mock) starting...")
    print(f"Upload folder: {os.path.abspath(UPLOAD_FOLDER)}")
    print("Listening on http://0.0.0.0:8889")
    app.run(host='0.0.0.0', port=8889, debug=False)
