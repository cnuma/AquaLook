FROM python:3.12-slim

WORKDIR /app

# Dependances d'abord : cette couche est mise en cache et n'est reconstruite
# que lorsque requirements.txt change.
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app ./app

# Utilisateur non privilegie : un service expose ne tourne pas en root.
RUN useradd --create-home --uid 10001 aqualook
USER aqualook

EXPOSE 8000
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
