FROM python:3.11-slim
WORKDIR /app
RUN pip install websockets
COPY client_test.py .
CMD ["python", "client_test.py"]