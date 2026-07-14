from fastapi import FastAPI, File, Form, UploadFile, HTTPException
from pathlib import Path
import hashlib, shutil, datetime

app = FastAPI()
ROOT = Path(r"D:\uploads\logs")

@app.post("/api/factory-tool/logs/upload")
async def upload_log(
    deviceId: str = Form(...),
    station: str = Form(""),
    dateFrom: str = Form(""),
    dateTo: str = Form(""),
    sha256: str = Form(""),
    file: UploadFile = File(...),
):
    save_dir = ROOT / deviceId / datetime.date.today().isoformat()
    save_dir.mkdir(parents=True, exist_ok=True)
    dest = save_dir / file.filename

    with dest.open("wb") as f:
        shutil.copyfileobj(file.file, f)

    digest = hashlib.sha256(dest.read_bytes()).hexdigest()
    if sha256 and digest.lower() != sha256.lower():
        dest.unlink(missing_ok=True)
        raise HTTPException(400, "sha256 mismatch")

    return {"code": 0, "message": "ok", "logUrl": str(dest)}

# 健康检查
@app.get("/health")
def health():
    return {"ok": True}