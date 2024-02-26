from PIL import Image
from vietocr.tool.predictor import Predictor
from vietocr.tool.config import Cfg

import cv2
import numpy as np
import os

from ultralytics import YOLO
from easyocr import Reader
from pathlib import Path
from dotenv import load_dotenv
# Load .env
dotenv_path = Path('.env')
load_dotenv(dotenv_path=dotenv_path)
YOLO_MODEL=os.getenv('YOLO_MODEL')
OCR_MODEL=os.getenv('OCR_MODEL')

# YOLO MODEL
model = YOLO(model=YOLO_MODEL)

# VietOCR config
config = Cfg.load_config_from_name('vgg_seq2seq')
config['weights'] = os.path.join(os.getcwd(), OCR_MODEL)
config['cnn']['pretrained']=False
config['device'] = 'cuda:0'
detector = Predictor(config)

# EasyOCR
reader = Reader(lang_list=['vi'], gpu=True)

def read_meter(image_path: str) -> str:
    im1 = cv2.imread(image_path)
    results = model.predict(source=im1, conf=0.5)
    for result in results:
        top_left_x = int(result.boxes.xyxy[0][0])
        top_left_y = int(result.boxes.xyxy[0][1])
        bottom_right_x = int(result.boxes.xyxy[0][2])
        bottom_right_y = int(result.boxes.xyxy[0][3])
        cropped_img = im1[top_left_y:bottom_right_y, top_left_x:bottom_right_x]
        # Chuyển ảnh sang grayscale
        gray_image = cv2.cvtColor(cropped_img, cv2.COLOR_BGR2GRAY)
        # Áp dụng ngưỡng (thresholding) để tách nền
        _, thresholded_image = cv2.threshold(gray_image, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)

        # Loại bỏ các đối tượng nhỏ bằng cách tìm contours và xác định kích thước
        contours, _ = cv2.findContours(thresholded_image, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        min_area = 85  # Điều chỉnh kích thước tối thiểu của đối tượng để loại bỏ

        # Tạo ảnh mask để loại bỏ đối tượneg nhỏ
        mask = np.ones_like(thresholded_image, dtype=np.uint8) * 255
        for contour in contours:
            if cv2.contourArea(contour) < min_area:
                cv2.drawContours(mask, [contour], -1, 0, thickness=cv2.FILLED)

        # Áp dụng mask để loại bỏ đối tượng nhỏ trên ảnh đã tách nền
        result_image = cv2.bitwise_and(thresholded_image, thresholded_image, mask=mask)
        text = reader.readtext(result_image)
        result = text[0][1].replace(' ', '')
        digits = ''.join([x for x in result if x.isdigit()])
        return str(digits)

# if __name__=='__main__':
#     result = read_meter("IMG_5971.jpeg")
#     print(result)