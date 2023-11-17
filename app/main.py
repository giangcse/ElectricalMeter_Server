from fastapi import FastAPI, UploadFile, File
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware
from pathlib import Path
from dotenv import load_dotenv
from pydantic import BaseModel
from PIL import Image
from io import BytesIO
from app.reader import read_meter

import pymongo
import os
import shutil
import datetime
import base64
import random

# Load .env
dotenv_path = Path('.env')
load_dotenv(dotenv_path=dotenv_path)
# MongoDB variables
MONGODB_SERVER = os.getenv('MONGODB_SERVER')
MONGODB_USERNAME = os.getenv('MONGODB_USERNAME')
MONGODB_PASSWORD = os.getenv('MONGODB_PASSWORD')
# MQTT variables
MQTT_BROKER = os.getenv('MQTT_BROKER')
MQTT_PORT = os.getenv('MQTT_PORT')
MQTT_USERNAME = os.getenv('MQTT_USERNAME')
MQTT_PASSWORD = os.getenv('MQTT_PASSWORD')

# Create a FastAPI app
app = FastAPI(title="Electrical Meter Server")
app.add_middleware(
    CORSMiddleware,
    allow_origins=['*'],
    allow_credentials=True,
    allow_methods=['*'],
    allow_headers=['*']
)

# Connect to MongoDB Atlas
client = pymongo.MongoClient(f"mongodb+srv://{MONGODB_USERNAME}:{MONGODB_PASSWORD}@{MONGODB_SERVER}/?retryWrites=true&w=majority")
db = client['ElectricalMeter']
e_log = db['ChiSoDien']
# Model for upload
class UploadModel(BaseModel):
    mac: str
    ip: str
    image: str

def decode_base64_to_image(base64_string, output_file_path):
    # Loại bỏ tiền tố 'data:image/png;base64,' từ base64 string (nếu có)
    base64_string = base64_string.split(",")[-1]

    # Giải mã base64 thành dữ liệu bytes
    image_data = base64.b64decode(base64_string)

    # Đọc hình ảnh từ dữ liệu bytes
    image = Image.open(BytesIO(image_data))

    # Lưu hình ảnh vào đường dẫn được chỉ định
    image.save(output_file_path)

# Upload endpoint
@app.post('/upload')
async def upload(file: UploadFile = File(...)):
    file_path = os.path.join('upload_folder', file.filename)
    with open(file_path, 'wb') as f:
        shutil.copyfileobj(file.file, f)
    
    with open(file_path, 'rb') as f:
        image_encode = base64.b64encode(f.read())
    save_to_db = e_log.insert_one({'image': image_encode})
    if save_to_db.acknowledged:
        os.remove(file_path)
        return JSONResponse(status_code=200, content="Uploaded")
    else:
        return JSONResponse(status_code=500, content="Database server error")
    
# Upload endpoint
@app.post('/upload_base64')
async def upload(upload: UploadModel):
    # Save base64 to image
    file_path = os.path.join('upload_folder', f'image_{random.randint(0, 999)}.jpg')

    decode_base64_to_image(upload.image, file_path)

    extracted_digits = read_meter(file_path)

    save_to_db = e_log.insert_one({'mac': upload.mac, 'ip': upload.ip, 'image': upload.image, 'raw_value': extracted_digits, 'createdAt': round(datetime.datetime.now().timestamp())})
    if save_to_db.acknowledged:
        os.remove(file_path)
        return JSONResponse(status_code=200, content="Uploaded")
    else:
        return JSONResponse(status_code=500, content="Database server error")