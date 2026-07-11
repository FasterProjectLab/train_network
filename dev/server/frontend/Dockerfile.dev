FROM node:20-alpine

WORKDIR /appli

# Installation des dépendances
COPY src/package*.json ./
RUN npm install

# Copie du reste du code
COPY src/. .

EXPOSE 3000

# Lancement du serveur de dev Vite ou Create-React-App
CMD ["npm", "start"]