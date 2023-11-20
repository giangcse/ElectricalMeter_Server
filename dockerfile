FROM python:3.9

WORKDIR /app

COPY . /app

RUN apt-get update && apt-get install ffmpeg libsm6 libxext6  -y

RUN pip install --no-cache-dir --upgrade pip && \
    pip install -r requirements.txt

EXPOSE 10000

CMD [ "uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "10000", "--workers", "2" ]