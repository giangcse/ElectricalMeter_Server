from roboflow import Roboflow
from PIL import Image
from vietocr.tool.predictor import Predictor
from vietocr.tool.config import Cfg

import cv2
import numpy as np

rf = Roboflow(api_key="M79YvxlMJMm44JdTfBZ7")
project = rf.workspace().project("mywatermeter")
model = project.version(1).model

# VietOCR config
config = Cfg.load_config_from_name('vgg_seq2seq')
# config['weights'] = os.path.join(os.getcwd(), 'models/vgg_seq2seq.pth')
config['cnn']['pretrained']=False
config['device'] = 'cpu'
detector = Predictor(config)

def read_meter(image_path: str) -> str:
    # infer on a local image
    response = model.predict(f"{image_path}", confidence=40, overlap=30).json()
    # extract coor. values from response
    w: int = round(response['predictions'][0]['width'])
    h: int = round(response['predictions'][0]['height'])
    x: int = round(response['predictions'][0]['x'] - (w / 2))
    y: int = round(response['predictions'][0]['y'] - (h / 2))
    
    image = cv2.imread(image_path)
    cropped_img = image[y : y+h, x : x+w]
    
    # pre_img = Image.fromarray(cropped_img)
    # Chuyển ảnh sang grayscale
    gray_image = cv2.cvtColor(cropped_img, cv2.COLOR_BGR2GRAY)
    # Áp dụng ngưỡng (thresholding) để tách nền
    _, thresholded_image = cv2.threshold(gray_image, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)

    # Loại bỏ các đối tượng nhỏ bằng cách tìm contours và xác định kích thước
    contours, _ = cv2.findContours(thresholded_image, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    min_area = 0.5  # Điều chỉnh kích thước tối thiểu của đối tượng để loại bỏ

    # Tạo ảnh mask để loại bỏ đối tượneg nhỏ
    mask = np.ones_like(thresholded_image, dtype=np.uint8) * 255
    for contour in contours:
        if cv2.contourArea(contour) < min_area:
            cv2.drawContours(mask, [contour], -1, 0, thickness=cv2.FILLED)

    # Áp dụng mask để loại bỏ đối tượng nhỏ trên ảnh đã tách nền
    result_image = cv2.bitwise_and(thresholded_image, thresholded_image, mask=mask)
    digits = detector.predict(Image.fromarray(result_image))
    return str(digits)

# if __name__=='__main__':
#     result = read_meter("dongho.jpg")