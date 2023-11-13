FROM python3.11

WORKDIR /app

COPY . /app

RUN pip install --no-cache-dir --upgrade pip && \
    pip install -r requirements.txt

EXPOSE 10000

CMD [ "uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "10000", "--worker", "2" ]